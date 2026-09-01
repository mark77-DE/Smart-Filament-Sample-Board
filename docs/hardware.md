# Hardware & Verkabelung

Diese Seite beschreibt den Aufbau des Smart Filament Sample Boards.

## Stückliste

| Komponente | Menge | Hinweise | Links |
|---|---|---|---|
| ESP-32 Devkit V1 **oder** ESP32-S3 Super Mini | 1 | | |
| PN532 NFC-Reader | 1 | | |
| WS2812B **oder**<br>SK6812 **oder**<br>SK6812W LEDs | 4×3 (Standard) NFC-Symbol<br>Plus Anzahl Samples | für Hintergrundbeleuchtung des NFC Symbols,<br> laut KI bis 150 LEDs je nach eigenem Setup<br>Abstand der LEDs sollte 33mm betragen, also 30LEDs/m | |
| 1.3" I2C Display 128×64 (SH1106 **oder** SSD1306) **oder** 1.9" TFT ST7789 | 1 | | |
| Taster | optional | | |
| Buzzer | optional | | |
| Ntag215 | bis 150 | DieStückzahl sollte min. zur Anzahl der geplanten Samples passen | [AliExpress](https://de.aliexpress.com/item/1005006375989905.html) |
| Netzteil | 1 | <!-- TODO: Spannung/Stromstärke --> | |

## Pinbelegung

Die Pinbelegung ist, je nach verwendeten Controllertyp und Display, unterschiedlich.
Für die kompilierten Binaries gibt es feste Vorgaben, wer selbst kompiliert, kann das in der platformio.ini entsprechend modifizieren.

Für den weit verbreiteten Standard typ, dem ESP32 Devkit V1 gillt folgendes pinout:

### ESP32 OLED Variante
| ESP32 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 4 | LEDs | Samples |
| GPIO 15 | LEDs | NFC-Symbol |
| GPIO 18 | SPI/SCK | NFC-Reader |
| GPIO 19 | SPI/MISO | NFC-Reader |
| GPIO 21 | SDA | OLED |
| GPIO 22 | SCL | OLED |
| GPIO 23 | SPI/MOSI | NFC-Reader |
| GPIO 26 | PN532 CS | NFC-Reader |
| GPIO 32 | Button | optional |
| GPIO 33 | Buzzer | optional |

### ESP32-S3 OLED Variante
*(getestet mit ESP32-S3 Zero)*

| ESP32-S3 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 1 | LEDs | NFC-Symbol |
| GPIO 2 | LEDs | Samples |
| GPIO 3 | SPI/SCK | NFC-Reader |
| GPIO 4 | SPI/MISO | NFC-Reader |
| GPIO 5 | SPI/MOSI | NFC-Reader |
| GPIO 6 | PN532 CS | NFC-Reader |
| GPIO 7 | Button | optional |
| GPIO 8 | Buzzer | optional |
| GPIO 12 | SCL | OLED |
| GPIO 13 | SDA | OLED |

### ESP32 ST7789 Variante
| ESP32 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 4 | LEDs | Samples |
| GPIO 5 | TFT CS | ST7789 Display |
| GPIO 12 | NFC_MISO | NFC-Reader |
| GPIO 13 | NFC_MOSI | NFC-Reader |
| GPIO 14 | NFC_SCK | NFC-Reader |
| GPIO 15 | LEDs | NFC-Symbol |
| GPIO 26 | PN532 CS | NFC-Reader |
| GPIO 27 | TFT RST | ST7789 Display |
| GPIO 32 | TFT DC | ST7789 Display |
| GPIO 33 | Buzzer | optional |

### ESP32 ST7789 Variante
| ESP32 Pin | Verbunden mit | Funktion |
|---|---|---|
| GPIO 4 | LEDs | Samples |
| GPIO 5 | TFT CS | ST7789 Display |
| GPIO 12 | NFC_MISO | NFC-Reader |
| GPIO 13 | NFC_MOSI | NFC-Reader |
| GPIO 14 | NFC_SCK | NFC-Reader |
| GPIO 15 | LEDs | NFC-Symbol |
| GPIO 25 | Button | optional |
| GPIO 26 | PN532 CS | NFC-Reader |
| GPIO 27 | TFT RST | ST7789 Display |
| GPIO 32 | TFT DC | ST7789 Display |
| GPIO 33 | Buzzer | optional |
| – | TFT MISO | nicht verbunden |



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
