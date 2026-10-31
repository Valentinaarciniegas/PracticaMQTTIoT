#pragma once
// Copia este archivo como config.h y pon tus datos (config.h no se sube a GitHub)

const char* WIFI_SSID = "TU_RED_WIFI";      // solo redes de 2.4 GHz
const char* WIFI_PASS = "TU_CLAVE";

// Broker MQTT: IP elastica de la EC2 con Mosquitto
const char* MQTT_HOST = "184.196.79.18";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "esp32";
const char* MQTT_PASS = "CLAVE_MQTT";

const char* DEVICE_ID = "esp32-secadero-01";   // client id MQTT
const char* SECADERO  = "secadero1";           // primer nivel del topic: secadero1/temperatura
