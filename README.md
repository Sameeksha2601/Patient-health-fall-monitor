# IoT-Based Patient Health & Fall Monitoring System

A low-cost embedded system that continuously monitors a patient's body
temperature and detects sudden falls in real time, using an ESP32,
a DS18B20 temperature sensor, and an MPU6050 accelerometer. Alerts are
raised locally (buzzer + LCD) and reported to a ThingSpeak cloud dashboard
for remote monitoring by a caregiver.

Built and simulated end-to-end in Wokwi, with a full project report
documenting the design, architecture, and results (see
`Health_Monitoring_Project_Report.docx`).

## Architecture

```
 ESP32 (Wokwi simulator)
   - DS18B20 temperature sensor
   - MPU6050 accelerometer (fall detection)
        |
        |  HTTP (WiFi)
        v
 ThingSpeak Cloud
   - Field 1: Temperature
   - Field 2: Fall Status (0/1)
   - Field 3: Status code (0=OK, 1=Fall, 2=Fever, 3=Low temp)
   - Live gauge + line charts
        |
        v
 Local alerts: Buzzer + 16x2 I2C LCD (immediate, no network dependency)
```

## Files

| File | Purpose |
|---|---|
| `health_monitor_sketch.ino` | ESP32 firmware: reads sensors, runs fall-detection and temperature-threshold logic, drives local alerts, uploads to ThingSpeak. |
| `Health_Monitoring_Project_Report.docx` | Full formal project report - abstract, literature survey, architecture, methodology, results, applications, future scope, references. |

## Setup

1. Open [wokwi.com](https://wokwi.com) and create a new ESP32 project.
2. Add a **DS18B20** (with a 4.7kohm pull-up resistor between its data line and 3.3V), an **MPU6050**, a **buzzer**, and optionally a **16x2 I2C LCD**.
3. Wire as commented at the top of `health_monitor_sketch.ino`:
   - DS18B20 data -> GPIO 4
   - MPU6050 SDA -> GPIO 21, SCL -> GPIO 22
   - Buzzer -> GPIO 25
4. Paste in `health_monitor_sketch.ino` and add these libraries via the Library Manager: `OneWire`, `DallasTemperature`, `Adafruit MPU6050`, `Adafruit Unified Sensor`, `LiquidCrystal_I2C`.
5. Create a free channel at [thingspeak.com](https://thingspeak.com) with 3 fields (Temperature, Fall Status, Status), copy its **Write API Key**, and set it in the code (`THINGSPEAK_API_KEY`).
6. Run the simulation and watch both the Serial Monitor and the ThingSpeak dashboard update live.

**Security note:** the ThingSpeak Write API Key is a secret credential. It has been replaced with a `YOUR_WRITE_API_KEY` placeholder in the committed code - do not commit real keys to a public repository.

## Fall detection logic

Uses a two-stage acceleration-magnitude check rather than a single threshold, to reduce false positives:

1. **Free-fall phase**: total acceleration magnitude drops below ~0.4g (near weightlessness).
2. **Impact phase**: if a free-fall phase was just seen, and acceleration then spikes above the impact threshold, a fall is confirmed.

A fall event is brief (often a single sensor reading), while cloud uploads happen on a fixed interval (15s, per ThingSpeak's free-tier rate limit). To avoid losing a fall event that occurs between upload cycles, the fall flag is **latched**: once a fall is detected it stays flagged as `1` until it has been successfully reported to ThingSpeak, then resets.

## Results

Validated in simulation by manually driving sensor values in Wokwi:
- Fever (temperature > 38°C) correctly triggers `FEVER ALERT`, buzzer, and status code 2.
- Hypothermia (temperature < 35°C) correctly triggers `LOW TEMP ALERT` and status code 3.
- A simulated fall (acceleration dropping near 0g, then spiking) correctly triggers `FALL DETECTED`, the buzzer, Fall Status = 1, and status code 1.
- All conditions correctly return to `OK` / status code 0 once readings return to normal.

## Applications

- Elderly care and assisted living facilities
- Post-operative home patient monitoring
- Remote monitoring in rural healthcare settings
- Rehabilitation monitoring for patients with mobility impairments
- GSM/SMS alerting as a fallback when WiFi is unavailable
- ML-based fall detection trained on labeled fall datasets, instead of fixed thresholds
- Battery-level monitoring and low-power sleep modes for real-world deployment

See `Health_Monitoring_Project_Report.docx` for full details, including the literature survey, block diagram, and discussion of limitations.
