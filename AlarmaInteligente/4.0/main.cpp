#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "SD.h"
#include <Wire.h>               // ---> CAMBIO OLED
#include <Adafruit_GFX.h>       // ---> CAMBIO OLED
#include <Adafruit_SSD1306.h>   // ---> CAMBIO OLED

// --- CONFIGURACIÓN ---
#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "12345678910a"
#define THINGSBOARD_SERVER "thingsboard.cloud"
#define THINGSBOARD_TOKEN "wxlWntfhJQMv5RcUFrjW"
#define DISCORD_WEBHOOK_URL "https://discord.com/api/webhooks/1401429369544511629/cZw1Q9hq7y-ihZLco7_lVwQTT8pIsK5pMefw1UnnYmvsOl68yKoC92SbQtYraB4XmS99"
#define DISCORD_USER_ID "861304303922839563"

// --- Pines del Proyecto (ACTUALIZADOS) ---
#define PIR_PIN       27
#define LDR_PIN       34
#define POT_PIN       35
#define RESET_BUTTON_PIN 15 // <-- PIN MOVIDO
#define ARM_BUTTON_PIN   4
#define ARM_LED_PIN      13
#define SD_CS_PIN     5
#define AUDIO_OUT_PIN 25

// ---> CAMBIO OLED: Configuración de la Pantalla
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Clientes ---
WiFiClient espClient;
PubSubClient client(espClient);
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;

// --- Variables Globales ---
bool sistemaArmado = true;
bool alarmaActivada = false;
bool sonidoSilenciado = false;
unsigned long tiempoAnterior = 0;
bool notificacionEnviada = false;
int ultimoEstadoBotonArmado = HIGH;
unsigned long tiempoUltimoRebote = 0;
bool modoOnline = false;

// (El resto del código no cambia)
void enviarMensajeDiscord(String titulo, String detalle) {
  if (WiFi.status() == WL_CONNECTED) {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){ return; }
    char horaActual[20];
    strftime(horaActual, sizeof(horaActual), "%H:%M:%S", &timeinfo);
    String mencion = "<@" + String(DISCORD_USER_ID) + ">";
    String mensajeCompleto = mencion + " **" + titulo + "** (" + String(horaActual) + ")\n";
    mensajeCompleto += "> " + detalle + "\n";
    mensajeCompleto += "____________________";
    HTTPClient http;
    WiFiClientSecure *client_secure = new WiFiClientSecure;
    client_secure->setInsecure(); 
    if (http.begin(*client_secure, DISCORD_WEBHOOK_URL)) {
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(2000);
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

void on_message(char* topic, byte* payload, unsigned int length) {
  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0';
  StaticJsonDocument<200> doc;
  deserializeJson(doc, json);
  const char* methodName = doc["method"];
  if (methodName != NULL && strcmp(methodName, "setValue") == 0) {
    if (alarmaActivada) {
        enviarMensajeDiscord("ALARMA RESETEADA", "Alarma reseteada remotamente desde el panel.");
    }
    alarmaActivada = false;
    if (mp3 && mp3->isRunning()) mp3->stop();
    notificacionEnviada = false;
    
    String topicStr = String(topic);
    String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
    client.publish(responseTopic.c_str(), "{}");
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ARM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ARM_LED_PIN, OUTPUT);
  
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Error al inicializar la tarjeta SD.");
    while(1);
  }

  // ---> CAMBIO OLED: Iniciar pantalla
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Fallo al iniciar la pantalla SSD1306"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Iniciando... (gay si lo lees ❤)");
  display.display();
  delay(1000);

  out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
  out->SetPinout(26, 25, -1);
  out->SetOutputModeMono(true);
  out->SetGain(0.9);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int intentos = 40;
  while (WiFi.status() != WL_CONNECTED && intentos > 0) {
    delay(500);
    intentos--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    modoOnline = true;
    configTime(-18000, 0, "pool.ntp.org");
    client.setServer(THINGSBOARD_SERVER, 1883);
    client.setCallback(on_message);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  } else {
    modoOnline = false;
  }
  
  Serial.println("Sistema de Alarma - LISTO");
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

void loop() {
  if (modoOnline) {
    if (WiFi.status() != WL_CONNECTED) {
      // El sistema de autoreconexión se encargará.
    } else {
      if (!client.connected()) {
        reconnect();
      }
      client.loop(); 
    }
  }

  int estadoBotonArmado = digitalRead(ARM_BUTTON_PIN);
  if (estadoBotonArmado == LOW && ultimoEstadoBotonArmado == HIGH && millis() - tiempoUltimoRebote > 50) {
    sistemaArmado = !sistemaArmado;
    if (!sistemaArmado) {
      alarmaActivada = false;
      if (mp3 && mp3->isRunning()) mp3->stop();
    }
    if (modoOnline) enviarMensajeDiscord(sistemaArmado ? "SISTEMA ARMADO" : "SISTEMA DESARMADO", "El estado del sistema ha cambiado.");
    tiempoUltimoRebote = millis();
  }
  ultimoEstadoBotonArmado = estadoBotonArmado;
  digitalWrite(ARM_LED_PIN, sistemaArmado);

  int nivelLuz = analogRead(LDR_PIN);
  int umbralLuz = analogRead(POT_PIN);

  if (sistemaArmado) {
    bool hayMovimiento = digitalRead(PIR_PIN);
    bool estaOscuro = nivelLuz > umbralLuz;
    bool botonResetPresionado = (digitalRead(RESET_BUTTON_PIN) == LOW);

    if (botonResetPresionado && alarmaActivada) {
      alarmaActivada = false;
      if (mp3 && mp3->isRunning()) mp3->stop();
      notificacionEnviada = false;
      if (modoOnline) enviarMensajeDiscord("ALARMA RESETEADA", "La alarma ha sido reseteada con el botón físico.");
    }
     
    if (!alarmaActivada && hayMovimiento && estaOscuro) {
      alarmaActivada = true;
      if (!notificacionEnviada && modoOnline) {
          String detalle = "Nivel de Luz: " + String(nivelLuz) + ", Umbral: " + String(umbralLuz);
          enviarMensajeDiscord("🚨 ALERTA DE MOVIMIENTO 🚨", detalle);
          notificacionEnviada = true;
      }
    }
  } else {
    alarmaActivada = false;
  }

  if (alarmaActivada) {
    if (mp3 == NULL || !mp3->isRunning()) {
      file = new AudioFileSourceSD("/alerta.mp3");
      mp3 = new AudioGeneratorMP3();
      mp3->begin(file, out);
    }
  } else {
    if (mp3 && mp3->isRunning()) {
      mp3->stop();
    }
  }
  
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
    }
  }
  
  if (!alarmaActivada) {
      notificacionEnviada = false;
  }
  
  if (millis() - tiempoAnterior > 1000) {
    tiempoAnterior = millis();
    
    // ---> CAMBIO OLED: Actualizar la pantalla
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("L ");
    display.print(nivelLuz);
    display.setCursor(0, 32);
    display.print("U ");
    display.print(umbralLuz);
    display.display();
    
    if (modoOnline) {
      StaticJsonDocument<200> jsonBuffer;
      JsonObject telemetry = jsonBuffer.to<JsonObject>();
      telemetry["sistema_armado"] = sistemaArmado;
      telemetry["movimiento"] = digitalRead(PIR_PIN);
      telemetry["luz"] = nivelLuz;
      telemetry["umbral"] = umbralLuz;
      telemetry["alarma_activa"] = alarmaActivada;
      char payload[200];
      serializeJson(telemetry, payload);
      client.publish("v1/devices/me/telemetry", payload);
    }
  }
}




