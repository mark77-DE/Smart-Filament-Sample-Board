# firmware/

Hier liegen die kompilierten Binärdateien, auf die `manifest.json` verweist.

Diese Dateien werden hier NICHT mitgeliefert - sie müssen bei jedem Release
neu exportiert und hier abgelegt (bzw. per GitHub Release-Workflow erzeugt) werden:

- bootloader.bin
- partitions.bin
- boot_app0.bin
- firmware.bin

Siehe `docs/web-installer-setup.md` für die genaue Anleitung zum Export
aus der Arduino IDE.
