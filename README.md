<p align="center">
  <img src="./Logo/Smart_Filament_Sample_Board_Logo_480p.png" alt="Smart Filament Sample Board Logo" width="300">
</p>

<h1 align="center">Smart Filament Sample Board</h1>
<p align="center"><i>Spot My Filament — finde jedes Filament-Sample per NFC-Tag auf einen Blick.</i></p>

<p align="center">
  <img alt="License" src="https://img.shields.io/github/license/mark77-DE/Smart-Filament-Sample-Board">
  <img alt="Platform" src="https://img.shields.io/badge/platform-ESP32-blue">
  <img alt="Home Assistant" src="https://img.shields.io/badge/integration-Home%20Assistant-41BDF5">
</p>

---

## Inhaltsverzeichnis

<!-- - [Über das Projekt](#über-das-projekt) -->
- [Funktionsweise](#funktionsweise)
- [Benötigte Hardware](#benötigte-hardware)
- [Galerie](#galerie)
- [Erste Schritte](#erste-schritte)
- [Dokumentation](#dokumentation)
- [Mitmachen](#mitmachen)
- [Lizenz](#lizenz)


<!-- 
## Über das Projekt

TODO: 2-3 Sätze: Für wen ist das Board gedacht? Welches Problem löst es?
Beispiel:
Wer viele Filament-Sample-Spulen sammelt, kennt das Problem: Welches Sample war das
nochmal, und wo liegt es? Das Smart Filament Sample Board löst das mit NFC-Tags,
einem ESP32 und einer Home-Assistant-Anbindung — Tag scannen, LED zeigt den Lagerplatz. -->

## Funktionsweise

Workflow basierend auf einer eigenen JSON-Datenbank auf dem ESP32:

1. NFC-Tag wird über den NFC-Reader gelesen.
2. Der ESP32 fragt die Datenbank ab und stellt die Daten auf dem Display dar.
3. Die entsprechende LED leuchtet auf und zeigt den Lagerplatz des Samples an.
4. Ein Klick im WebIF zeigt ebenfalls, wo das Filament-Sample lagert.

Zusätzlich gibt es eine Anbindung an **Home Assistant** via HA-Discovery und MQTT:
Der ESP32 sendet das ausgewählte Filament (per NFC-Tag oder Klick im WebIF) an Home
Assistant. LEDs und Display lassen sich per HA ein-/ausschalten, der ESP übermittelt
den Status zurück.

## Benötigte Hardware

Siehe [Hardware-Dokumentation](./docs/hardware.md)



## Galerie

<p align="center">
  <img src="./pictures/board_complete.jpg" alt="Fertiges Board" width="480">
  <img src="./pictures/reader.jpg" alt="Display/Reader" width="265"><br>
  <sub>Fertig aufgebautes Board</sub>
</p>

<p align="center">
  <img src="./screenshots/dashboard.png" alt="Dashboard Daymode" width="45%">
  <img src="./screenshots/dashboard_highlighted.png" alt="Dashboard Highlighted" width="45%"><br>
  <sub>Home-Assistant-Dashboard: Übersicht (links) und markiertes Sample (rechts)</sub>
</p>

## Erste Schritte

Kurzfassung — die ausführliche Anleitung steht in [`docs/setup.md`](docs/setup.md).

1. Hardware gemäß [`docs/hardware.md`](docs/hardware.md) verdrahten.
2. Firmware aus [`ESP32-Webserver`](ESP32-Webserver) auf den ESP32 flashen (siehe [`docs/setup.md`](docs/setup.md)).
3. WLAN-Zugangsdaten konfigurieren.
4. Optional: MQTT und Home-Assistant-Integration aktivieren (Auto-Discovery).
5. NFC-Tags anlegen und Filament-Samples zuordnen, siehe [`docs/usage.md`](docs/usage.md).

## Dokumentation

Ausführliche Anleitungen findest du im [`docs/`](docs) Ordner:

| Dokument | Inhalt |
|---|---|
| [`docs/hardware.md`](docs/hardware.md) | Verkabelung, Pinbelegung, Schaltplan |
| [`docs/setup.md`](docs/setup.md) | Firmware flashen, Konfiguration, Home-Assistant-Setup |
| [`docs/usage.md`](docs/usage.md) | Alltägliche Bedienung, NFC-Tags anlegen, WebIF |
| [`docs/3d-print.md`](docs/3d-print.md) | 3D-Druckteile aus [`3D-Daten`](3D-Daten), Druckeinstellungen |

<!-- Sammlung offener Notizen/Ideen liegt aktuell in Notes/ – ggf. dort verlinken -->

## Mitmachen

Contributions, Issues und Feature-Wünsche sind willkommen!
Schau gerne im [Issues-Tab](https://github.com/mark77-DE/Smart-Filament-Sample-Board/issues) vorbei.

<!-- TODO: falls gewünscht, CONTRIBUTING.md ergänzen und hier verlinken -->

## Lizenz

Dieses Projekt steht unter der [GPL-3.0 Lizenz](LICENSE).
