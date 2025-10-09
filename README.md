#  ESP32 Control de auto – API REST + MQTT - Nicolas Rodriguez - Juan Diego Martinez  - Camilo Otalora.

Sistema de control remoto para un carrito con **ESP32**. Expone una **API REST** con un **único endpoint `/move`** para instrucciones (adelante, atrás, izquierda, derecha, detener) con **velocidad** y **duración (máx. 5 s)**; un **healthcheck `/ping`**; y **publicación MQTT** de cada instrucción (incluye la **IP del cliente** que hizo la petición).  
Este README incluye **endpoints, ejemplos JSON, MQTT, diagrama de secuencia y evidencias**.

---

## Objetivos

- Endpoint **único** de movimiento: `/move?dir=<f|b|l|r|x>&speed=<0-255>&time=<ms≤5000>`.
- Endpoint **healthcheck**: `/ping`.
- Publicar cada instrucción en **MQTT** con **`client_ip`**.
- Documentar **uso**, **evidencias** (Postman + monitor serie) y **diagrama**.

---

##  Tecnologías

- **ESP32 Dev Module** + **Arduino IDE 2.3.6**
- Librerías: `WiFi.h`, `WebServer.h`, `PubSubClient.h`
- **Broker MQTT** público: `test.mosquitto.org:1883`
- **Topic**: `esp32/car/instructions`

---


##  API REST

### Endpoints

| Método | Endpoint | Descripción | Ejemplo |
|:------:|:---------|:------------|:--------|
| GET | `/ping` | Healthcheck del servidor | `http://10.180.50.179/ping?` |
| GET | `/move` | Instrucción de movimiento | `http://10.180.50.179/move?dir=f&speed=200&time=2000` |

### Parámetros de `/move`

| Parámetro | Tipo | Descripción | Valores válidos |
|:--|:--|:--|:--|
| `dir` | `char` | Dirección | `f` (adelante), `b` (atrás), `l` (izquierda), `r` (derecha), `x` (detener) |
| `speed` | `int` | PWM | `0` – `255` |
| `time` | `int` | Duración en ms (**máx. 5000**) | `0` – `5000` |

> **Fail-safe:** el firmware recorta cualquier `time` recibido a **≤ 5000 ms** para evitar movimientos indefinidos.

### Ejemplos de Respuesta JSON

**`GET /ping` → 200 OK**
```json
{
  "status": "ok",
  "message": "PONG"
}
```

**`GET /move?dir=f&speed=220&time=2000` → 200 OK**
```json
{
  "status": "ok",
  "dir": "f",
  "speed": 220,
  "time": 2000,
  "request_id": 4
}
```

**`GET /move` con parámetros faltantes → 400**
```json
{
  "status": "error",
  "message": "Missing parameters: dir, speed, time"
}
```

---

##  MQTT

- **Broker:** `test.mosquitto.org`
- **Puerto:** `1883`
- **Topic:** `esp32/car/instructions`

Cada vez que se procesa `/move` exitosamente, el ESP32 publica un **JSON** con la instrucción **y la IP del cliente** que hizo la petición HTTP.

**Payload publicado (ejemplo):**
```json
{
  "dir": "f",
  "speed": 200,
  "time": 3000,
  "client_ip": "192.168.93.102",
  "request_id": 7
}
```



---

##  Diagrama de Secuencia (Mermaid)


<img width="2461" height="1326" alt="DIAGRAMA" src="https://github.com/user-attachments/assets/2a3ee186-0d4b-4925-8def-18d0b478b1cc" />



##  Evidencias (Postman + Monitor Serie)



###  1) Movimiento hacia adelante (`dir=f`)
<img width="1280" height="684" alt="image" src="https://github.com/user-attachments/assets/f7aaf153-962b-45ed-83c4-ceba5eab008f" />

**Descripción:** Petición desde Postman para mover hacia **adelante** por **2000 ms** a velocidad **200**. En el Monitor Serie se ve la IP del cliente y la confirmación del inicio de movimiento.

###  2) Movimiento hacia atrás (`dir=b`)

<img width="1280" height="682" alt="image" src="https://github.com/user-attachments/assets/7d24198b-256a-4e06-8020-5f400a854a37" />

**Descripción:** `GET /move?dir=b&speed=200&time=2000`. Respuesta 200 OK con JSON y logs de validación en serie.

###  3) Giro a la izquierda (`dir=l`)
<img width="1280" height="682" alt="image" src="https://github.com/user-attachments/assets/7ad24f41-2f38-4f8c-8379-c0a151d5377e" />

**Descripción:** Giro a **izquierda**; ejecución no bloqueante, JSON correcto.

###  4) Giro a la derecha (`dir=r`)
<img width="1280" height="685" alt="image" src="https://github.com/user-attachments/assets/f8a11ff2-52ea-4e2e-9152-0d87c975f458" />

**Descripción:** Giro a **derecha**; JSON y logs confirman velocidad/tiempo.

###  5) Prueba de validación (`dir=a` inválido)
<img width="1280" height="682" alt="image" src="https://github.com/user-attachments/assets/011f558e-f9ec-4317-a187-28cac3f132dc" />

**Descripción:** Dirección inválida; el sistema protege deteniendo motores y publicando a MQTT el request con control seguro.

###  Interaccion con la IA: 
https://github.com/nicorodri18/HTTP-MQTT.git


## � Flujo de operación

1. Cliente hace `GET /move`.
2. ESP32 valida y responde inmediatamente con JSON.
3. Se activa movimiento **no bloqueante**.
4. Se publica mensaje en **MQTT** con `client_ip`.
5. Al cumplirse `time`, `stopMotors()`.
6. `GET /ping` verifica salud del servidor.

```C
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

//  CONFIG WIFI 
const char* ssid = "motog72";
const char* password = "otalora27";

//  CONFIG MQTT 
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/car/instructions";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

//  PINES MOTORES 
int C1 = 18;
int A1 = 19;
int C2 = 21;
int A2 = 22;
unsigned long requestCount = 0;

// Variables para manejo no bloqueante
unsigned long motorStartTime = 0;
int motorDuration = 0;
bool motorsRunning = false;

//  FUNCIONES 
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

// ENDPOINTS :D

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

  // RESPONDER 
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

//  MQTT 
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

//  SETUP 
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
void loop() {
  server.handleClient();
  checkMotors();
  
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}


```

