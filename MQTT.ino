#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// =================== CONFIG WIFI ====================
const char* ssid = "motog72";
const char* password = "otalora27";

// =================== CONFIG MQTT ====================
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/car/instructions";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// =================== PINES MOTORES ==================
int C1 = 18;
int A1 = 19;
int C2 = 21;
int A2 = 22;
unsigned long requestCount = 0;

// Variables para manejo no bloqueante
unsigned long motorStartTime = 0;
int motorDuration = 0;
bool motorsRunning = false;

// =================== FUNCIONES =======================
void stopMotors() {
  digitalWrite(C1, LOW);
  digitalWrite(A1, LOW);
  digitalWrite(C2, LOW);
  digitalWrite(A2, LOW);
  motorsRunning = false;
  Serial.println("🛑 MOTORES DETENIDOS");
}

void startMotors(char dir, int speed, int duration) {
  stopMotors();

  String dirName;
  switch (dir) {
    case 'f': 
      analogWrite(C1, speed); digitalWrite(A1, LOW);
      analogWrite(C2, speed); digitalWrite(A2, LOW);
      dirName = "ADELANTE";
      break;
    case 'b': 
      digitalWrite(C1, LOW); analogWrite(A1, speed);
      digitalWrite(C2, LOW); analogWrite(A2, speed);
      dirName = "ATRAS";
      break;
    case 'l': 
      digitalWrite(C1, LOW); analogWrite(A1, speed);
      analogWrite(C2, speed); digitalWrite(A2, LOW);
      dirName = "IZQUIERDA";
      break;
    case 'r': 
      analogWrite(C1, speed); digitalWrite(A1, LOW);
      digitalWrite(C2, LOW); analogWrite(A2, speed);
      dirName = "DERECHA";
      break;
    case 'x':
      stopMotors();
      return;
    default: 
      stopMotors();
      return;
  }

  motorsRunning = true;
  motorStartTime = millis();
  motorDuration = duration;
  
  Serial.printf("🚗 INICIANDO MOVIMIENTO: %s | Velocidad: %d | Duración: %dms\n", 
                dirName.c_str(), speed, duration);
}

void checkMotors() {
  if (motorsRunning && (millis() - motorStartTime >= motorDuration)) {
    stopMotors();
    Serial.println("✓ Movimiento completado");
  }
}

// =================== ENDPOINTS =======================

void handlePing() {
  requestCount++;
  
  Serial.println("\n████████████████████████████████████████");
  Serial.printf("🏓 PING - Request #%lu\n", requestCount);
  Serial.printf("🌐 IP: %s\n", server.client().remoteIP().toString().c_str());
  Serial.println("████████████████████████████████████████\n");
  Serial.flush();
  
  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"PONG\"}");
}

void handleMove() {
  requestCount++;
  
  // IMPRIMIR INMEDIATAMENTE
  Serial.println("\n");
  Serial.println("████████████████████████████████████████");
  Serial.printf("🎮 MOVE REQUEST #%lu\n", requestCount);
  Serial.printf("🌐 IP: %s\n", server.client().remoteIP().toString().c_str());
  Serial.printf("📦 Argumentos: %d\n", server.args());
  Serial.flush();
  
  // Mostrar todos los parámetros
  for (int i = 0; i < server.args(); i++) {
    Serial.printf("   ➤ %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
    Serial.flush();
  }
  
  if (!server.hasArg("dir") || !server.hasArg("speed") || !server.hasArg("time")) {
    Serial.println("❌ ERROR: Faltan parámetros");
    Serial.println("████████████████████████████████████████\n");
    Serial.flush();
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters: dir, speed, time\"}");
    return;
  }

  char dir = server.arg("dir")[0];
  int speed = server.arg("speed").toInt();
  int duration = server.arg("time").toInt();
  if (duration > 5000) duration = 5000;

  Serial.printf("🎯 Dir: %c | Speed: %d | Time: %d ms\n", dir, speed, duration);
  Serial.println("████████████████████████████████████████\n");
  Serial.flush();

  // RESPONDER INMEDIATAMENTE
  String response = "{\"status\":\"ok\",\"dir\":\"" + String(dir) + 
                    "\",\"speed\":" + String(speed) + 
                    ",\"time\":" + String(duration) + 
                    ",\"request_id\":" + String(requestCount) + "}";
  server.send(200, "application/json", response);
  
  // LUEGO mover el carro (no bloqueante)
  startMotors(dir, speed, duration);

  // Publicar a MQTT
  if (mqttClient.connected()) {
    String msg = "{\"dir\":\"" + String(dir) + "\",\"speed\":" + String(speed) +
                 ",\"time\":" + String(duration) + ",\"request_id\":" + String(requestCount) + "}";
    mqttClient.publish(mqtt_topic, msg.c_str());
    Serial.println("📤 MQTT publicado");
  }
  
  Serial.flush();
}

void handleNotFound() {
  Serial.println("\n⚠  404 - Ruta no encontrada: " + server.uri());
  server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Not Found\"}");
}

// =================== MQTT ===========================
void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("🔄 Reconectando MQTT... ");
    if (mqttClient.connect("ESP32_CarClient")) {
      Serial.println("✅");
    } else {
      Serial.printf("❌ (rc=%d)\n", mqttClient.state());
    }
  }
}

// =================== SETUP ===========================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("████████████████████████████████████████");
  Serial.println("   ESP32 CAR CONTROL - API REST");
  Serial.println("████████████████████████████████████████\n");
  
  pinMode(C1, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(C2, OUTPUT);
  pinMode(A2, OUTPUT);
  stopMotors();

  Serial.printf("📶 Conectando WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi conectado");
  Serial.print("🌐 IP: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  // Solo endpoints de API
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/move", HTTP_GET, handleMove);
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  Serial.println("\n████████████████████████████████████████");
  Serial.println("  🚀 API REST LISTA");
  Serial.println("████████████████████████████████████████");
  Serial.println("\n📋 ENDPOINTS DISPONIBLES:");
  Serial.println("   GET /ping");
  Serial.println("   GET /move?dir=<f|b|l|r|x>&speed=<0-255>&time=<ms>");
  Serial.println("\n💡 EJEMPLO EN POSTMAN:");
  Serial.print("   http://");
  Serial.print(WiFi.localIP());
  Serial.println("/move?dir=f&speed=200&time=2000");
  Serial.println("\n⏳ Esperando peticiones...\n");
  Serial.flush();
}

// =================== LOOP ============================
void loop() {
  server.handleClient();
  checkMotors();
  
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
 
 
