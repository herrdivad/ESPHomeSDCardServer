# Arbeitsplan: zuverlässiger OTA-Dateiserver für die CYD-SD-Karte

## 1. Zweck dieses Dokuments

Dieses Dokument ist die vollständige Übergabe an eine neue Codex-, Claude-Code- oder menschliche Entwicklungsinstanz. Die Umsetzung soll als **Clean-Room-Neubau** erfolgen und darf weder funktional noch strukturell vom vorhandenen, fehlerhaften Repository abhängen.

Der neue Code soll Daten zuverlässig über WLAN auf die SD-Karte eines FNK0104S mit ESP32-S3R8 übertragen, Dateien auflisten, herunterladen, löschen und Verzeichnisse anlegen können. Fehler müssen sichtbar, überprüfbar und korrekt an den Client zurückgegeben werden.

Der Schwerpunkt liegt zunächst ausschließlich auf der tatsächlich eingesetzten Plattform:

- Board-Familie: FNK0104S / CYD
- MCU: ESP32-S3R8
- Framework: ESP-IDF über ESPHome
- PSRAM: 8 MB Octal-PSRAM, 80 MHz
- SD-Anbindung: SDMMC, voraussichtlich 4-Bit
- Netzwerk: WLAN über ESPHome
- Normaler ESPHome-Webserver: Port 80
- Neuer SD-Dateiserver: eigener Port 8080

Der erste Release soll ausdrücklich **keine Arduino-Kompatibilität** versprechen. Unterstützung weiterer Boards oder Frameworks darf erst dokumentiert werden, nachdem sie auf echter Hardware getestet wurde.

---

## 2. Bekannte Ausgangslage

### 2.1 Aktuelle, funktionierende ESPHome-Basis

Die vom Nutzer übermittelte Konfiguration enthielt durch das Einfügen verlorene Zeilenumbrüche (`80MHz ota:` und `port: 80#`). Bereinigt lautet die relevante Basis:

```yaml
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf
    advanced:
      include_builtin_idf_components:
        - fatfs
        - spiffs
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"

# Das FNK0104S verwendet einen ESP32-S3R8 mit Octal-PSRAM.
psram:
  mode: octal
  speed: 80MHz

ota:
  - platform: esphome

web_server:
  port: 80
```

`web_server_base:` ist für den Neubau keine Abhängigkeit und soll nicht ausdrücklich konfiguriert werden, sofern die konkret installierte ESPHome-Version es nicht aus einem anderen Grund verlangt. Der offizielle `web_server:` lädt seine interne Basis selbst. Der neue SD-Server benutzt diesen Webserver bewusst nicht.

### 2.2 Bekannte SDMMC-Pins

Die gegenwärtig vorgesehenen Pins sind:

| Signal | GPIO |
|---|---:|
| CLK | GPIO38 |
| CMD | GPIO40 |
| DATA0 | GPIO39 |
| DATA1 | GPIO41 |
| DATA2 | GPIO48 |
| DATA3 | GPIO47 |

Diese Belegung deutet auf SDMMC im 4-Bit-Modus hin. Sie ist vor der eigentlichen HTTP-Implementierung auf der konkreten FNK0104S-Hardware zu verifizieren.

Zu prüfen:

- Sind alle sechs Leitungen tatsächlich mit dem SD-Slot verbunden?
- Besitzt das Board ausreichende externe Pull-ups an CMD und DAT0–DAT3?
- Sind die GPIOs nicht gleichzeitig durch Display, Touch, Backlight oder andere Komponenten belegt?
- Funktioniert 4-Bit stabil? Nur bei nachgewiesenen Hardwareproblemen darf testweise auf 1-Bit mit DATA0 reduziert werden.
- Liefert der Slot 3,3 V stabil, insbesondere während Schreibspitzen?

Interne Pull-ups dürfen höchstens ergänzend aktiviert werden. Sie ersetzen keine für SDMMC geeigneten externen Pull-ups.

### 2.3 Vor Beginn festzuhaltende Versionen

Die neue Instanz muss vor dem Programmieren die reale Build-Umgebung protokollieren:

```bash
esphome version
```

Zusätzlich sind aus dem Build-Log festzuhalten:

- ESPHome-Version
- ESP-IDF-Version
- Zielchip und Board
- Compiler-Version
- verwendete Partitionstabelle

Die Entwicklung wird zunächst auf genau diese Versionen festgelegt. Ein Versionsbereich darf erst nach separaten Build- und Hardwaretests angegeben werden.

