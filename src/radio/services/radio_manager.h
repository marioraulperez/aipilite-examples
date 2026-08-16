#pragma once

#include "stations.h"

void radioManagerInit();
void radioManagerPlayCurrent();
void radioManagerNextCountry();
void radioManagerNextStation();
void radioManagerReconnect();

const RadioCountry &radioCurrentCountry();
const RadioStation &radioCurrentStation();
uint8_t radioCountryIndex();
uint8_t radioStationIndex();
