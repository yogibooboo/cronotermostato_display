# Cronotermostato ESP32-S3 con Display Touch

Cronotermostato programmabile basato su ESP32-S3 con display touch integrato (modulo JC3248W535EN).

## Hardware

### Modulo principale: JC3248W535EN (Guition)
- **MCU**: ESP32-S3-WROOM-1 (dual-core 240MHz)
- **Display**: 3.5" IPS 320x480, controller AXS15231B, interfaccia QSPI
- **Touch**: Capacitivo I2C (indirizzo 0x3B)
- **Memoria**: 16MB Flash, 8MB PSRAM
- **Connettività**: WiFi + Bluetooth integrati
- **Extra**: Slot SD card, ~12 GPIO disponibili

### Sensori e Attuatori
- Sensore temperatura/umidità: AHT10 (I2C, 0x38)
- Relè controllo caldaia
- LED status WiFi (GPIO 6, active LOW)

## Funzionalità

### Implementate ✅
- [x] Setup WiFi con reconnection automatica
- [x] Gestione WebSocket clients
- [x] LED status connessione
- [x] Task FreeRTOS dual-core
- [x] Diagnostica sistema (heap, PSRAM, uptime)

### In Sviluppo 🚧
- [ ] Sincronizzazione tempo NTP
- [ ] 4 programmi orari configurabili
- [ ] Programmazione settimanale
- [ ] Eccezioni (vacanze)
- [ ] Storico temperatura/umidità (365 giorni)
- [ ] Interfaccia web di controllo
- [ ] UI touch locale su display
- [ ] Console CLI per debug
- [ ] Server Telnet

## Configurazione

### 1. Installazione

```bash
git clone https://github.com/yourusername/cronotermostato_display.git
cd cronotermostato_display
```

### 2. Credenziali WiFi

Copia il file template e inserisci le tue credenziali:

```bash
cp include/credentials.h.example include/credentials.h
```

Modifica `include/credentials.h`:
```c
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
```

### 3. Build e Upload

Con PlatformIO:
```bash
pio run --target upload
pio device monitor
```

## Struttura Progetto

```
cronotermostato_display/
├── platformio.ini           # Configurazione PlatformIO
├── sdkconfig.esp32-s3-devkitc-1  # Configurazione ESP-IDF
├── partitions.csv           # Schema partizioni flash
├── CLAUDE.md               # Documentazione progetto
├── README.md
├── include/
│   ├── comune.h            # Strutture dati e definizioni
│   ├── wifi.h              # WiFi e WebSocket
│   └── credentials.h.example
├── src/
│   ├── main.cpp            # Entry point applicazione
│   └── wifi.cpp            # Gestione WiFi
└── data/                   # File web UI (futuro)
```

## Pinout ESP32-S3

### Display QSPI
- CS: GPIO 45
- CLK: GPIO 47
- D0: GPIO 21
- D1: GPIO 48
- D2: GPIO 40
- D3: GPIO 39
- Backlight: GPIO 1

### Touch I2C
- SDA: GPIO 4
- SCL: GPIO 8
- RST: GPIO 12
- INT: GPIO 11
- Addr: 0x3B

### Sensore AHT10
- SDA: GPIO 4 (condiviso con touch)
- SCL: GPIO 8 (condiviso con touch)
- Addr: 0x38

### Attuatori
- Relè caldaia: GPIO 19
- LED WiFi: GPIO 6 (active LOW)

## Architettura Software

Progetto basato su ESP-IDF 5.5.0 con architettura modulare:

- **WiFi Manager**: Gestione connessione e reconnection automatica
- **WebSocket Server**: Comunicazione real-time con web UI
- **Thermostat Engine**: Logica controllo temperatura con isteresi
- **History Manager**: Storico dati su SPIFFS (file giornalieri)
- **Storage Manager**: Configurazione persistente
- **Display Manager**: UI locale su display touch (futuro)

### Task FreeRTOS
- **Core 0**: Thermostat task (priorità 5) - controllo temperatura
- **Core 1**: Diagnostics task (priorità 1) - monitoraggio sistema

## Memoria

Utilizzo attuale:
- **RAM**: ~11.5% (37 KB / 320 KB)
- **Flash**: ~36% (759 KB / 2 MB)
- **PSRAM**: 8 MB disponibili per storico dati

## Riferimenti

- [JC3248W535EN Touch LCD](https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD)
- [ESP32-S3 Display Setup](https://f1atb.fr/esp32-s3-3-5-inch-capacitive-touch-ips-display-setup/)
- [ESPHome Config](https://community.home-assistant.io/t/jc3248w535-guition-3-5-config/791363)

## Licenza

[Specifica la tua licenza qui]

## Autore

[Il tuo nome/username]
