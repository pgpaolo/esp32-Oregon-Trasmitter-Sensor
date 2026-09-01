#pragma once

#include <Arduino.h>

#ifndef OREGON_TX_PIN
  #ifdef TARGET_ESP32C3
    #define OREGON_TX_PIN 4
  #else
    #define OREGON_TX_PIN 25
  #endif
#endif

#ifndef STATUS_LED_PIN
  #ifdef TARGET_ESP32C3
    #define STATUS_LED_PIN 8
  #else
    #define STATUS_LED_PIN 2
  #endif
#endif

#ifndef BME_SDA_PIN
  #ifdef TARGET_ESP32C3
    #define BME_SDA_PIN 6
  #else
    #define BME_SDA_PIN 21
  #endif
#endif

#ifndef BME_SCL_PIN
  #ifdef TARGET_ESP32C3
    #define BME_SCL_PIN 7
  #else
    #define BME_SCL_PIN 22
  #endif
#endif

#ifndef UV_ADC_PIN
  #ifdef TARGET_ESP32C3
    #define UV_ADC_PIN 0
  #else
    #define UV_ADC_PIN 34
  #endif
#endif

constexpr uint16_t OSV3_HALF_US_DEFAULT = 488;
constexpr uint16_t OSV3_PREAMBLE_BITS = 24;
constexpr uint8_t  MAX_VIRTUAL_SENSORS = 4;
constexpr char AP_SSID[] = "Oregon-TX-Setup";
constexpr char AP_PASSWORD[] = "";
constexpr char FW_VERSION[] = "0.1.0-beta.1";
constexpr char FW_STATUS[] = "BETA / TESTING";
