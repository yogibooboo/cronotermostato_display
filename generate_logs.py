#!/usr/bin/env python3
"""
Generatore di file di log binari per cronotermostato
Formato: header (12 bytes) + records (12 bytes ciascuno - history_sample_t)
"""

import struct
import math
import random
from datetime import datetime, timedelta

def generate_day_log(year, month, day, output_file, base_pressure=1013):
    """
    Genera un file di log giornaliero con dati simulati realistici
    """

    # Header
    magic = b'TLOG'
    version = 1
    num_samples = 1440  # 1 sample/minuto per 24 ore
    reserved = 0

    # Scrivi header (12 bytes)
    with open(output_file, 'wb') as f:
        f.write(magic)                              # 4 bytes: magic
        f.write(struct.pack('B', version))          # 1 byte: version
        f.write(struct.pack('<H', year))            # 2 bytes: anno (little endian)
        f.write(struct.pack('B', month))            # 1 byte: mese
        f.write(struct.pack('B', day))              # 1 byte: giorno
        f.write(struct.pack('<H', num_samples))     # 2 bytes: num samples
        f.write(struct.pack('B', reserved))         # 1 byte: reserved

        # Genera 1440 campioni (uno al minuto)
        for minute in range(num_samples):
            hour = minute // 60
            min_of_hour = minute % 60

            # Simula temperatura realistica seguendo programma
            # Notte (00:00-07:00): 16-17°C
            # Mattina (07:00-09:00): salita graduale a 21°C
            # Giorno (09:00-17:00): 20-21°C
            # Sera (17:00-22:00): discesa graduale a 18°C
            # Notte (22:00-24:00): discesa a 16°C

            if hour < 7:
                # Notte: temperatura bassa e stabile
                base_temp = 16.0
                variation = 0.3
            elif hour < 9:
                # Risveglio: salita graduale
                progress = (hour - 7) * 60 + min_of_hour
                base_temp = 16.0 + (progress / 120.0) * 5.0  # salita lineare a 21°C
                variation = 0.5
            elif hour < 17:
                # Giorno: temperatura target alta
                base_temp = 20.5
                variation = 0.5
            elif hour < 22:
                # Sera: discesa graduale
                progress = (hour - 17) * 60 + min_of_hour
                base_temp = 20.5 - (progress / 300.0) * 2.5  # discesa a 18°C
                variation = 0.4
            else:
                # Pre-notte: continua discesa
                progress = (hour - 22) * 60 + min_of_hour
                base_temp = 18.0 - (progress / 120.0) * 2.0  # discesa a 16°C
                variation = 0.3

            # Aggiungi rumore realistico
            temp = base_temp + random.uniform(-variation, variation)

            # Simula isteresi caldaia (ON quando sotto setpoint - isteresi)
            setpoint = base_temp + 0.5
            hysteresis = 0.5

            # Stato caldaia basato su temperatura e setpoint
            if temp < (setpoint - hysteresis):
                heater_on = 1
            elif temp > setpoint:
                heater_on = 0
            else:
                # Zona isteresi: mantieni stato precedente con probabilità
                heater_on = 1 if random.random() > 0.7 else 0

            # Umidità: inversamente correlata con temperatura e caldaia
            # Più alta di notte, più bassa quando la caldaia è accesa
            base_humidity = 55
            if heater_on:
                humidity = base_humidity - 8 + random.randint(-3, 2)
            else:
                humidity = base_humidity + random.randint(-2, 5)

            # Variazione giornaliera umidità (più alta di notte)
            if hour < 7 or hour > 22:
                humidity += 5

            humidity = max(30, min(70, humidity))  # Clamp 30-70%

            # Pressione: varia lentamente durante il giorno con trend sinusoidale
            # Range tipico: 980-1040 hPa
            pressure_trend = math.sin(minute / 1440.0 * 2 * math.pi) * 10  # +/- 10 hPa
            pressure = int(base_pressure + pressure_trend + random.randint(-2, 2))
            pressure = max(950, min(1050, pressure))  # Clamp 950-1050

            # Converti temperatura e setpoint in centesimi (int16)
            temp_centesimi = int(round(temp * 100))
            setpoint_centesimi = int(round(setpoint * 100))

            # Flags byte (bit 0: relay_on)
            flags = 0x01 if heater_on else 0x00

            # Banco attivo (simula rotazione tra i 4 programmi)
            active_bank = (hour // 6) % 4  # Cambia banco ogni 6 ore

            # Scrivi record (12 bytes - history_sample_t)
            f.write(struct.pack('<H', minute))           # 2 bytes: minute_of_day
            f.write(struct.pack('<h', temp_centesimi))   # 2 bytes: temp x100 (signed)
            f.write(struct.pack('B', humidity))          # 1 byte: humidity
            f.write(struct.pack('B', flags))             # 1 byte: flags
            f.write(struct.pack('<h', setpoint_centesimi)) # 2 bytes: setpoint x100
            f.write(struct.pack('B', active_bank))       # 1 byte: active_bank
            f.write(struct.pack('B', 0))                 # 1 byte: reserved
            f.write(struct.pack('<H', pressure))         # 2 bytes: pressure hPa

    print(f"Generated: {output_file}")
    print(f"  Size: {12 + num_samples * 12} bytes")
    print(f"  Date: {year}-{month:02d}-{day:02d}")
    print(f"  Samples: {num_samples}")

def main():
    """Genera log per 10 giorni intorno a oggi (5 prima + oggi + 4 dopo)"""

    # Data di oggi
    today = datetime.now()

    # Genera log per 10 giorni: da -5 a +4
    for i in range(-5, 5):
        date = today + timedelta(days=i)
        filename = f"log_{date.year}{date.month:02d}{date.day:02d}.bin"
        # Varia la pressione base per ogni giorno (simula variazioni meteo)
        base_pressure = 1013 + i * 3 + random.randint(-5, 5)
        generate_day_log(date.year, date.month, date.day, filename, base_pressure)
        print()

if __name__ == "__main__":
    main()
