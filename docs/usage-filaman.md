# Bedienung bei Anbindung an Filaman

Allgemeine Bedienschritte (WebIF-Übersicht, Home Assistant, Filtern/Sortieren)
stehen in [`usage.md`](usage.md). Hier geht es nur um das, was bei
Anbindung an FilaMan anders läuft als im lokalen Betrieb (siehe
[`usage-local.md`](usage-local.md)).

## NFC-Tags anlegen

1. Im Filaman WebIf die Seite Filamente öffnen.
2. Für jede Farbe, für die man ein Sample anlegen möchte, benötigt das Filament 2 filamentspezifische Extrafelder.

<p align="left">
  <img src="../screenshots/filaman_extrafields_1.png" alt="NFC Tag" width="320">
  <img src="../screenshots/filaman_extrafields_2.png" alt="NFC Tag" width="320">
</p>

3. Die Keys **müssen zwingend** `sampleboard_uid` und `sampleboard_led` sein.
4. `sampleboard_uid` bekommt die UID des Sample-NFC-Tags.
5. `sampleboard_led` bekommt die zugeordnete LED auf deinem Board.
6. Im FSB WebIf unter Einstellungen → Filaman die Zugangsdaten eintragen (vorher z. B. einen Nutzer mit nur Leseberechtigung in Filaman anlegen).
7. Im FSB WebIf unter Einstellungen → Sync Filaman ausführen. Das kann je nach DB-Größe (entscheidend ist die Anzahl der Filamente, nicht der Spulen) eine Weile dauern.

## Sample per NFC-Tag finden

NFC-Tag an den Leser halten, FSB zeigt auf dem Display Hersteller, Typ und Farbe aus den eben synchronisierten Daten und fragt zusätzlich bei Filaman den Lagerplatz ab.

Auch im WebIF fragt das FSB nach Auswahl eines Filaments die FilaMan-DB ab und aktualisiert den Lagerort dort live.

## Sample bearbeiten / löschen

Dazu passende UID im Filaman löschen und erneut synchronisieren.
