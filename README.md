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


## 🛰️ API REST

### Endpoints

| Método | Endpoint | Descripción | Ejemplo |
|:------:|:---------|:------------|:--------|
| GET | `/ping` | Healthcheck del servidor | `http://<IP_ESP32>/ping` |
| GET | `/move` | Instrucción de movimiento | `http://<IP_ESP32>/move?dir=f&speed=200&time=2000` |

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

> Si usas un broker propio, cambia host/puerto/topic en el código. Puedes verificar con `mosquitto_sub -h test.mosquitto.org -t esp32/car/instructions -v`.

---

##  Diagrama de Secuencia (Mermaid)


<img width="2461" height="1326" alt="DIAGRAMA" src="https://github.com/user-attachments/assets/2a3ee186-0d4b-4925-8def-18d0b478b1cc" />



##  Evidencias (Postman + Monitor Serie)



### ▶️ 1) Movimiento hacia adelante (`dir=f`)
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


## 🧭 Flujo de operación

1. Cliente hace `GET /move`.
2. ESP32 valida y responde inmediatamente con JSON.
3. Se activa movimiento **no bloqueante**.
4. Se publica mensaje en **MQTT** con `client_ip`.
5. Al cumplirse `time`, `stopMotors()`.
6. `GET /ping` verifica salud del servidor.




.