---

## 3. Verbindliche Architekturentscheidung

### 3.1 Kein ESPHome-AsyncWebHandler für den Dateitransfer

Der neue Dateiserver darf für Upload, Download und Delete **nicht** von ESPHomes internem `web_server_base`, `AsyncWebHandler`, `handleUpload()`, Multipart-Unterstützung oder `USE_WEBSERVER_OTA` abhängen.

Grund: Diese Schicht hat sich zwischen ESPHome-Versionen verändert, unterstützt unter ESP-IDF nur eine Teilmenge der erwarteten Methoden und koppelt Multipart-Verarbeitung an Firmware-OTA. Genau diese impliziten Abhängigkeiten sollen ausgeschlossen werden.

### 3.2 Eigener nativer ESP-IDF-HTTP-Server

Der External Component startet einen zweiten HTTP-Server direkt mit ESP-IDFs `esp_http_server`:

- ESPHome-Webserver: Port 80, unverändert
- SD-Dateiserver: Port 8080, konfigurierbar
- Eigener `ctrl_port`, zum Beispiel 32769, damit er nicht mit dem ersten HTTPD-Server kollidiert
- Eigene URI-Handler für GET, POST, DELETE und OPTIONS
- Direkter Zugriff auf `httpd_req_recv()` für Upload-Streaming
- Direkter Zugriff auf `httpd_resp_send_chunk()` für Download-Streaming

Damit bleibt Firmware-OTA über `platform: esphome` vollständig unabhängig vom SD-Dateitransfer.

### 3.3 Sprache

Wenn ESPHome beibehalten wird, muss der Laufzeitteil auf dem Mikrocontroller C++ sein:

- Python im External Component: YAML-Schema, Validierung und C++-Codegenerierung
- C++ auf dem ESP32: SDMMC, FATFS, HTTP-Server und Dateizugriffe
- HTML/CSS/JavaScript: Browseroberfläche
- Optional Python oder C# auf dem PC: CLI-Uploader und Tests

Python oder C# können den PC-Client implementieren, aber nicht ohne Wechsel des gesamten Firmware-Ökosystems den ESPHome-Laufzeitteil ersetzen. MicroPython oder .NET nanoFramework wären ein vollständiger Abschied von ESPHome und sind nicht Teil dieses Projekts.

---

## 4. Neues Repository und Dateistruktur

Ein neues Repository beziehungsweise ein vollständig neuer Quellordner ist anzulegen. Keine C++-Dateien aus dem alten Repository kopieren.

Vorgeschlagene Struktur:

```text
cyd-sd-http-server/
├── README.md
├── LICENSE
├── components/
│   └── cyd_sd_http_server/
│       ├── __init__.py
│       ├── cyd_sd_http_server.h
│       ├── cyd_sd_http_server.cpp
│       ├── sd_storage.h
│       ├── sd_storage.cpp
│       ├── path_utils.h
│       ├── path_utils.cpp
│       └── web_ui.h
├── examples/
│   └── fnk0104s-esp-idf.yaml
├── scripts/
│   ├── upload_file.py
│   └── hardware_smoke_test.py
└── tests/
    ├── test_path_rules.py
    └── README.md
```

Der Code soll klein und nachvollziehbar bleiben. Zusätzliche Abstraktionsschichten sind nur einzuführen, wenn sie konkret getestet werden können.

---

## 5. Vorgeschlagene YAML-Schnittstelle

Zielkonfiguration:

```yaml
external_components:
  - source:
      type: local
      path: components

cyd_sd_http_server:
  id: cyd_sd_server

  # Eigener Server, unabhängig von web_server: auf Port 80.
  port: 8080
  control_port: 32769

  mount_point: /sdcard
  format_if_mount_failed: false
  bus_width: 4

  clk_pin: GPIO38
  cmd_pin: GPIO40
  data0_pin: GPIO39
  data1_pin: GPIO41
  data2_pin: GPIO48
  data3_pin: GPIO47

  # Niemals unbemerkt beliebig große Dateien akzeptieren.
  # 0 kann später optional „kein Softwarelimit“ bedeuten; zunächst besser explizit setzen.
  max_upload_size: 200MB

  # Muss aus secrets.yaml kommen. Keine unsichere Voreinstellung.
  api_key: !secret sd_server_api_key
```

Vorgeschlagene Validierungsregeln in `__init__.py`:

