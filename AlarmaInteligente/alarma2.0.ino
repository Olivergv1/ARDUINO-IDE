#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "time.h"

// --- CONFIGURACIÓN ---
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define THINGSBOARD_SERVER ""
#define THINGSBOARD_TOKEN ""
#define DISCORD_WEBHOOK_URL ""
#define DISCORD_USER_ID "" // <--- ¡IMPORTANTE!
#define BEACON_UUID ""

// --- Pines del Proyecto ---
#define PIR_PIN       27
#define LDR_PIN       34
#define POT_PIN       35
#define BUTTON_PIN    25
#define RELAY_PIN     26
#define LED_PIN       23

// --- Clientes ---
WiFiClient espClient;
PubSubClient client(espClient);

// --- Variables Globales ---
bool alarmaActivada = false;
unsigned long tiempoAnterior = 0;
bool notificacionEnviada = false;

// --- Variables para Bluetooth ---
bool dueñoPresente = false;
unsigned long tiempoUltimoEscaneo = 0;
const long intervaloEscaneo = 300000; // Escanear cada 5 minutos

// ---> DISCORD: Función de envío de mensajes COMPLETAMENTE REHECHA <---
void enviarMensajeDiscord(String titulo, String detalle) {
  if (WiFi.status() == WL_CONNECTED) {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){ return; }
    char horaActual[20];
    strftime(horaActual, sizeof(horaActual), "%H:%M:%S", &timeinfo);

    // Formato compacto con mención
    String mencion = "<@" + String(DISCORD_USER_ID) + ">";
    String mensajeCompleto = mencion + " **" + titulo + "** (" + String(horaActual) + ")\n";
    mensajeCompleto += "> " + detalle + "\n";
    mensajeCompleto += "____________________";

    HTTPClient http;
    WiFiClientSecure *client_secure = new WiFiClientSecure;
    client_secure->setInsecure(); 

    if (http.begin(*client_secure, DISCORD_WEBHOOK_URL)) {
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(2000); // Límite de tiempo de 2 segundos para evitar congelamiento

      // JSON para permitir la mención
      StaticJsonDocument<512> jsonDoc;
      jsonDoc["content"] = mensajeCompleto;
      JsonObject allowed_mentions = jsonDoc.createNestedObject("allowed_mentions");
      JsonArray parse = allowed_mentions.createNestedArray("parse");
      parse.add("users");
      
      char jsonBuffer[512];
      serializeJson(jsonDoc, jsonBuffer);
      
      http.POST(jsonBuffer);
      http.end();
    }
    delete client_secure;
  }
}

// Clase para manejar los resultados del escaneo BLE
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().toString() == BEACON_UUID) {
            dueñoPresente = true;
        }
    }
};

// Función on_message (Control Remoto)
void on_message(char* topic, byte* payload, unsigned int length) {
  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0';
  StaticJsonDocument<200> doc;
  deserializeJson(doc, json);
  const char* methodName = doc["method"];
  if (methodName != NULL && strcmp(methodName, "setValue") == 0) {
    if (alarmaActivada) {
        enviarMensajeDiscord("ALARMA DESACTIVADA", "Desactivación remota desde el panel.");
    }
    alarmaActivada = false;
    String topicStr = String(topic);
    String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
    client.publish(responseTopic.c_str(), "{}");
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  configTime(-18000, 0, "pool.ntp.org");

  client.setServer(THINGSBOARD_SERVER, 1883);
  client.setCallback(on_message);
  
  Serial.println("Sistema de Alarma con Llave Inteligente - LISTO");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32 Device", THINGSBOARD_TOKEN, NULL)) {
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      delay(5000);
    }
  }
}

// Función para escanear Bluetooth de forma controlada
void escanearBluetooth() {
  dueñoPresente = false;
  Serial.println("Pausando WiFi para escanear Bluetooth...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(2, false); 
  
  if (dueñoPresente) {
    Serial.println(">>> Llave inteligente encontrada! <<<");
  } else {
    Serial.println("Llave inteligente no encontrada.");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Reanudando WiFi...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    return;
  }
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  if (millis() - tiempoUltimoEscaneo > intervaloEscaneo) {
    tiempoUltimoEscaneo = millis();
    escanearBluetooth();
    return;
  }

  bool hayMovimiento = digitalRead(PIR_PIN);
  int nivelLuz = analogRead(LDR_PIN);
  int umbralLuz = analogRead(POT_PIN);
  bool estaOscuro = nivelLuz > umbralLuz;
  bool botonPresionado = (digitalRead(BUTTON_PIN) == LOW);

  // --- LÓGICA DE CONTROL ---
  if (botonPresionado) {
    if (alarmaActivada) {
        enviarMensajeDiscord("ALARMA DESACTIVADA", "Desactivación manual con el botón físico.");
    }
    alarmaActivada = false;
  } 
  else if (!alarmaActivada && hayMovimiento && estaOscuro && !dueñoPresente) {
    alarmaActivada = true;
    if (!notificacionEnviada) {
        String detalle = "Nivel de Luz: " + String(nivelLuz) + ", Umbral: " + String(umbralLuz);
        enviarMensajeDiscord("🚨 ALERTA DE MOVIMIENTO 🚨", detalle);
        notificacionEnviada = true;
    }
  }

  // --- CONTROL DE SALIDAS (RELÉ Y LED) ---
  if (alarmaActivada) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    notificacionEnviada = false;
  }

  // --- ENVÍO DE DATOS Y MONITOR SERIE ---
  if (millis() - tiempoAnterior > 1000) {
    tiempoAnterior = millis();
    Serial.print("Movimiento: "); Serial.print(hayMovimiento);
    Serial.print(" | Luz: "); Serial.print(nivelLuz);
    Serial.print(" | Umbral: "); Serial.print(umbralLuz);
    Serial.print(" | Dueño Presente?: "); Serial.print(dueñoPresente);
    Serial.print(" | Alarma ON?: "); Serial.println(alarmaActivada);
    
    StaticJsonDocument<200> jsonBuffer;
    JsonObject telemetry = jsonBuffer.to<JsonObject>();
    telemetry["movimiento"] = hayMovimiento;
    telemetry["luz"] = nivelLuz;
    telemetry["umbral"] = umbralLuz;
    telemetry["alarma_activa"] = alarmaActivada;
    telemetry["dueño_presente"] = dueñoPresente;
    char payload[200];
    serializeJson(telemetry, payload);
    client.publish("v1/devices/me/telemetry", payload);
  }
}