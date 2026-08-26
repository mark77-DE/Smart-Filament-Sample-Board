# Setup & Konfiguration

Diese Anleitung beschreibt, wie die Firmware aufgespielt und das Board eingerichtet wird.

## Voraussetzungen

<!-- TODO: zutreffendes auswählen/anpassen -->

- Arduino IDE (Version x.x) **oder** PlatformIO
- ESP32 Board-Package installiert
- Folgende Libraries:
  - <!-- TODO: z. B. Adafruit_PN532 -->
  - <!-- TODO: z. B. FastLED / Adafruit_NeoPixel -->
  - <!-- TODO: z. B. U8g2 (Display) -->
  - <!-- TODO: z. B. PubSubClient (MQTT) -->
  - <!-- TODO: z. B. ArduinoJson -->

## 1. Repository klonen

```bash
git clone https://github.com/mark77-DE/Smart-Filament-Sample-Board.git
cd Smart-Filament-Sample-Board/ESP32-Webserver
```

## 2. Konfiguration anpassen

<!-- TODO: Beschreibe, welche Datei angepasst werden muss, z. B. config.h -->

```cpp
// Beispiel – Datei- und Variablennamen an den tatsächlichen Code anpassen
#define WIFI_SSID     "dein-wlan"
#define WIFI_PASSWORD "dein-passwort"

#define MQTT_HOST     "192.168.x.x"
#define MQTT_PORT     1883
#define MQTT_USER     "..."
#define MQTT_PASSWORD "..."
```

## 3. Firmware flashen

<!-- TODO: konkrete Schritte für Arduino IDE oder PlatformIO
Beispiel PlatformIO:
```bash
pio run -t upload
```
-->

## 4. Home-Assistant-Integration

Das Board meldet sich per **MQTT Auto-Discovery** bei Home Assistant an.

1. MQTT-Broker in Home Assistant einrichten (falls noch nicht vorhanden).
2. Board neu starten — die Entitäten erscheinen automatisch unter **Einstellungen → Geräte & Dienste → MQTT**.
3. <!-- TODO: welche Entitäten werden angelegt (Sensoren, Switches, Lights)? -->

## 5. Erste Inbetriebnahme prüfen

<!-- TODO: Woran erkennt man, dass alles funktioniert?
z. B. Display zeigt Startbildschirm, LED-Testlauf, WebIF unter http://<ip>/ erreichbar -->

## Troubleshooting

<!-- TODO: häufige Probleme, z. B.
- NFC-Reader wird nicht erkannt → Verkabelung/Adresse prüfen
- Keine MQTT-Verbindung → Broker-IP/Zugangsdaten prüfen
-->
