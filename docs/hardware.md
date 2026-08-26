# Hardware & Verkabelung

Diese Seite beschreibt den Aufbau des Smart Filament Sample Boards.

## Stückliste

| Komponente | Menge | Hinweise | Links |
|---|---|---|---|
| ESP-32 Devkit V1 **oder** ESP32-S3 Super Mini | 1 | | |
| PN532 NFC-Reader | 1 | | |
| WS2812B LEDs | 4×3 (Standard) | für Hintergrundbeleuchtung des NFC Symbols,<br> laut KI bis 150 LEDs je nach eigenem Setup<br>Abstand der LEDs sollte 33mm betragen, also 30LEDs/m | |
| 1.3" I2C Display 128×64 (SH1106) **oder** 1.9" TFT ST7789 | 1 | | |
| Taster | optional | | |
| Buzzer | optional | | |
| Ntag215 | bis 150 | DieStückzahl sollte min. zur Anzahl der geplanten Samples passen | [AliExpress](https://de.aliexpress.com/item/1005006375989905.html) |
| Netzteil | 1 | <!-- TODO: Spannung/Stromstärke --> | |

## Pinbelegung

Die Pinbelegung ist, je nach verwendeten Controllertyp und Display, unterschiedlich.
Für die kompilierten Binaries gibt es feste Vorgaben, wer selbst kompiliert, kann das in der platformio.ini entsprechend modifizieren.

Für den weit verbreiteten Standard typ, dem ESP32 Devkit V1 gillt folgendes pinout:

### OLED Display variante

| ESP32 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 18 | SPI/SCK | NFC-Reader |
| GPIO 19 | SPI/MISO | NFC-Reader |
| GPIO 23 | SPI/MOSI | NFC-Reader |
| GPIO 26 | PN532 CS | NFC-Reader |
| GPIO 21 | SDA | OLED |
| GPIO 22 | SCL | OLED |
| GPIO 32 | Button | optional |
| GPIO 33 | Buzzer | optional |
| GPIO 4 | LEDs | Samples |
| GPIO 15 | LEDs | NFC-Symbol |

Für den ESP32-S3-Zero gillt folgendes Pinout

| ESP32 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 3 | SPI/SCK | NFC-Reader |
| GPIO 4 | SPI/MISO | NFC-Reader |
| GPIO 5 | SPI/MOSI | NFC-Reader |
| GPIO 6 | PN532 CS | NFC-Reader |
| GPIO 13 | SDA | OLED |
| GPIO 12 | SCL | OLED |
| GPIO 7 | Button | optional |
| GPIO 8 | Buzzer | optional |
| GPIO 2 | LEDs | Samples |
| GPIO 1 | LEDs | NFC-Symbol |



## Schaltplan / Wiring-Diagramm

<!-- TODO: Bild einbinden, z. B. aus Fritzing oder handgezeichnet
![Wiring Diagram](../pictures/wiring.png)
-->

## Aufbauhinweise

<!-- TODO: Besonderheiten beim Zusammenbau, z. B.:
- Reihenfolge der LED-Kette
- Stromversorgung bei vielen LEDs (separates Netzteil? Sicherung?)
- Gehäuse / mechanischer Aufbau, Verweis auf 3D-Daten
-->

Optimalfall: Das Netzteil sollte von der Leistung her zur Anzahl der LEDs passen wobei jede LED max. 60mA ziehen könnte..
Relaistisch (ohne Garantie): Durch die Limitierung der Brightness und der Tatsache, dass nie alle LEDs zeitgleich leuchten, kann das Netzteil auch kleiner ausgeführt werden. Für eine Testaufbau mit z.B. 16 LEDs reicht die Versorgung über edn ESP völlig aus.

Siehe auch: [`3d-print.md`](3d-print.md) für die gedruckten Gehäuseteile.
