# Bedienung

Diese Seite beschreibt die Bedienschritte, die unabhängig davon gelten, ob
das Board mit der **lokalen DB** oder mit **FilaMan** arbeitet. Details zum
Anlegen und Finden von Samples in den beiden Modi stehen in
[`usage-local.md`](usage-local.md) bzw. [`usage-filaman.md`](usage-filaman.md).

## Sample über das WebIF finden

1. WebIF im Browser öffnen: `http://<ip-des-boards>/`
2. Filament in der Liste auswählen und anklicken.
3. Die passende LED am Board leuchtet auf, um den Sample-Lagerplatz zu markieren.

<p align="center">
  <img src="../screenshots/dashboard_highlighted.png" alt="Dashboard mit markiertem Sample" width="480">
</p>

## Steuerung über Home Assistant

- LEDs und Display lassen sich per Home Assistant ein-/ausschalten. (z.B. schalten per Bewegungssensor)
- Der ESP32 übermittelt den aktuellen Status (z. B. ausgewähltes Filament) an Home Assistant zurück. (z.B. Sprachausgabe)
<!-- TODO: Beispiel-Dashboard-Card / Automatisierung verlinken, falls vorhanden -->

## Sonstiges

<b>Filtern:</b> durch Eingabe von Filterwörtern kann gesucht werden, wird im Material Filter z.B. PLA ausgeählt, leuchten alle PLA Samples.
<b>Sortieren:</b> durch Klick auf den kategorienamen wird auf- bzw. absteigend gefiltert.
