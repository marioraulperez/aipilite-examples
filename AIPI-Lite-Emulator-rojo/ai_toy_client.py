"""
Cliente de ejemplo para el protocolo tipo "AI toy" (MQTT + UDP/Opus/AES-128-CTR)
observado en la captura de Wireshark.

Arquitectura detectada:
  1. El dispositivo se conecta a un broker MQTT (152.32.151.73:8883, TLS).
  2. Publica un "hello" en el topic  xorigin/device-server/<SERIAL>
  3. El servidor responde por el topic forwards/p2p/<SERIAL> con un JSON:
       {
         "type": "hello",
         "session_id": "...",
         "transport": "udp",
         "udp": {"server": "...", "port": 8884,
                 "encryption": "aes-128-ctr",
                 "key": "<hex 16 bytes>",
                 "nonce": "<hex 16 bytes>"},
         "audio_params": {"format":"opus","sample_rate":24000,
                           "channels":1,"frame_duration":60}
       }
  4. A partir de ahí, audio Opus cifrado con AES-128-CTR viaja por UDP
     en ambas direcciones al server:port indicados.

IMPORTANTE:
- La key/nonce son por sesión: hay que hacer el handshake cada vez, no se
  pueden hardcodear los valores de una captura vieja.
- El formato del paquete UDP está tomado de la doc oficial del proyecto
  xiaozhi-esp32 (github.com/78/xiaozhi-esp32/blob/main/docs/mqtt-udp.md),
  que es la arquitectura que este dispositivo parece implementar:
      |type 1B|flags 1B|payload_len 2B|ssrc 4B|timestamp 4B|sequence 4B|
      |payload cifrado, payload_len bytes|
  Ese header de 16 bytes va sin cifrar como prefijo del datagrama, y ese
  mismo header se usa como contador/IV de AES-CTR. type/flags/ssrc salen
  del "nonce" recibido en el hello MQTT; payload_len/timestamp/sequence
  se arman por paquete.
- CONFIRMADO contra una captura real completa (Follow UDP Stream):
  el header de 16 bytes es exactamente
      type(1B)=0x01 flags(1B)=0x00 payload_len(2B, big-endian)
      ssrc(4B, = nonce recibido en el hello) timestamp(4B, queda en 0
      toda la sesión) sequence(4B, big-endian, arranca en 1 e
      incrementa de a 1 por paquete, contador independiente por
      dirección).
  payload_len coincide exactamente con el tamaño real de cada trozo
  de audio Opus cifrado que sigue al header.
- Las credenciales MQTT (usuario/password/client_id) normalmente las
  entrega el servidor en un paso previo de "check-in" (HTTP/OTA), no
  vienen en el JSON del hello. Buscá ese tráfico en tu captura si el
  broker te rechaza la conexión.

Dependencias:
    pip install paho-mqtt pycryptodome opuslib sounddevice numpy
    (opuslib necesita libopus instalado en el sistema: apt install libopus0 libopus-dev)
"""

import json
import socket
import struct
import threading
import time

import numpy as np
import paho.mqtt.client as mqtt
import sounddevice as sd
from Crypto.Cipher import AES
import opuslib

# ---------------------------------------------------------------------------
# Configuración (ajustar con los datos reales de tu dispositivo)
# ---------------------------------------------------------------------------
MQTT_HOST = "152.32.151.73"
MQTT_PORT = 1883                      # confirmado por captura: MQTT en texto plano, NO TLS
                                        # (el TLS visto antes en :443 es otra conexión, probablemente
                                        # el check-in HTTP/OTA previo, no el canal MQTT)
DEVICE_SERIAL = "XY006PL01USA0428029"
TOPIC_PUBLISH = f"xorigin/device-server/{DEVICE_SERIAL}"   # cliente -> servidor
TOPIC_SUBSCRIBE = f"forwards/p2p/{DEVICE_SERIAL}"           # servidor -> cliente

# Credenciales confirmadas en el CONNECT capturado (frame 95):
#   Client ID = User Name = serial del dispositivo
#   Password  = 32 hex chars (pinta de MD5, probablemente derivado de
#               serial + secreto de firmware). Para ESTE dispositivo ya
#               la tenés; para otro dispositivo necesitarías capturar su
#               propio CONNECT o descifrar el algoritmo de derivación.
#
# OJO: usar este client_id conecta como si fueras el dispositivo real.
# MQTT normalmente permite una sola sesión activa por client_id, así que
# esto puede desconectar al dispositivo físico si está en uso.
MQTT_USERNAME = DEVICE_SERIAL
MQTT_PASSWORD = "97a9c6b5032b8404c51a9adbb89513a6"
MQTT_USE_TLS = False