- nur ESP32
- nur ESP-IDF
- zunächst nur ESP32-S3
- Portbereich validieren
- `port` und `control_port` dürfen nicht identisch sein
- 1-Bit verlangt CLK, CMD und DATA0
- 4-Bit verlangt zusätzlich DATA1, DATA2 und DATA3
- `format_if_mount_failed` standardmäßig zwingend `false`
- `api_key` erforderlich und Mindestlänge, zum Beispiel 32 Zeichen
- URL-Präfix fest `/sd` oder konfigurierbar, aber normalisiert
- klare `cv.Invalid`-Meldung bei nicht unterstützter Konfiguration
- konkrete Mindestversion erst nach Ermittlung und Test der tatsächlichen ESPHome-Version festlegen

Das Python-Modul darf keine Laufzeitlogik vortäuschen. Seine Aufgabe ist ausschließlich Validierung, Codegenerierung sowie das Hinzufügen wirklich benötigter ESP-IDF-Komponenten und Defines.

---

## 6. SDMMC- und FATFS-Speicherschicht

### 6.1 Initialisierung

Die Speicherschicht benutzt native ESP-IDF-APIs:

- `SDMMC_HOST_DEFAULT()`
- `SDMMC_SLOT_CONFIG_DEFAULT()`
- Pinbelegung explizit setzen
- `slot_config.width = 4` beziehungsweise 1
- bei Bedarf `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` ergänzen
- `esp_vfs_fat_sdmmc_mount()` auf `/sdcard`
- `format_if_mount_failed = false` als sichere Voreinstellung
- sinnvollen Wert für `max_files`, zum Beispiel 8

Ein Mount-Fehler muss:

- mit dem konkreten `esp_err_t` geloggt werden,
- den ESPHome-Component-Status auf Fehler setzen,
- den HTTP-Server entweder gar nicht starten oder nur einen eindeutigen 503-Status liefern,
- niemals automatisch formatieren, solange der Nutzer dies nicht ausdrücklich aktiviert hat.

### 6.2 Ergebnis- und Fehlervertrag

Keine öffentliche Dateioperation darf `void` zurückgeben, wenn sie scheitern kann. Intern ist ein Ergebnisobjekt vorzusehen, beispielsweise:

```cpp
enum class StorageError {
  NONE,
  NOT_MOUNTED,
  INVALID_PATH,
  NOT_FOUND,
  ALREADY_EXISTS,
  NOT_A_FILE,
  NOT_A_DIRECTORY,
  OPEN_FAILED,
  READ_FAILED,
  WRITE_FAILED,
  SHORT_WRITE,
  CLOSE_FAILED,
  RENAME_FAILED,
  DELETE_FAILED,
  NO_SPACE,
  BUSY,
};

struct StorageResult {
  StorageError error;
  int system_errno;
  size_t bytes_processed;
};
```

Jeder Fehler muss sowohl maschinenlesbar an HTTP als auch mit Pfad, Operation und `errno` im ESPHome-Log erscheinen.

### 6.3 Schreibregeln

- `fwrite()` muss auf exakt geschriebene Bytezahl geprüft werden, nicht nur auf Null/Nichtnull.
- `fclose()` muss geprüft werden.
- Vor erfolgreichem Abschluss darf die Zieldatei nicht sichtbar ersetzt werden.
- Upload zunächst in derselben FAT-Partition als temporäre Datei schreiben, zum Beispiel `/.upload-<id>.part`.
- Nach vollständigem Empfang: flush, close, Größe prüfen, optional Prüfsumme prüfen, dann umbenennen.
- Existierende Zieldatei nur ersetzen, wenn `overwrite=true` ausdrücklich angefordert wurde.
- Scheitert der Upload, bleibt eine vorhandene Zieldatei unverändert.
- Temporäre Dateien bei Fehlern entfernen.
- Beim Booten alte `.part`-Dateien erkennen und entweder mit klarer Logmeldung entfernen oder in einer Diagnose-Liste anzeigen.

Hinweis: Das Verhalten von FATFS beim Ersetzen existierender Dateien durch `rename()` muss auf der eingesetzten ESP-IDF-Version geprüft werden. Falls `rename()` kein bestehendes Ziel ersetzt, ist eine sichere, dokumentierte Zwei-Schritt-Strategie nötig. Ein Stromausfall zwischen den Schritten ist ausdrücklich zu testen.

### 6.4 Nebenläufigkeit

