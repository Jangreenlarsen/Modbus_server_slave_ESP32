# HTTP REST API Documentation

**Version:** v6.0.0 | **Feature:** FEAT-011

ESP32 Modbus RTU Server's HTTP REST API giver nem integration med Node-RED, web dashboards, og andre HTTP-baserede systemer.

---

## Indholdsfortegnelse

1. [Oversigt](#oversigt)
2. [Konfiguration](#konfiguration)
3. [Authentication](#authentication)
4. [API Endpoints](#api-endpoints)
   - [System Status](#system-status)
   - [Counters](#counters)
   - [Timers](#timers)
   - [Registers](#registers)
   - [ST Logic](#st-logic)
5. [Response Format](#response-format)
6. [Error Handling](#error-handling)
7. [Node-RED Integration](#node-red-integration)
8. [Eksempler](#eksempler)

---

## Oversigt

HTTP REST API'en kører som en separat FreeRTOS task og blokerer ikke Modbus kommunikation. Den bruger ESP-IDF's `esp_http_server` komponent og ArduinoJson til JSON serialisering.

### Nøglefunktioner
- **JSON Responses:** Alle endpoints returnerer JSON
- **RESTful Design:** GET for læsning, POST for skrivning
- **Basic Auth:** Optional HTTP Basic Authentication
- **Statistics:** Request/error counters via `show http`
- **Low Latency:** Typisk <50ms response time

### Hukommelsesforbrug
| Komponent | RAM | Flash |
|-----------|-----|-------|
| HTTP Server Task | ~4 KB | ~20 KB |
| ArduinoJson | ~2 KB (stack) | ~15 KB |
| Handler Code | - | ~8 KB |
| **Total** | **~6 KB** | **~43 KB** |

---

## Konfiguration

### CLI Kommandoer

```bash
# Aktivér/deaktivér HTTP server
set http enabled on
set http enabled off

# Skift port (default: 80)
set http port 8080

# Aktivér Basic Authentication
set http auth on
set http username admin
set http password hemmeligt123

# Deaktivér authentication
set http auth off

# Gem konfiguration
save

# Vis HTTP status
show http
```

### Default Konfiguration
| Parameter | Default Værdi |
|-----------|---------------|
| Enabled | Yes |
| Port | 80 |
| Auth Enabled | No |
| Username | admin |
| Password | modbus123 |

### Eksempel: `show http` Output
```
=== HTTP REST API STATUS ===
Server Status: RUNNING

--- Configuration ---
Enabled: YES
Port: 80
Auth: DISABLED

--- Statistics ---
Total Requests: 156
Successful (2xx): 152
Client Errors (4xx): 3
Server Errors (5xx): 1

--- API Endpoints ---
  GET  /api/status           - System info
  GET  /api/counters         - All counters
  ...
```

---

## Authentication

Når `auth` er aktiveret, kræves HTTP Basic Authentication header.

### Request Header Format
```
Authorization: Basic base64(username:password)
```

### Eksempel med curl
```bash
# Med authentication
curl -u admin:hemmeligt123 http://192.168.1.100/api/status

# Alternativt
curl -H "Authorization: Basic YWRtaW46aGVtbWVsaWd0MTIz" \
     http://192.168.1.100/api/status
```

### Fejlresponse (401 Unauthorized)
```json
{
  "error": "Authentication required",
  "status": 401
}
```

---

## API Endpoints

### Base URL
```
http://<ESP32_IP>:<PORT>
```
Eksempel: `http://192.168.1.100` (port 80) eller `http://192.168.1.100:8080`

---

### System Status

#### GET /api/status
Returnerer system information.

**Response:**
```json
{
  "version": "6.0.0",
  "build": 1105,
  "uptime_ms": 123456789,
  "heap_free": 180000,
  "wifi_connected": true,
  "ip": "192.168.1.100",
  "modbus_slave_id": 1
}
```

**Response Fields:**
| Field | Type | Description |
|-------|------|-------------|
| `version` | string | Firmware version |
| `build` | number | Build number |
| `uptime_ms` | number | Milliseconds since boot |
| `heap_free` | number | Free heap bytes |
| `wifi_connected` | boolean | WiFi connection status |
| `ip` | string/null | IP address (null if not connected) |
| `modbus_slave_id` | number | Configured Modbus slave ID |

**Eksempel:**
```bash
curl http://192.168.1.100/api/status
```

---

### Counters

#### GET /api/counters
Returnerer alle 4 counters.

**Response:**
```json
{
  "counters": [
    {
      "id": 1,
      "enabled": true,
      "mode": "HW_PCNT",
      "value": 12345
    },
    {
      "id": 2,
      "enabled": false,
      "mode": "DISABLED",
      "value": 0
    },
    {
      "id": 3,
      "enabled": true,
      "mode": "SW",
      "value": 567
    },
    {
      "id": 4,
      "enabled": false,
      "mode": "DISABLED",
      "value": 0
    }
  ]
}
```

**Counter Modes:**
| Mode | Description |
|------|-------------|
| `DISABLED` | Counter ikke aktiveret |
| `SW` | Software polling mode |
| `SW_ISR` | Software interrupt mode |
| `HW_PCNT` | Hardware PCNT mode |

---

#### GET /api/counters/{id}
Returnerer detaljeret info for enkelt counter.

**URL Parameter:**
- `id` - Counter ID (1-4)

**Response:**
```json
{
  "id": 1,
  "enabled": true,
  "mode": "HW_PCNT",
  "value": 12345,
  "raw": 123,
  "frequency": 100,
  "running": true,
  "overflow": false,
  "compare_triggered": false
}
```

**Response Fields:**
| Field | Type | Description |
|-------|------|-------------|
| `id` | number | Counter ID (1-4) |
| `enabled` | boolean | Counter enabled |
| `mode` | string | Counter mode |
| `value` | number | Scaled counter value |
| `raw` | number | Raw prescaled value |
| `frequency` | number | Measured frequency (Hz) |
| `running` | boolean | Counter running status |
| `overflow` | boolean | Overflow flag |
| `compare_triggered` | boolean | Compare threshold reached |

**Eksempel:**
```bash
curl http://192.168.1.100/api/counters/1
```

**Fejl (ugyldig ID):**
```json
{
  "error": "Invalid counter ID (must be 1-4)",
  "status": 400
}
```

---

### Timers

#### GET /api/timers
Returnerer alle 4 timers.

**Response:**
```json
{
  "timers": [
    {
      "id": 1,
      "enabled": true,
      "mode": "ASTABLE",
      "output": true
    },
    {
      "id": 2,
      "enabled": false,
      "mode": "DISABLED",
      "output": false
    },
    {
      "id": 3,
      "enabled": true,
      "mode": "MONOSTABLE",
      "output": false
    },
    {
      "id": 4,
      "enabled": false,
      "mode": "DISABLED",
      "output": false
    }
  ]
}
```

**Timer Modes:**
| Mode | Description |
|------|-------------|
| `DISABLED` | Timer ikke aktiveret |
| `ONESHOT` | Mode 1: One-shot (3 faser) |
| `MONOSTABLE` | Mode 2: Monostable (retriggerable pulse) |
| `ASTABLE` | Mode 3: Astable (blink/oscillate) |
| `INPUT_TRIGGERED` | Mode 4: Input-triggered |

---

#### GET /api/timers/{id}
Returnerer detaljeret info for enkelt timer.

**Response (Astable mode eksempel):**
```json
{
  "id": 1,
  "enabled": true,
  "mode": "ASTABLE",
  "output_coil": 10,
  "output": true,
  "on_duration_ms": 1000,
  "off_duration_ms": 500
}
```

**Response (Monostable mode eksempel):**
```json
{
  "id": 3,
  "enabled": true,
  "mode": "MONOSTABLE",
  "output_coil": 12,
  "output": false,
  "pulse_duration_ms": 2000
}
```

---

### Registers

#### GET /api/registers/hr/{addr}
Læs holding register.

**URL Parameter:**
- `addr` - Register address (0-159)

**Response:**
```json
{
  "address": 100,
  "value": 12345
}
```

**Eksempel:**
```bash
curl http://192.168.1.100/api/registers/hr/100
```

---

#### POST /api/registers/hr/{addr}
Skriv til holding register.

**URL Parameter:**
- `addr` - Register address (0-159)

**Request Body:**
```json
{
  "value": 54321
}
```

**Response:**
```json
{
  "address": 100,
  "value": 54321,
  "status": "ok"
}
```

**Eksempel:**
```bash
curl -X POST \
     -H "Content-Type: application/json" \
     -d '{"value": 54321}' \
     http://192.168.1.100/api/registers/hr/100
```

---

#### GET /api/registers/ir/{addr}
Læs input register (read-only).

**Response:**
```json
{
  "address": 50,
  "value": 1234
}
```

---

#### GET /api/registers/coils/{addr}
Læs coil.

**Response:**
```json
{
  "address": 10,
  "value": true
}
```

---

#### POST /api/registers/coils/{addr}
Skriv til coil.

**Request Body:**
```json
{
  "value": true
}
```
eller
```json
{
  "value": 1
}
```

**Response:**
```json
{
  "address": 10,
  "value": true,
  "status": "ok"
}
```

---

#### GET /api/registers/di/{addr}
Læs discrete input (read-only).

**Response:**
```json
{
  "address": 5,
  "value": false
}
```

---

### ST Logic

#### GET /api/logic
Returnerer status for alle 4 ST Logic programs.

**Response:**
```json
{
  "enabled": true,
  "execution_interval_ms": 10,
  "total_cycles": 123456,
  "programs": [
    {
      "id": 1,
      "name": "Logic1",
      "enabled": true,
      "compiled": true,
      "execution_count": 12345,
      "error_count": 0
    },
    {
      "id": 2,
      "name": "Logic2",
      "enabled": false,
      "compiled": false,
      "execution_count": 0,
      "error_count": 0
    },
    {
      "id": 3,
      "name": "Logic3",
      "enabled": true,
      "compiled": true,
      "execution_count": 12340,
      "error_count": 2,
      "last_error": "Division by zero"
    },
    {
      "id": 4,
      "name": "Logic4",
      "enabled": false,
      "compiled": false,
      "execution_count": 0,
      "error_count": 0
    }
  ]
}
```

---

#### GET /api/logic/{id}
Returnerer detaljeret info for enkelt program med variabler.

**Response:**
```json
{
  "id": 1,
  "name": "Logic1",
  "enabled": true,
  "compiled": true,
  "execution_count": 12345,
  "error_count": 0,
  "last_execution_us": 125,
  "min_execution_us": 98,
  "max_execution_us": 450,
  "overrun_count": 0,
  "variables": [
    {
      "index": 0,
      "name": "counter",
      "type": "INT",
      "value": 42
    },
    {
      "index": 1,
      "name": "threshold",
      "type": "INT",
      "value": 100
    },
    {
      "index": 2,
      "name": "motor_on",
      "type": "BOOL",
      "value": true
    },
    {
      "index": 3,
      "name": "temperature",
      "type": "REAL",
      "value": 23.5
    }
  ]
}
```

**Variable Types:**
| Type | Description |
|------|-------------|
| `BOOL` | Boolean (true/false) |
| `INT` | 16-bit signed integer |
| `DINT` | 32-bit signed integer |
| `REAL` | 32-bit floating point |

---

## Response Format

### Success Response (2xx)
Alle succesfulde responses returnerer JSON med relevante data.

**Content-Type:** `application/json`

### Error Response (4xx/5xx)
```json
{
  "error": "Error message description",
  "status": 400
}
```

**HTTP Status Codes:**
| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad Request (invalid parameters) |
| 401 | Unauthorized (auth required) |
| 404 | Not Found |
| 500 | Internal Server Error |

---

## Error Handling

### Typiske Fejl

**Ugyldig register adresse:**
```bash
curl http://192.168.1.100/api/registers/hr/999
```
```json
{
  "error": "Invalid register address",
  "status": 400
}
```

**Ugyldig JSON:**
```bash
curl -X POST -d 'not json' http://192.168.1.100/api/registers/hr/100
```
```json
{
  "error": "Invalid JSON",
  "status": 400
}
```

**Manglende value felt:**
```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{}' http://192.168.1.100/api/registers/hr/100
```
```json
{
  "error": "Missing 'value' field",
  "status": 400
}
```

---

## Node-RED Integration

HTTP REST API'en er designet til nem integration med Node-RED.

### Basic Setup

1. **Tilføj HTTP Request node**
2. **Konfigurer:**
   - Method: GET (for læsning) eller POST (for skrivning)
   - URL: `http://<ESP32_IP>/api/...`
   - Return: `a parsed JSON object`

3. **Tilføj JSON node** (hvis nødvendigt)
4. **Behandl data i Function node**

### Flow Eksempel: Læs Counter Værdi

```
[http request] → [json] → [function] → [debug/dashboard]
```

**HTTP Request Node:**
- URL: `http://192.168.1.100/api/counters/1`
- Method: GET
- Return: a parsed JSON object

**Function Node:**
```javascript
// Extract counter value
msg.payload = msg.payload.value;
return msg;
```

### Flow Eksempel: Skriv Register

```
[inject/ui] → [function] → [http request] → [debug]
```

**Function Node (før HTTP Request):**
```javascript
msg.payload = { "value": msg.payload };
msg.headers = { "Content-Type": "application/json" };
return msg;
```

**HTTP Request Node:**
- URL: `http://192.168.1.100/api/registers/hr/100`
- Method: POST
- Return: a parsed JSON object

### Flow Eksempel: Dashboard med Auto-Update

```
[inject (5s interval)] → [http request] → [json] → [function] → [gauge/chart]
```

**Inject Node:**
- Repeat: interval 5 seconds
- Payload: (empty)

**Function Node:**
```javascript
// For gauge: extract single value
msg.payload = msg.payload.heap_free;
return msg;
```

### Med Authentication

Hvis HTTP auth er aktiveret:

**HTTP Request Node:**
- Use authentication: ✓
- Type: basic authentication
- Username: admin
- Password: [din password]

---

## Eksempler

### Bash/curl Eksempler

```bash
# System status
curl http://192.168.1.100/api/status

# Alle counters
curl http://192.168.1.100/api/counters

# Specifik counter
curl http://192.168.1.100/api/counters/1

# Alle timers
curl http://192.168.1.100/api/timers

# Læs holding register
curl http://192.168.1.100/api/registers/hr/100

# Skriv holding register
curl -X POST -H "Content-Type: application/json" \
     -d '{"value": 12345}' \
     http://192.168.1.100/api/registers/hr/100

# Læs coil
curl http://192.168.1.100/api/registers/coils/10

# Skriv coil
curl -X POST -H "Content-Type: application/json" \
     -d '{"value": true}' \
     http://192.168.1.100/api/registers/coils/10

# ST Logic status
curl http://192.168.1.100/api/logic

# ST Logic program med variabler
curl http://192.168.1.100/api/logic/1

# Med authentication
curl -u admin:password http://192.168.1.100/api/status
```

### Python Eksempel

```python
import requests
import json

ESP32_IP = "192.168.1.100"
BASE_URL = f"http://{ESP32_IP}"

# Optional: Authentication
AUTH = None  # or ('admin', 'password')

# GET system status
response = requests.get(f"{BASE_URL}/api/status", auth=AUTH)
status = response.json()
print(f"Version: {status['version']}")
print(f"Uptime: {status['uptime_ms']} ms")
print(f"Free heap: {status['heap_free']} bytes")

# GET counter value
response = requests.get(f"{BASE_URL}/api/counters/1", auth=AUTH)
counter = response.json()
print(f"Counter 1 value: {counter['value']}")

# POST write register
data = {"value": 12345}
response = requests.post(
    f"{BASE_URL}/api/registers/hr/100",
    json=data,
    auth=AUTH
)
result = response.json()
print(f"Write result: {result['status']}")
```

### JavaScript/Fetch Eksempel

```javascript
const ESP32_IP = '192.168.1.100';

// GET system status
async function getStatus() {
  const response = await fetch(`http://${ESP32_IP}/api/status`);
  const data = await response.json();
  console.log('Version:', data.version);
  console.log('Uptime:', data.uptime_ms, 'ms');
  return data;
}

// GET counter
async function getCounter(id) {
  const response = await fetch(`http://${ESP32_IP}/api/counters/${id}`);
  const data = await response.json();
  return data.value;
}

// POST write register
async function writeRegister(addr, value) {
  const response = await fetch(`http://${ESP32_IP}/api/registers/hr/${addr}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ value: value })
  });
  const result = await response.json();
  return result.status === 'ok';
}

// Eksempel brug
getStatus().then(status => console.log(status));
getCounter(1).then(value => console.log('Counter 1:', value));
writeRegister(100, 54321).then(ok => console.log('Write OK:', ok));
```

---

## Performance

### Typiske Response Tider
| Endpoint | Response Time |
|----------|---------------|
| `/api/status` | ~10-20 ms |
| `/api/counters` | ~15-25 ms |
| `/api/registers/hr/{addr}` | ~5-15 ms |
| `/api/logic/{id}` (med vars) | ~30-50 ms |

### Concurrent Requests
HTTP serveren håndterer requests sekventielt. For høj throughput, brug batching eller polling med passende interval (f.eks. 1-5 sekunder).

### Modbus vs HTTP
HTTP API påvirker ikke Modbus RTU kommunikation - de kører i separate FreeRTOS tasks.

---

## Troubleshooting

### HTTP Server Starter Ikke
- Check WiFi forbindelse: `show wifi`
- Verify HTTP er enabled: `show http`
- Check for port konflikt med Telnet

### 401 Unauthorized
- Check auth er korrekt konfigureret: `show http`
- Verify credentials i request

### Timeout / Ingen Response
- Check ESP32 IP adresse
- Verify WiFi forbindelse
- Check firewall settings

### JSON Parse Fejl
- Ensure Content-Type header: `application/json`
- Validate JSON syntax

---

**Document Version:** 1.0
**Last Updated:** 2026-01-21
**Compatible With:** Firmware v6.0.0+