SAMPLE_RATE = 24000
CHANNELS = 1
FRAME_MS = 60
FRAME_SAMPLES = SAMPLE_RATE * FRAME_MS // 1000  # 1440 samples @ 24kHz/60ms

# Índices de dispositivo de audio (ver salida de:
#   python -c "import sounddevice as sd; print(sd.query_devices())"
# ). Dejar en None para usar el default del sistema (puede fallar en
# Windows si no hay uno bien configurado -> ver INPUT_DEVICE/OUTPUT_DEVICE).
INPUT_DEVICE = None   # ej: 0  (micrófono)
OUTPUT_DEVICE = None  # ej: 1  (parlantes/auriculares)


class SessionInfo:
    def __init__(self):
        self.session_id = None
        self.udp_host = None
        self.udp_port = None
        self.key = None
        self.nonce_base = None
        self.ready = threading.Event()
        self.ptt_active = threading.Event()   # True = usuario hablando
        self.mqtt_client = None               # referencia al cliente MQTT


session = SessionInfo()


# ---------------------------------------------------------------------------
# MQTT: login + negociación de sesión
# ---------------------------------------------------------------------------
def on_connect(client, userdata, flags, rc, properties=None):
    print("[mqtt] conectado, rc =", rc)
    session.mqtt_client = client
    client.subscribe(TOPIC_SUBSCRIBE)

    hello_msg = {
        "type": "hello",
        "version": 3,
        "transport": "udp",
        "features": {
            "mcp": True,
            "aec": False,
        },
        "audio_params": {
            "format": "opus",
            "sample_rate": SAMPLE_RATE,
            "channels": CHANNELS,
            "frame_duration": FRAME_MS,
        },
    }
    client.publish(TOPIC_PUBLISH, json.dumps(hello_msg))


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        print("[mqtt] mensaje no-JSON en", msg.topic, "(probablemente binario, ignorado)")
        return

    print("[mqtt] mensaje en", msg.topic, ":", payload)

    if payload.get("type") == "hello" and "udp" in payload:
        udp_cfg = payload["udp"]
        session.session_id = payload.get("session_id")
        session.udp_host = udp_cfg["server"]
        session.udp_port = udp_cfg["port"]
        session.key = bytes.fromhex(udp_cfg["key"])
        session.nonce_base = bytes.fromhex(udp_cfg["nonce"])
        session.ready.set()
        print(f"[mqtt] sesión {session.session_id} lista -> "
              f"{session.udp_host}:{session.udp_port}")

    elif payload.get("type") == "tts":
        state = payload.get("state", "?")
        print(f"[mqtt] servidor TTS: estado={state}")

    elif payload.get("type") == "goodbye":
        print("[mqtt] servidor cerró la sesión (goodbye)")


def start_mqtt():
    client = mqtt.Client(client_id=DEVICE_SERIAL, protocol=mqtt.MQTTv311)
    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    if MQTT_USE_TLS:
        client.tls_set()  # ajustar ca_certs/certfile si el server pide cert de cliente
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=30)
    client.loop_start()
    return client


# ---------------------------------------------------------------------------
# Cifrado AES-128-CTR del canal UDP
#
# Formato confirmado por la doc oficial (github.com/78/xiaozhi-esp32,
# docs/mqtt-udp.md):
#
#   |type 1B|flags 1B|payload_len 2B|ssrc 4B|timestamp 4B|sequence 4B|
#   |payload (payload_len bytes, cifrado con AES-128-CTR)|
#
# Ese header de 16 bytes (sin cifrar, va como prefijo del datagrama UDP)
# es a la vez el contador/IV de AES-CTR para ese paquete: type/flags/ssrc
# se toman del "nonce" que llegó en el hello por MQTT, payload_len se
# calcula por paquete, y timestamp/sequence se incrementan.
#
# OJO: el endianness (big vs little endian) de payload_len/ssrc/timestamp/
# sequence no está 100% confirmado acá -> lo dejo en big-endian ('>') por
# ser lo más común en protocolos de red, pero conviene validarlo contra
# los bytes crudos de tus paquetes 138-140.
# ---------------------------------------------------------------------------
HEADER_FMT = ">BBHIII"  # type, flags, payload_len, ssrc, timestamp, sequence
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # 16 bytes