- FATFS-Zugriffe durch einen eigenen Mutex schützen.
- Zunächst maximal einen Upload gleichzeitig erlauben.
- Einen zweiten Upload mit HTTP 409 `BUSY` ablehnen.
- Keine globalen Dateihandles ohne klaren Besitzer.
- Server-Stopp und Component-Teardown müssen offene Dateien schließen.
- Abgebrochene Verbindungen dürfen kein offenes `FILE *` hinterlassen.
- Ein Upload-Kontext braucht Startzeit, erwartete Größe, empfangene Größe, temporären Pfad, Zielpfad und Fehlerzustand.

### 6.5 Pfadsicherheit

Eine zentrale Funktion muss jeden externen Pfad validieren und normalisieren:

- Pfad muss absolut innerhalb der virtuellen SD-Wurzel sein, zum Beispiel `/bilder/a.jpg`.
- `.` und `..` vollständig ablehnen.
- Backslash, NUL und Steuerzeichen ablehnen.
- Wiederholte Slashes normalisieren oder ablehnen.
- Mount-Punkt niemals direkt aus einem ungeprüften Request zusammensetzen.
- Maximale Gesamtpfadlänge und maximale Segmentlänge prüfen.
- Leerer Pfad ist nur dort erlaubt, wo `/` gemeint ist.
- Löschen des Mount-Roots verbieten.
- Symlink-Traversal verhindern; FATFS besitzt normalerweise keine Symlinks, die Annahme trotzdem dokumentieren.
- Dateinamen niemals ungefiltert als HTML ausgeben.

Pfadnormalisierung ist als separat testbare Funktion zu implementieren.

---

## 7. HTTP-API-Vertrag

Basis-URL:

```text
http://<gerät>:8080/sd
```

Die Weboberfläche wird vom selben Port ausgeliefert, damit keine CORS-Abhängigkeit entsteht.

### 7.1 Authentifizierung

Alle API-Endpunkte verlangen:

```http
X-API-Key: <zufälliger langer Schlüssel>
```

Anforderungen:

- Schlüssel kommt aus `secrets.yaml`.
- Vergleich möglichst konstantzeitlich durchführen.
- Schlüssel nie loggen.
- Browseroberfläche fragt ihn ab und speichert ihn höchstens in `sessionStorage`, nicht dauerhaft.
- HTTP überträgt den Schlüssel unverschlüsselt. Dokumentieren, dass der Server nur in einem vertrauenswürdigen, segmentierten LAN betrieben werden darf.
- Kein permissives CORS als Voreinstellung.
- OPTIONS nur soweit für konkret erlaubte Clients erforderlich beantworten.

### 7.2 Einheitliches JSON-Format

Erfolg:

```json
{
  "ok": true,
  "operation": "upload",
  "path": "/bilder/foto.jpg",
  "bytes": 123456
}
```

Fehler:

```json
{
  "ok": false,
  "operation": "upload",
  "path": "/bilder/foto.jpg",
  "error": "SHORT_WRITE",
  "message": "SD card accepted fewer bytes than requested",
  "errno": 28,
  "expected_bytes": 123456,
  "processed_bytes": 98304
}
```

Kein Handler darf Erfolg melden, bevor die zugrunde liegende Operation nachweislich erfolgreich abgeschlossen wurde.

### 7.3 Endpunkte

#### Gesundheitszustand

```http
GET /sd/api/health
```

Antwort enthält mindestens:

- gemountet ja/nein
- Kartentyp, sofern verfügbar
- Gesamt-, belegter und freier Speicher
- Serverversion
- aktiver Upload ja/nein
- Uptime

#### Verzeichnis auflisten

```http
GET /sd/api/list?path=/
```

Antwort enthält Einträge mit:

- Name
- vollständiger virtueller Pfad
- Typ Datei/Verzeichnis
- Größe
- optional Änderungszeit, falls FATFS sie sinnvoll liefert

Keine unbegrenzte Rekursion. Bei sehr großen Verzeichnissen Pagination oder ein hart dokumentiertes Limit einführen.

#### Datei herunterladen

```http
GET /sd/api/file?path=/bilder/foto.jpg
```

- Datei in festen Blöcken streamen, zum Beispiel 8 KiB.
- Niemals komplette Datei in RAM oder PSRAM laden.
- korrekten `Content-Length` senden, wenn bekannt.
- `Content-Type: application/octet-stream` als sichere Voreinstellung.
- `Content-Disposition` mit sicher kodiertem Dateinamen.
- Lesefehler und Client-Abbruch loggen.

