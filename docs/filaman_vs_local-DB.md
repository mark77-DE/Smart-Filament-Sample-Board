# Workflow: Lokale DB (filaments.json) vs. FilaMan-DB

Diese Seite beschreibt, wann das Smart Filament Sample Board auf die **lokale
`filaments.json`** (LittleFS) zugreift und wann es live gegen die
**FilaMan-API** abfragt — inklusive Sync-Mechanismus.

## Grundprinzip

| | Lokale DB (`filaments.json`) | FilaMan-DB (live) |
|---|---|---|
| Quelle | LittleFS auf dem ESP32 | FilaMan-Server (`server-ip:port`) |
| Inhalt | vendor, type, color, ledIndex, info1 (Bestand), info2 (Shop-URL) | Alle Felder inkl. `storage` (Lagerort) |
| Aktualisierung | Manuelle Pflege per WebIF | Immer aktuell (Live-Abfrage) |
| Geschwindigkeit | Sehr schnell (lokal) | Abhängig von Netzwerk/API-Latenz |
| Warum getrennt? | Stabile Stammdaten, selten geändert | Lagerort ändert sich häufig (AMS/Bambuddy-Zuordnung) und wäre in lokaler DB schnell veraltet |

Der Lagerort (`storage`) wird bewusst **nicht** synchronisiert, sondern immer
live abgefragt, da er sich zu oft ändert.

## Workflow 1: NFC-Scan am Sample Board (Nutzung)

```mermaid
flowchart TD
    A[Nutzer scannt NFC-Tag am Sample Board] --> B{sampleboard_uid bekannt?}
    B -- "Nein" --> Z[Fehleranzeige: UID nicht gefunden]
    B -- "Ja, in lokaler DB" --> C[Lade Stammdaten aus filaments.json:<br/>vendor, type, color, ledIndex, info1, info2]
    C --> D[LED am ledIndex ansteuern]
    C --> E{Netzwerk verfügbar?}
    E -- "Nein" --> F[Anzeige nur mit Stammdaten<br/>Lagerort: 'nicht verfügbar']
    E -- "Ja" --> G[Live-Query an FilaMan:<br/>filament_id -> zugehörige Spulen -> Lagerorte]
    G --> H[Lagerorte aggregieren<br/>sortiert nach Restbestand]
    H --> I[Vollständige Anzeige:<br/>Stammdaten + aktueller Lagerort]
```

**Kernidee:** Die lokale DB liefert sofort die stabilen Anzeigedaten und
steuert die LED, unabhängig vom Netzwerk. Der Lagerort wird nur bei
Bedarf live nachgeladen und ergänzt die Anzeige, sobald er verfügbar ist.

## Workflow 2: Sync der lokalen DB (Wartung)

```mermaid
flowchart TD
    A[Nutzer löst Sync aus<br/>WebIF-Button oder Hardware-Taste] --> B[Login an FilaMan-API<br/>POST /auth/login -> Cookie session_id]
    B --> C[GET /api/v1/filaments<br/>alle Filamente mit gesetztem sampleboard_uid]
    C --> D[Für jedes Filament Custom-Field<br/>sampleboard_led auslesen -> ledIndex]
    D --> E[Für jedes Filament zugehörige Spulen<br/>abfragen: Anzahl + Restgewicht summieren]
    E --> F[Feld-Mapping:<br/>vendor = manufacturer.name<br/>type = material_type<br/>color = manufacturer_color_name<br/>info1 = 'X Spulen mit Y g Restgewicht'<br/>info2 = shop_url]
    F --> G[filaments.json auf LittleFS schreiben/überschreiben]
    G --> H[Sync abgeschlossen -> Bestätigung im WebIF]
```

**Wichtig:** Der Sync läuft **nicht automatisch** bei jedem Boot, sondern nur
auf explizite Nutzeranforderung, um unnötige API-Last und Inkonsistenzen zu
vermeiden.

## Entscheidungslogik im Code (Kurzfassung)

```mermaid
flowchart LR
    A[UID gescannt] --> B[Lookup in filaments.json]
    B --> C[Stammdaten + LED sofort]
    C --> D{Storage-Info gewünscht?}
    D -- "Ja" --> E[Live-Call FilaMan]
    D -- "Nein / offline" --> F[Fertig ohne Lagerort]
    E --> G[Fertig mit Lagerort]
```

## Offene Punkte

- Workaround für den FilaMan-Bug bei der Custom-Field-Suche auf
  `/api/v1/filaments` bleibt bestehen, bis der Fix im Upstream-Projekt
  gemergt ist.