def parse_nonce_template(nonce_hex_bytes: bytes):
    """Extrae type/flags/ssrc del nonce recibido en el hello MQTT."""
    type_, flags, _payload_len, ssrc, _ts, _seq = struct.unpack(
        HEADER_FMT, nonce_hex_bytes
    )
    return type_, flags, ssrc


def build_header(type_, flags, ssrc, payload_len, timestamp, sequence) -> bytes:
    return struct.pack(HEADER_FMT, type_, flags, payload_len, ssrc, timestamp, sequence)


def make_cipher(key: bytes, header: bytes):
    # El header de 16 bytes se usa directo como initial_value del contador CTR.
    ctr = int.from_bytes(header, byteorder="big")
    return AES.new(key, AES.MODE_CTR, initial_value=ctr, nonce=b"")


# ---------------------------------------------------------------------------
# Envío: mic -> Opus -> AES -> UDP
# ---------------------------------------------------------------------------
def sender_loop(sock: socket.socket, dest, encoder, stop_evt: threading.Event):
    type_, flags, ssrc = parse_nonce_template(session.nonce_base)
    # Confirmado en captura real: sequence arranca en 1 (no en 0) y
    # timestamp se queda fijo en 0 durante toda la sesión.
    seq = 1
    timestamp = 0
    mic_frame = 0   # contador para el medidor de nivel

    def callback(indata, frames, time_info, status):
        nonlocal seq, mic_frame
        if not session.ptt_active.is_set():
            return
        if status:
            print("[audio-in]", status)

        # Medidor de nivel RMS en línea (cada 5 frames ≈ 300 ms)
        mic_frame += 1
        if mic_frame % 5 == 0:
            rms = float(np.sqrt(np.mean(indata[:, 0] ** 2)))
            db = 20 * np.log10(rms + 1e-9)
            filled = int(max(0, min(24, (db + 72) / 3)))
            bar = "█" * filled + "░" * (24 - filled)
            print(f"\r[mic] {bar} {db:+5.1f} dB", end="", flush=True)

        pcm16 = (indata[:, 0] * 32767).astype(np.int16).tobytes()
        try:
            encoded = encoder.encode(pcm16, FRAME_SAMPLES)
        except Exception as e:
            print(f"\n[opus-encode] ERROR: {e!r}")
            return

        header = build_header(type_, flags, ssrc, len(encoded), timestamp, seq)
        cipher = make_cipher(session.key, header)
        ciphertext = cipher.encrypt(encoded)

        try:
            sock.sendto(header + ciphertext, dest)
        except Exception as e:
            print(f"\n[udp-send] ERROR: {e!r}")
            return

        if seq <= 3 or seq % 100 == 0:
            print(f"\n[udp-send] seq={seq} opus_bytes={len(encoded)} -> {dest}")
        seq += 1

    with sd.InputStream(samplerate=SAMPLE_RATE, channels=CHANNELS,
                         blocksize=FRAME_SAMPLES, dtype="float32",
                         device=INPUT_DEVICE,
                         callback=callback):
        print("[main] micrófono abierto, capturando audio...")
        while not stop_evt.is_set():
            time.sleep(0.1)


# ---------------------------------------------------------------------------
# Recepción: UDP -> AES -> Opus -> speaker
# ---------------------------------------------------------------------------
def receiver_loop(sock: socket.socket, decoder, stop_evt: threading.Event):
    out_stream = sd.OutputStream(samplerate=SAMPLE_RATE, channels=CHANNELS,
                                  dtype="int16", device=OUTPUT_DEVICE)
    out_stream.start()
    sock.settimeout(1.0)
    print("[main] esperando paquetes UDP de audio del servidor...")

    recv_count = 0
    last_log = time.monotonic()

    while not stop_evt.is_set():
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            if time.monotonic() - last_log > 5:
                print(f"[udp-recv] sin paquetes en los últimos 5s "
                      f"(total recibidos hasta ahora: {recv_count})")
                last_log = time.monotonic()
            continue

        if len(data) < HEADER_SIZE:
            print(f"[udp-recv] paquete demasiado chico ({len(data)} bytes), descartado")
            continue

        header, payload = data[:HEADER_SIZE], data[HEADER_SIZE:]
        recv_count += 1
        if recv_count <= 3 or recv_count % 50 == 0:
            print(f"[udp-recv] #{recv_count} de {addr}, {len(data)} bytes")

        try:
            cipher = make_cipher(session.key, header)
            opus_frame = cipher.decrypt(payload)
            pcm = decoder.decode(opus_frame, FRAME_SAMPLES)
        except Exception as e:
            print(f"[udp-recv] ERROR decodificando paquete #{recv_count}: {e!r}")
            continue

        samples = np.frombuffer(pcm, dtype=np.int16)
        out_stream.write(samples)

    out_stream.stop()
    out_stream.close()