#### Datei hochladen

```http
POST /sd/api/file?path=/bilder/foto.jpg&overwrite=false
Content-Type: application/octet-stream
Content-Length: 123456
X-API-Key: ...

<Binärdaten>
```

Implementierung:

1. Authentifizieren.
2. Query-Pfad dekodieren und validieren.
3. `Content-Length` verlangen.
4. Größenlimit und freien Speicher prüfen.
5. Temporäre Datei öffnen.
6. Body in einer Schleife mit `httpd_req_recv()` empfangen.
7. Jeden Block vollständig auf SD schreiben und Zähler aktualisieren.
8. Bei Socket-Timeout angemessen weiter versuchen oder abbrechen; keine Endlosschleife.
9. Bei Client-Abbruch sauber schließen und temporäre Datei beseitigen.
10. Erst nach exakter Größenprüfung finalisieren und umbenennen.
11. JSON-Erfolg senden.

Multipart ist absichtlich nicht erforderlich.

#### Datei löschen

Primär:

```http
DELETE /sd/api/file?path=/bilder/foto.jpg
```

Optional kann zusätzlich ein POST-Fallback angeboten werden:

```http
POST /sd/api/delete?path=/bilder/foto.jpg
```

Da der neue native HTTPD-Server DELETE selbst registriert, besteht keine Abhängigkeit von ESPHomes Webserver-Methoden. Verzeichnisse werden nicht über diesen Endpunkt gelöscht.

#### Verzeichnis anlegen

```http
POST /sd/api/directory?path=/bilder
```

Body darf leer sein, sofern `Content-Length: 0` korrekt verarbeitet wird. Alternativ einen kleinen JSON-Body verwenden.

#### Verzeichnis löschen

Erst nach dem MVP ergänzen:

```http
DELETE /sd/api/directory?path=/bilder
```

Voreinstellung: nur leere Verzeichnisse. Kein rekursives Löschen ohne separate, ausdrücklich bestätigte API.

### 7.4 HTTP-Statuscodes

Da der neue Server direkt ESP-IDF verwendet, sollen passende Statuszeilen explizit gesetzt werden:

- 200: erfolgreiche Operation
- 204: erfolgreich gelöscht, falls ohne JSON-Body
- 400: ungültiger Request oder Pfad
- 401: API-Key fehlt oder ist falsch
- 404: Datei/Verzeichnis nicht gefunden
- 409: Ziel existiert, Upload läuft oder Ressource ist belegt
- 411: Content-Length fehlt
- 413: Upload überschreitet konfiguriertes Limit
- 422: semantisch ungültige Operation
- 500: interner Fehler
- 503: SD-Karte nicht verfügbar
- 507: zu wenig Speicherplatz

Wenn ein Fehler einen JSON-Body hat, muss `Content-Type: application/json` gesetzt werden.

---

## 8. Browseroberfläche

Die Oberfläche wird als eigenes statisches HTML auf Port 8080 ausgeliefert:

```text
GET /sd/
```

Mindestfunktionen:

- API-Key-Eingabe
- Anzeige von Mount-Status und freiem Speicher
- Navigation durch Verzeichnisse
- Datei auswählen und hochladen
- Upload-Fortschritt über `XMLHttpRequest.upload.onprogress`
- klare Erfolgs- und Fehlermeldung einschließlich Servertext
- Liste nach erfolgreicher Operation automatisch aktualisieren
- Downloadlink
- Löschen erst nach Bestätigungsdialog
- Anlegen eines Verzeichnisses
- Buttons während laufender Operation deaktivieren

Sicherheitsregeln:

- Dateinamen ausschließlich über `textContent`, niemals über ungefiltertes `innerHTML` einsetzen.
- Keine externen JavaScript- oder CSS-CDNs.
- Keine Zugangsdaten dauerhaft speichern.
- API-Antworten auf `response.ok` und erwartetes JSON prüfen.
- Netzwerkfehler und HTTP-Fehler getrennt anzeigen.

Die Oberfläche ist Komfort, nicht die einzige Zugriffsmöglichkeit. Alle Funktionen müssen auch über die dokumentierte HTTP-API funktionieren.

---

## 9. PC-Clients

### 9.1 curl-Referenz

Upload:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@./foto.jpg" \
  "http://geraet.local:8080/sd/api/file?path=/foto.jpg&overwrite=false"
```

Download:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -o foto.downloaded.jpg \
  "http://geraet.local:8080/sd/api/file?path=/foto.jpg"
```

