# Bedienung mit lokaler DB

Allgemeine Bedienschritte (WebIF-Übersicht, Home Assistant, Filtern/Sortieren)
stehen in [`usage.md`](usage.md). Hier geht es nur um das, was beim Betrieb
mit der lokalen `filaments.json` anders läuft als bei der FilaMan-Anbindung
(siehe [`usage-filaman.md`](usage-filaman.md)).

## NFC-Tags anlegen

1. Im WebIf die Seite Settings öffnen
2. NFC-Tag an den Reader halten, der Tag wird automatisch eingetragen
<p align="left">
  <img src="../screenshots/settings_main_add-tag.png" alt="NFC Tag" width="320">
</p>
3. die restlichen Informationen zum Sample eintragen, <b>UID, name, color, material und LED sind Pflichtfelder</b>, der Rest ist optional<br>
4. Es können nur LEDs ausgewählt werden, die noch nicht vergeben sind. Wird eine LED ausgewählt, leuchtet diese entsprechend auf dem Board kurz auf zur Orientierung.<br>
<p align="left">
  <img src="../pictures/led_highlight.jpg" alt="LED highlight" width="320">
</p>

## Sample per NFC-Tag finden

1. NFC-Tag an den Reader halten.
2. Der ESP32 fragt die Datenbank ab und zeigt die Filament-Infos auf dem Display an.
3. Die LED am zugehörigen Sample-Lagerplatz leuchtet auf.
4. Die entsprechende Kachel im WebIF leuchtet auch auf.

## Sample bearbeiten / löschen

Um ein Sample zu bearbeiten oder zu löschen, muss der Schalter "bearbeiten" aktiv sein
<!-- TODO: Ablauf im WebIF genauer beschreiben -->
