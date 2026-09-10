# AIPI Lite — Protocolo MQTT + UDP

Documentación del protocolo de comunicación del **AIPI Lite** (dispositivo físico, color rojo)
y su emulador en `ai_toy_client.py`.

Fuente oficial del protocolo:
[78/xiaozhi-esp32 — docs/mqtt-udp.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/mqtt-udp.md)

---

## Dispositivo

El AIPI Lite es un toy de voz con IA basado en la arquitectura xiaozhi-esp32.
Se identifica con:

| Campo       | Valor                        |
|-------------|------------------------------|
| Serial      | `XY006PL01USA0428029`        |
| MQTT user   | igual al serial              |
| MQTT pass   | `97a9c6b5032b8404c51a9adbb89513a6` |

Las credenciales se obtuvieron levantando un hotspot en la PC y capturando el tráfico con Wireshark.

---

## Arquitectura de dos canales

```
AIPI Lite  ──MQTT (TCP 1883)──▶  broker  ──▶  servidor IA
           ◀─────────────────────────────────────────────
           ──UDP (puerto dado por el server)──▶  audio
           ◀────────────────────────────────────────────
```

- **MQTT** — mensajes de control en JSON (bajo volumen, fiable)
- **UDP** — audio Opus cifrado con AES-128-CTR (baja latencia, bidireccional)

---

## Flujo completo de sesión

```
Dispositivo                          Servidor
    │                                    │
    │── MQTT CONNECT ──────────────────▶ │
    │── hello (MQTT) ──────────────────▶ │   tipo: "hello", transport: "udp"
    │◀─ hello (MQTT) ────────────────── │   session_id + clave UDP + endpoint
    │                                    │
    │── listen start (MQTT) ──────────▶ │   tipo: "listen", state: "start"
    │═══ audio Opus/AES → UDP ════════▶ │   paquetes de voz del usuario
    │◀══ audio Opus/AES ← UDP ══════════│   respuesta TTS del servidor
    │── listen stop (MQTT) ───────────▶ │   tipo: "listen", state: "stop"
    │                                    │
    │── goodbye (MQTT) ───────────────▶ │
    │── MQTT DISCONNECT ──────────────▶ │
```

---

## Mensajes MQTT (dispositivo → servidor)

Topic de publicación: `xorigin/device-server/<SERIAL>`
Topic de suscripción: `forwards/p2p/<SERIAL>`

### hello
```json
{
  "type": "hello",
  "version": 3,
  "transport": "udp",
  "features": { "mcp": true, "aec": false },
  "audio_params": { "format": "opus", "sample_rate": 24000,
                    "channels": 1, "frame_duration": 60 }
}
```
- `mcp`: habilita comandos IoT vía Model Context Protocol
- `aec`: `false` = sin cancelación de eco por hardware (el servidor no aplica AEC extra)

### listen
```json
{ "session_id": "xxx", "type": "listen", "state": "start", "mode": "manual" }
{ "session_id": "xxx", "type": "listen", "state": "stop",  "mode": "manual" }
```
**Crítico:** sin este mensaje el servidor ignora el audio UDP aunque llegue.
Es el equivalente al botón push-to-talk físico.

### goodbye
```json
{ "session_id": "xxx", "type": "goodbye" }
```

---

## Mensajes MQTT (servidor → dispositivo)

| `type`    | Descripción |
|-----------|-------------|
| `hello`   | Respuesta al handshake; incluye endpoint UDP, key y nonce |
| `tts`     | Ciclo de vida de la respuesta de voz: `state` = `start` / `stop` / `sentence_start` |
| `stt`     | Resultado del reconocimiento de voz (transcripción) |
| `llm`     | Actualización de emoción para UI |
| `mcp`     | Comando IoT (JSON-RPC 2.0) |
| `alert`   | Alerta con `status`, `message`, `emotion` |
| `goodbye` | El servidor cierra la sesión; el dispositivo debe cerrar el canal UDP |

---

## Canal UDP — formato de paquete

```
 0       1       2       3       4       5       6       7  bytes
┌───────┬───────┬───────────────┬───────────────────────────────┐
│ type  │ flags │  payload_len  │             ssrc              │
├───────────────────────────────┼───────────────────────────────┤
│           timestamp           │           sequence            │
├───────────────────────────────┴───────────────────────────────┤
│              payload (payload_len bytes, cifrado)             │
└───────────────────────────────────────────────────────────────┘
```

| Campo         | Tamaño | Notas |
|---------------|--------|-------|
| `type`        | 1 B    | siempre `0x01` |
| `flags`       | 1 B    | sin uso (`0x00`) |
| `payload_len` | 2 B    | big-endian |
| `ssrc`        | 4 B    | viene del nonce del hello |
| `timestamp`   | 4 B    | fijo en `0` durante la sesión |
| `sequence`    | 4 B    | big-endian, arranca en 1, +1 por paquete |
| `payload`     | variable | audio Opus cifrado con AES-128-CTR |

**Cifrado:** el header completo de 16 bytes se usa como `initial_value` del contador AES-CTR.

---

## VAD — quién decide cuándo el usuario terminó de hablar

El servidor **no tiene VAD propio**. Siempre espera que el cliente le señale cuándo procesar el audio. El stream UDP llega pero se encola hasta recibir el `listen stop` por MQTT.

El dispositivo físico maneja esto de dos maneras:

```
ESP32 firmware (cliente)
  ├── modo "manual"  → usuario aprieta botón físico → cliente manda listen start/stop
  └── modo "auto"    → webrtcvad corre en el ESP32 → cliente manda listen start/stop solo
                                      ↑
                                VAD local en el chip
```

En ningún caso el servidor "decide" que el usuario terminó de hablar — esa responsabilidad es siempre del cliente.

En el emulador (`ai_toy_client.py`) usamos `mode: "manual"` con la tecla Enter, que es el equivalente exacto al botón PTT del juguete físico. Para implementar `mode: "auto"` habría que agregar un hilo de VAD (por ejemplo con `webrtcvad`) que detecte silencio y llame `send_listen_stop` automáticamente.

---

## Emulador — `ai_toy_client.py`

| Función / clase    | Responsabilidad |
|--------------------|-----------------|
| `SessionInfo`      | Estado de sesión: credenciales UDP, PTT, referencia MQTT |
| `on_connect`       | Envía `hello` al conectarse; suscribe al topic de respuestas |
| `on_message`       | Parsea mensajes del servidor: `hello`, `tts`, `goodbye` |
| `sender_loop`      | Mic → Opus encode → AES-128-CTR → UDP (solo cuando PTT activo) |
| `receiver_loop`    | UDP → AES decrypt → Opus decode → speaker |
| `ptt_console_loop` | Enter para start/stop; envía `listen` por MQTT |
| `send_goodbye`     | Envía `goodbye` y desconecta limpio al hacer Ctrl+C |

### Uso básico

```bash
pip install paho-mqtt pycryptodome opuslib sounddevice numpy
python ai_toy_client.py
# → presionar ENTER para hablar, ENTER para parar, Ctrl+C para salir
```
