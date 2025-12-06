# Smart-Filament-Sample-Board

Webserver 




Workflow basierend auf Homeassistant:

NFC Tag mit Homeassistant HandyApp in Homeassitant erstellen.
NFC Tag wird vom Handy gelesen und in Homeassitant verarbeitet.
Homeassitant sendet hinterlegte Daten, z.B.: "esun PLA+ gelb" an ESPHOME Display


Worklfow basierend auf eigener Datenbank:

NFC-Tag wird über den NFC-Reader gelesen
ESP32 Fragt Datenbank ab und stellt Daten auf Display dar.

Für beide ggf. Anbindung an Spoolman zum ANzeigen möglicher Bestände
