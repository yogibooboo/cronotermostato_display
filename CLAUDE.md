# Cronotermostato ESP32-S3 con Display Touch

## Obiettivo del progetto

Migrazione e rinnovamento di un cronotermostato esistente (ESP32 + interfaccia web) verso ESP32-S3 con display touch capacitivo integrato (modulo JC3248W535EN).

## Hardware target

### Modulo principale: JC3248W535EN (Guition)
- **MCU**: ESP32-S3-WROOM-1 (dual-core 240MHz)
- **Display**: 3.5" IPS 320x480, controller AXS15231B, interfaccia QSPI
- **Touch**: Capacitivo I2C (indirizzo 0x3B)
- **Memoria**: 16MB Flash, 8MB PSRAM
- **Connettività**: WiFi + Bluetooth integrati
- **Extra**: Slot SD card, ~12 GPIO disponibili

### Pinout display/touch (già verificato):
```cpp
// Display QSPI
#define QSPI_CS   45
#define QSPI_CLK  47
#define QSPI_D0   21
#define QSPI_D1   48
#define QSPI_D2   40
#define QSPI_D3   39

// Touch I2C
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_RST 12
#define TOUCH_INT 11
#define TOUCH_ADDR 0x3B

// Backlight PWM
#define GFX_BL 1
```

### Sensore temperatura
- Attuale: AHT10 (I2C, 0x38) - da confermare se mantenerlo o sostituire

### Uscita relè caldaia
- Da assegnare su GPIO disponibile (era GPIO 19 sul vecchio progetto)

---

## Progetto esistente - Analisi

### Funzionalità implementate (da mantenere):
1. **4 programmi orari** - Ogni programma ha 48 slot (mezz'ore) con soglia temperatura
2. **Modalità operative**: ON/OFF globale, Manuale/Automatico
3. **Programmazione settimanale** - Assegna un programma diverso a ogni giorno
4. **Eccezioni** - Periodo con programma speciale (es. vacanze)
5. **Isteresi** - Evita oscillazioni on/off
6. **Correzione temperatura** - Offset calibrazione sensore
7. **Storico 7 giorni** - Buffer circolare temperatura/umidità/stato caldaia (1 sample/minuto)
8. **Interfaccia web** - Controllo completo via browser con WebSocket

### Struttura dati programma (`fileprog.txt`):
```
Indici 0-199:   4 banchi × 50 valori (48 mezz'ore + 2 padding)
                Ogni valore = soglia temperatura × 10 (es. 175 = 17.5°C)

Indice 200:     ON/OFF (0=off, 1=on)
Indice 201:     Manuale/Auto (0=auto, 1=manuale)
Indice 202:     Soglia manuale (×10)
Indice 203:     Correzione temperatura (int8_t, ×10)
Indice 214:     Isteresi (×10)
Indice 215:     Eccezione abilitata (0/1)
Indice 216-219: Giorno/mese inizio, giorno/mese fine eccezione
Indice 220:     Banco eccezione
Indice 221:     Programma unico/settimanale (0=unico, 1=settimanale)
Indice 222:     Banco programma unico
Indice 223-229: Banco per ogni giorno settimana (dom-sab)

Caratteri 865-904: 4 nomi programma × 10 caratteri
```

### Librerie usate (vecchio progetto):
- AHT10 (enjoyneering)
- AsyncTCP + ESPAsyncWebServer
- ESP32Time
- SPIFFS (da sostituire con LittleFS)

### Protocollo WebSocket esistente:
```
TX dal server:
  "T 23.5"          - Temperatura
  "U 45"            - Umidità  
  "A " / "S "       - Acceso / Spento
  "D YYYY MM DD HH MM SS W" - Data/ora
  "B X"             - Banco selezionato
  "P00[...]"        - Programma completo
  Buffer binari     - Storico (marker 'T' o 'U' + dati)

RX dal client:
  "P00[...]"        - Salvataggio programma
```

---

## Architettura proposta per nuovo progetto

```
cronotermostato-s3/
├── platformio.ini
├── CLAUDE.md
├── src/
│   ├── main.cpp              # Setup e loop principale
│   ├── config.h              # Pin, costanti, configurazione
│   ├── thermostat_engine.h/cpp   # Logica termostato (dal vecchio)
│   ├── display_manager.h/cpp     # Gestione display + touch + UI
│   ├── web_server.h/cpp          # WebSocket server (dal vecchio)
│   ├── sensor_manager.h/cpp      # Lettura AHT10
│   ├── storage_manager.h/cpp     # LittleFS, salvataggio programmi
│   └── mqtt_client.h/cpp         # (Opzionale) Per accesso remoto
├── data/                     # File web UI (da vecchio progetto)
│   ├── index.html
│   ├── cronocode.js
│   ├── cronocss.css
│   └── ...
└── docs/
    └── pinout.md
```

### Librerie suggerite per ESP32-S3:
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_mode = dio
board_build.f_flash = 80000000L
board_build.f_cpu = 240000000L
board_build.partitions = default_16MB.csv

lib_deps = 
    moononournation/GFX Library for Arduino@^1.4.9
    ; oppure LVGL per UI più complessa
    enjoyneering/AHT10@^1.1.0
    mathieucarbou/ESPAsyncWebServer
    bblanchon/ArduinoJson@^7.0.0
    ; Per MQTT (opzionale):
    ; knolleary/PubSubClient@^2.8

build_flags = 
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

---

## Decisioni da prendere

1. **Libreria grafica**: Arduino_GFX (semplice) vs LVGL (potente ma complessa)
2. **UI sul display**: Definire quali schermate/controlli touch implementare
3. **MQTT**: Implementare per accesso remoto o solo LAN?
4. **Sensore**: Mantenere AHT10 o sostituire con SHT40?
5. **Framework**: Arduino puro (più veloce) o ESP-IDF (più controllo)?

---

## TODO

- [ ] Creare nuovo progetto PlatformIO per ESP32-S3
- [ ] Testare display/touch con esempio base
- [ ] Portare logica termostato
- [ ] Portare web server e WebSocket
- [ ] Implementare UI touch per display locale
- [ ] Migrare SPIFFS → LittleFS
- [ ] (Opzionale) Aggiungere MQTT per accesso remoto

---

## Note tecniche

### Inizializzazione display JC3248W535EN con Arduino_GFX:
```cpp
#include <Arduino_GFX_Library.h>

Arduino_DataBus *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);

void setup() {
    gfx->begin();
    gfx->fillScreen(BLACK);
    
    // Backlight
    pinMode(1, OUTPUT);
    digitalWrite(1, HIGH);
}
```

### Touch reading:
```cpp
#include <Wire.h>
#define TOUCH_ADDR 0x3B

Wire.begin(4, 8);  // SDA=4, SCL=8
// Lettura touch da implementare con protocollo AXS15231
```

---

## Riferimenti

- [GitHub JC3248W535EN-Touch-LCD](https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD)
- [F1ATB ESP32-S3 Display Setup](https://f1atb.fr/esp32-s3-3-5-inch-capacitive-touch-ips-display-setup/)
- [ESPHome config](https://community.home-assistant.io/t/jc3248w535-guition-3-5-config/791363)