Delete:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -X DELETE \
  "http://geraet.local:8080/sd/api/file?path=/foto.jpg"
```

Pfade müssen im echten Client URL-kodiert werden.

### 9.2 Python-Uploader

Ein kleines `scripts/upload_file.py` soll:

- Host, lokalen Pfad und Zielpfad als Argumente akzeptieren,
- API-Key aus Umgebungsvariable oder sicherer Eingabe lesen,
- Datei streamen statt vollständig einzulesen,
- Timeout setzen,
- Server-JSON anzeigen,
- bei Fehlern mit Exitcode ungleich Null enden,
- optional nach Upload wieder herunterladen und SHA-256 vergleichen.

C# kann später mit `HttpClient` denselben API-Vertrag verwenden. Es ist keine separate Firmwareunterstützung erforderlich.

---

## 10. Logging und Diagnose

Jede Operation erhält eine kurze Request-ID. Niemals API-Keys oder Dateiinhalte loggen.

Beispiel erfolgreicher Upload:

```text
[I][cyd_sd_http]: [req=42] upload start path=/bilder/foto.jpg bytes=123456 overwrite=false
[D][cyd_sd_storage]: [req=42] temporary file=/.upload-42.part
[I][cyd_sd_http]: [req=42] upload complete path=/bilder/foto.jpg bytes=123456 duration_ms=1840
```

Beispiel Fehler:

```text
[E][cyd_sd_storage]: [req=43] write failed path=/.upload-43.part requested=8192 written=4096 errno=28
[W][cyd_sd_http]: [req=43] upload aborted target=/video.bin received=10485760 committed=10481664
```

Beim Start ausgeben:

- Serveradresse und Port
- Control-Port
- Mount-Punkt
- SDMMC-Modus und Pins
- Karte erkannt
- Gesamtspeicher und freier Speicher
- Uploadlimit
- Authentifizierung aktiviert ja/nein, niemals den Schlüssel
- `format_if_mount_failed`

Wenn SD nicht gemountet ist, darf im Log nicht trotzdem „server ready“ ohne Einschränkung stehen.

---

## 11. Implementierungsphasen

### Phase 0: Clean-Room-Projekt und Versionsfreeze

- Neues Repository anlegen.
- Lizenz festlegen.
- Keine Dateien aus dem alten Repository übernehmen.
- ESPHome-/ESP-IDF-/Compiler-Version dokumentieren.
- Minimale Buildkonfiguration für FNK0104S erstellen.
- Serielle Logs sicherstellen.

Abnahmekriterium: Leerer External Component kompiliert und wird vom Gerät geladen.

### Phase 1: Nur SDMMC und Speicherschicht

- SDMMC mit den sechs Pins initialisieren.
- Mount ohne automatisches Formatieren.
- Karteninformationen und freien Speicher loggen.
- Testoperationen intern: Datei erzeugen, exakt lesen, umbenennen, löschen.
- Alle Rückgabewerte prüfen.
- Pfadnormalisierung mit Tests implementieren.

Abnahmekriterium: Ein deterministischer Boot-Selbsttest oder temporär aktivierbarer Test beweist auf echter Hardware Lesen/Schreiben/Löschen. Testdateien werden anschließend entfernt.

### Phase 2: Eigener HTTPD-Server und Health/List

- Zweiten HTTPD-Server auf Port 8080 starten.
- Control-Port-Kollision verhindern.
- API-Key-Prüfung implementieren.
- `/health` und `/list` implementieren.
- Server sauber stoppen können.

Abnahmekriterium: Port 80 und Port 8080 laufen gleichzeitig über mindestens eine Stunde stabil.

### Phase 3: Streaming-Upload

- Raw-Binary-POST implementieren.
- Content-Length, Uploadlimit und freien Platz prüfen.
- temporäre Datei und finalen Commit implementieren.
- Client-Abbruch und Timeout behandeln.
- parallelen Upload ablehnen.

Abnahmekriterium: 1 KiB, 1 MiB, 20 MiB und eine größere realistische Datei werden ohne Neustart übertragen und stimmen bitgenau.

### Phase 4: Streaming-Download und Delete

- Download in festen Blöcken.
- DELETE-Datei-Endpunkt.
- Verzeichnis anlegen.
- korrekte MIME-/Downloadheader.

Abnahmekriterium: Upload → Download → SHA-256-Vergleich → Delete funktioniert automatisiert.

### Phase 5: Browseroberfläche

- UI ohne externe Assets.
- Fortschrittsanzeige.
- Fehlerdialoge.
- automatische Aktualisierung.
- sichere Darstellung beliebiger Dateinamen.

Abnahmekriterium: Alle MVP-Operationen sind ohne Entwicklerwerkzeuge im Browser ausführbar und jeder Fehler ist sichtbar.

### Phase 6: Härtung, Dokumentation und Release

- Abbruch-, Stromausfall-, volle-Karte- und Parallelitätstests.
- Speicher- und Socketverbrauch beobachten.
- README mit exakter Plattformunterstützung.
- Bekannte Einschränkungen prominent in Haupt-README.
- Keine ungetesteten Frameworks bewerben.
- Beispielkonfiguration vollständig kompilieren.

Abnahmekriterium: Testmatrix dokumentiert, bekannte Einschränkungen stehen auf der obersten README-Ebene.

---

## 12. Hardware- und Integrationstestmatrix

Mindestens folgende Tests auf echter Hardware durchführen und Ergebnisse dokumentieren:

| Test | Erwartetes Ergebnis |
|---|---|
| Boot ohne SD-Karte | klare Fehlermeldung, kein Crash, Health meldet unavailable |
| Boot mit gültiger FAT32-Karte | Mount erfolgreich |
| unbekanntes/defektes Dateisystem | keine automatische Formatierung |
| leere Datei | definierte Unterstützung oder klarer 422-Fehler |
| 1 KiB Upload | SHA-256 identisch |
| 1 MiB Upload | SHA-256 identisch |
| 20 MiB Upload | SHA-256 identisch, kein hoher RAM-Anstieg |
| Datei größer als Limit | 413 vor dem Schreiben |
| nicht genügend freier Speicher | 507, keine Zieldatei |
| vorhandene Datei, overwrite=false | 409, alte Datei unverändert |
| vorhandene Datei, overwrite=true | neue Datei nach vollständigem Commit korrekt |
| Client während Upload beenden | Ziel unverändert, Handle geschlossen, `.part` bereinigt |
| WLAN-Abbruch | kein Crash, späterer Upload wieder möglich |
| Stromverlust während Upload | vorhandenes Ziel möglichst unverändert, Restdatei erkannt |
| zwei gleichzeitige Uploads | einer erfolgreich, zweiter 409 BUSY |
| Download großer Datei | gestreamt, SHA-256 identisch |
| Delete existierende Datei | Datei entfernt, korrekte Antwort |
| Delete nicht existente Datei | 404 |
| Delete Root oder Verzeichnis als Datei | sicher abgelehnt |
| Pfad mit `../` | 400, keine Operation außerhalb Mount-Root |
| sehr langer Dateiname | gemäß FATFS-Grenzen kontrolliert akzeptiert oder abgelehnt |
| Leerzeichen/Umlaute im Namen | korrekt URL-kodiert und sicher angezeigt |
| falscher API-Key | 401, keine Dateisystemoperation |
| 1 Stunde wiederholte Transfers | kein Leak, kein Watchdog, beide Webserver erreichbar |

Zusätzlich über serielle Logs und gegebenenfalls ESPHome-Debugsensoren beobachten:

- freier Heap vor/nach großen Transfers
- freier PSRAM vor/nach großen Transfers
- offene Sockets
- Reboots und Reset-Ursache
- Watchdogmeldungen
- FATFS-Fehler

---

## 13. Automatisierter Smoke-Test

`scripts/hardware_smoke_test.py` soll gegen ein echtes Gerät laufen und mindestens:

1. Health abrufen.
2. zufällige Testdatei lokal erzeugen.
3. lokale SHA-256 berechnen.
4. Datei hochladen.
5. Verzeichnisinhalt prüfen.
6. Datei wieder herunterladen.
7. SHA-256 vergleichen.
8. Datei löschen.
9. prüfen, dass Download anschließend 404 liefert.
10. eindeutigen Exitcode liefern.

Testdaten lokal in einem temporären Verzeichnis anlegen und nach Erfolg entfernen. Keine Benutzerdateien überschreiben oder löschen.

---

## 14. Definition of Done für den ersten Release

Der MVP gilt erst als abgeschlossen, wenn alle folgenden Punkte erfüllt sind:

- Die Beispiel-YAML kompiliert mit der dokumentierten ESPHome-Version.
- FNK0104S mountet die SD-Karte reproduzierbar über die dokumentierten Pins.
- ESPHome-Webserver auf Port 80 und SD-Server auf Port 8080 laufen gleichzeitig stabil.
- Upload nutzt weder Multipart noch Webserver-OTA.
- Große Dateien werden gestreamt und nicht vollständig im RAM gehalten.
- Download wird ebenfalls gestreamt.
- Jeder Datei-I/O-Rückgabewert wird geprüft.
- Fehlgeschlagene Uploads melden niemals Erfolg.
- Fehlgeschlagene Deletes melden niemals Erfolg.
- Abgebrochene Uploads beschädigen keine vorhandene Zieldatei.
- Pfad-Traversal ist durch Tests ausgeschlossen.
- API-Key wird verlangt und nicht geloggt.
- Browser, curl und Python-Smoke-Test funktionieren.
- Upload/Download-Prüfsummen stimmen.
- Haupt-README nennt unterstützte und nicht unterstützte Plattformen prominent.
- Bekannte Einschränkungen sind nicht in Unterordnern versteckt.
- Die dokumentierte Hardware-Testmatrix wurde tatsächlich ausgeführt.

---

## 15. Bewusste Nicht-Ziele des ersten Releases

- Arduino-Framework
- andere ESP32-Varianten
- SPI-basierte SD-Slots
- SD-Karten-Hotplug während des Betriebs
- rekursives Löschen kompletter Verzeichnisbäume
- mehrere parallele Uploads
- WebDAV, FTP, SMB oder SFTP
- TLS direkt auf dem ESP
- öffentliche Erreichbarkeit aus dem Internet
- Integration in Home Assistants Medienbrowser
- automatische Formatierung unbekannter Karten

Diese Punkte dürfen später ergänzt werden, aber nur mit eigenem Design und Tests.

---

## 16. Risiken und offene Entscheidungen

Vor oder während Phase 1 klären:

1. **Pinbelegung:** Die genannten SDMMC-Pins auf dem konkreten FNK0104S bestätigen.
2. **Pull-ups:** Schaltplan oder Messung prüfen; interne Pull-ups allein sind möglicherweise unzureichend.
3. **FAT-Format:** Welche Kartengröße und welches Dateisystem werden tatsächlich verwendet? Ziel zunächst FAT32; exFAT nur nach nachgewiesener Toolchain-Unterstützung.
4. **Rename-Semantik:** Atomaren Ersetzungsablauf auf der konkreten FATFS-Version testen.
5. **HTTPD-Ressourcen:** Zweiter Server benötigt Task, Stack und Sockets. Port und Control-Port sowie `CONFIG_LWIP_MAX_SOCKETS` beobachten.
6. **Uploadlimit:** Real benötigte maximale Dateigröße mit Nutzer festlegen; Beispielwert 200 MB ist nur ein Startwert.
7. **Leere Dateien:** Entscheiden, ob Upload mit `Content-Length: 0` als gültige leere Datei gelten soll.
8. **Zeitstempel:** Ohne RTC/NTP können Dateizeiten unbrauchbar sein; nicht als garantiertes Feature darstellen.
9. **Stromausfallsicherheit:** FATFS bietet keine vollständige Transaktion. Garantien exakt nach Tests formulieren.
10. **API-Key im Browser:** Komfort gegen Sicherheit abwägen; vorerst nur `sessionStorage`.

Offene Entscheidungen dürfen nicht still angenommen werden. Sie sind in README oder ADR-Dateien zu dokumentieren.

---

## 17. Startanweisung für die übernehmende Instanz

Die nächste Instanz soll in dieser Reihenfolge beginnen:

1. Dieses Dokument vollständig lesen.
2. Neues Repository/Verzeichnis anlegen; alten Quellcode nicht kopieren.
3. Tatsächliche ESPHome-Version ermitteln und dokumentieren.
4. Bereinigte Basis-YAML ohne alten `sd_mmc_card` und `sd_file_server` kompilieren.
5. Pinbelegung und SDMMC-Mount isoliert auf Hardware verifizieren.
6. Erst nach erfolgreichem Mount den zweiten nativen HTTPD-Server implementieren.
7. Zuerst Health und List, danach Raw-Upload, danach Download/Delete, zuletzt UI.
8. Nach jeder Phase die genannten Hardware-Abnahmekriterien ausführen.
9. Keine Erfolgsantwort senden, bevor die echte Operation vollständig geprüft wurde.
10. Keine Unterstützung behaupten, die nicht auf echter Hardware getestet wurde.

