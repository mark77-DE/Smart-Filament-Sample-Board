<img width="1920" height="750" alt="Smart_Filament_Sample_Board_Logo" src="https://github.com/user-attachments/assets/37c89a3e-e5f9-414a-b1f2-c0bbddc41e5f" />
# Smart-Filament-Sample-Board

Worklfow basierend auf eigener JSON-Datenbank auf dem ESP32:

NFC-Tag wird über den NFC-Reader gelesen
ESP32 Fragt Datenbank ab und stellt Daten auf Display dar.

Entsprechende LED wird dargestellt um den Lagerplatz des Samples anzuzeigen.

Ein Klick im WebIf zeigt, wo das Filament-Sample lagert.

Anbindung an Homeassistant via HA-Discovery und MQTT. Der ESP sendet das ausgewählte Filament (NFC Reader oder Klick im WebIF) an Homeassistant.
Die LEDs und das Display können per HA ein- und ausgeschaltet werden bzw. übermittelt der ESP den Status an Homeassistant.

