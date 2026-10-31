// CocaTrace - Practica 2: ESP32 + DHT22 -> broker MQTT (Mosquitto en AWS)
// Topics:  <SECADERO>/temperatura  y  <SECADERO>/humedad   -> {"valor": 24.6, "ts": 1793461967}
//          <SECADERO>/estado  -> "conectado" / "desconectado" (retenido, Last Will)
// Librerias: PubSubClient (Nick O'Leary) y DHT sensor library (Adafruit)
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h>
#include "config.h"

#define DHTPIN  4
#define DHTTYPE DHT22

const unsigned long INTERVALO_MS = 30000;   // una lectura cada 30 s
const unsigned long REINTENTO_MQTT_MS = 5000;
const int BUFFER_MAX = 120;                 // 1 hora de respaldo

struct Lectura {
  time_t ts;
  float t;
  float h;
};

Lectura buffer[BUFFER_MAX];
int bufInicio = 0;
int bufCount  = 0;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

String topicTemp, topicHum, topicEstado;
unsigned long ultimaLectura = 0;
unsigned long ultimoIntentoMqtt = 0;
bool primeraLectura = true;

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Conectando a WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSin WiFi. Las lecturas quedan en el buffer.");
  }
}

// Conecta al broker con usuario y clave. Si el ESP32 se cae, el broker publica
// "desconectado" en <SECADERO>/estado (Last Will).
bool conectarMQTT() {
  if (mqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (millis() - ultimoIntentoMqtt < REINTENTO_MQTT_MS && ultimoIntentoMqtt != 0) return false;
  ultimoIntentoMqtt = millis();

  Serial.printf("Conectando a MQTT %s:%d... ", MQTT_HOST, MQTT_PORT);
  if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS, topicEstado.c_str(), 1, true, "desconectado")) {
    Serial.println("OK");
    mqtt.publish(topicEstado.c_str(), "conectado", true);
    return true;
  }
  // rc = 4: usuario o clave incorrectos; rc = 5: no autorizado; rc = -2: no hay conexion al puerto
  Serial.printf("fallo, rc=%d\n", mqtt.state());
  return false;
}

void encolar(const Lectura& l) {
  int idx = (bufInicio + bufCount) % BUFFER_MAX;
  buffer[idx] = l;
  if (bufCount < BUFFER_MAX) bufCount++;
  else bufInicio = (bufInicio + 1) % BUFFER_MAX;  // lleno: se descarta la mas antigua
}

bool publicar(const String& topic, float valor, time_t ts) {
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"valor\":%.1f,\"ts\":%lu}", valor, (unsigned long)ts);
  bool ok = mqtt.publish(topic.c_str(), msg);
  Serial.printf("PUB %s %s -> %s\n", topic.c_str(), msg, ok ? "OK" : "FALLO");
  return ok;
}

bool enviar(const Lectura& l) {
  if (!mqtt.connected()) return false;
  return publicar(topicTemp, l.t, l.ts) && publicar(topicHum, l.h, l.ts);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  dht.begin();

  topicTemp   = String(SECADERO) + "/temperatura";
  topicHum    = String(SECADERO) + "/humedad";
  topicEstado = String(SECADERO) + "/estado";

  WiFi.setAutoReconnect(true);
  conectarWiFi();
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(30);
}

void loop() {
  conectarMQTT();
  mqtt.loop();

  if (!primeraLectura && millis() - ultimaLectura < INTERVALO_MS) return;
  primeraLectura = false;
  ultimaLectura = millis();

  conectarWiFi();

  // Cada lectura lleva su hora real; sin hora NTP no se guarda
  time_t ahora = time(nullptr);
  if (ahora < 1700000000) {
    Serial.println("Esperando la hora NTP...");
    return;
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t) || t < -40 || t > 80 || h < 0 || h > 100) {
    Serial.println("Lectura invalida, se descarta.");
    return;
  }

  Serial.printf("T = %.1f °C   HR = %.1f %%\n", t, h);
  encolar({ ahora, t, h });

  while (bufCount > 0) {
    if (!enviar(buffer[bufInicio])) break;
    bufInicio = (bufInicio + 1) % BUFFER_MAX;
    bufCount--;
  }
  if (bufCount > 0) Serial.printf("Pendientes en buffer: %d\n", bufCount);
}