# ---------------------------------------------------------------------------
# Control MQTT: listen / goodbye
# ---------------------------------------------------------------------------
def send_listen_start(client):
    msg = {
        "session_id": session.session_id,
        "type": "listen",
        "state": "start",
        "mode": "manual",
    }
    client.publish(TOPIC_PUBLISH, json.dumps(msg))
    print("[mqtt] → listen start")


def send_listen_stop(client):
    msg = {
        "session_id": session.session_id,
        "type": "listen",
        "state": "stop",
        "mode": "manual",
    }
    client.publish(TOPIC_PUBLISH, json.dumps(msg))
    print("[mqtt] → listen stop")


def send_goodbye(client):
    msg = {"session_id": session.session_id, "type": "goodbye"}
    client.publish(TOPIC_PUBLISH, json.dumps(msg))
    print("[mqtt] → goodbye")


# ---------------------------------------------------------------------------
# Push-to-talk por consola
# ---------------------------------------------------------------------------
def ptt_console_loop(client, stop_evt: threading.Event):
    print()
    print("[ptt] Presioná ENTER para empezar a hablar, ENTER de nuevo para parar.")
    print("[ptt] Ctrl+C para salir.")
    while not stop_evt.is_set():
        try:
            input()
        except EOFError:
            break
        if stop_evt.is_set():
            break
        if session.ptt_active.is_set():
            session.ptt_active.clear()
            print()   # nueva línea después del medidor RMS
            send_listen_stop(client)
            print("[ptt] PARADO  — ENTER para volver a hablar")
        else:
            session.ptt_active.set()
            send_listen_start(client)
            print("[ptt] HABLANDO — ENTER para parar")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    print("[main] dispositivos de audio disponibles:")
    print(sd.query_devices())
    print(f"[main] default device (in, out): {sd.default.device}")
    if INPUT_DEVICE is None or OUTPUT_DEVICE is None:
        print("[main] AVISO: INPUT_DEVICE/OUTPUT_DEVICE en None -> usando "
              "default del sistema. Si falla, poné el índice numérico "
              "de la lista de arriba en esas dos variables.")

    mqtt_client = start_mqtt()

    print("[main] esperando handshake de sesión por MQTT...")
    if not session.ready.wait(timeout=15):
        raise RuntimeError("no llegó respuesta 'hello' del servidor por MQTT")

    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind(("0.0.0.0", 0))
    dest = (session.udp_host, session.udp_port)

    encoder = opuslib.Encoder(SAMPLE_RATE, CHANNELS, opuslib.APPLICATION_VOIP)
    decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)

    stop_evt = threading.Event()
    t_send = threading.Thread(target=sender_loop,
                               args=(udp_sock, dest, encoder, stop_evt),
                               daemon=True)
    t_recv = threading.Thread(target=receiver_loop,
                               args=(udp_sock, decoder, stop_evt),
                               daemon=True)
    t_ptt = threading.Thread(target=ptt_console_loop,
                              args=(mqtt_client, stop_evt),
                              daemon=True)
    t_send.start()
    t_recv.start()
    t_ptt.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[main] cerrando...")
        stop_evt.set()
        if session.ptt_active.is_set():
            session.ptt_active.clear()
            send_listen_stop(mqtt_client)
        send_goodbye(mqtt_client)
        time.sleep(0.3)   # deja tiempo al broker para procesar el goodbye
        t_send.join(timeout=2)
        t_recv.join(timeout=2)
        mqtt_client.loop_stop()
        mqtt_client.disconnect()


if __name__ == "__main__":
    main()
