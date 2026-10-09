# Plan: Bildsteuerung mit verstecktem Select

Stand: 2026-10-09. **Planung, noch nicht implementiert.**

Ziel ist eine dritte, pro LVGL-Bild-Widget wählbare Betriebsart. Der Nutzer
konfiguriert weiterhin `select:` mit einer zusätzlichen booleschen Option
`hidden`. Home Assistant zeigt den aktuellen Bildnamen als ausschließlich
lesbaren Textstatus; Bildwechsel erfolgen über eine Aktion im HA-Automationseditor.
Diese Bedeutung von „Text Entry“ wurde vom Nutzer bestätigt.

| Konfiguration | Darstellung in Home Assistant | Bildwechsel |
| --- | --- | --- |
| Ohne `select:` | Keine Bild-Entität | Bisheriges statisches Startbild |
| `select:` ohne `hidden` oder mit `hidden: false` | Bisheriges Dropdown | Dropdown und bestehende Automationen |
| `select:` mit `hidden: true` | Textstatus als `sensor.*`, kein Dropdown | Neue ESPHome-API-Aktion |

`hidden` ist eine neue Option dieser Komponente. Sie ist kein allgemeiner
ESPHome- oder Home-Assistant-Schalter. Alle drei Betriebsarten dürfen auf
demselben Gerät gleichzeitig vorkommen und teilen weiterhin Bildliste und Cache.

Ein ausgeblendetes HA-Select allein erfüllt das Ziel nicht: Es bleibt ein
bedienbares Select. Vorgesehen sind deshalb ein internes ESPHome-Select, ein
separater Textsensor und eine API-Aktion. Interne ESPHome-Entitäten werden nicht
an HA veröffentlicht; siehe die [ESPHome-Entitätsoptionen](https://esphome.io/components/text_sensor/).

Die Aktion ist auch in Skripten und manuell aufrufbar. „Nur Automationen“ meint
hier den vorgesehenen Bedienweg ohne Select-Bedienelement, keine technische
Zugriffssperre gegen manuelle Aktionsaufrufe. HA verwendet dieselben Aktionen
für verschiedene Aufrufwege; siehe [HA-Aktionen](https://www.home-assistant.io/docs/scripts/perform-actions/).

## Geplante YAML-Oberfläche

Dieses Fragment beschreibt die geplante Erweiterung und ist mit dem aktuellen
Komponentenstand noch nicht gültig. LVGL-Widget und übrige Gerätekonfiguration
werden wie bisher separat angelegt.

```yaml
api:
  # Bestehende API-Konfiguration einschließlich Verschlüsselung beibehalten.

cyd_sd_http_server:
  display_images:
    images:
      - name: Tag
        path: /images/tag.bin
      - name: Nacht
        path: /images/nacht.bin
    widgets:
      - image_widget: sd_display_image
        initial_image: Tag
        select:
          id: sd_display_image_select
          name: Display image
          hidden: true
```

- `hidden` liegt innerhalb von `select:` und hat den Standardwert `false`.
- Dieselbe Erweiterung gilt für die bestehende Singularform `display_image`.
- `id` bleibt die ID eines echten ESPHome-Selects. Lokale Referenzen und
  Select-Automationen bleiben damit verwendbar.
- Bei `hidden: true` wird dieses Select intern gehalten. Die Komponente erzeugt
  automatisch einen externen Textsensor mit dem konfigurierten Anzeigenamen.
  Kein zusätzlicher `text_sensor:`-Block ist nötig.
- Der Textsensor erhält eine eigene, deterministisch aus der Widget-ID abgeleitete
  ID. Anzeigename und Icon werden übernommen; Select-Callbacks verbleiben am
  Select. Die Behandlung weiterer Entitätsoptionen wird explizit implementiert
  und getestet, statt die gesamte Select-Konfiguration auf den Sensor zu kopieren.
- Für den neuen Modus ist `api:` erforderlich. Bestehende Konfigurationen ohne
  verstecktes Select erhalten keine neue API-Pflicht.
- `hidden` ist eine Firmware-Konfiguration, kein zur Laufzeit schaltbarer Modus.

## Bedienung in Home Assistant

Pro verstecktem Widget wird automatisch eine Aktion mit genau einem Textparameter
`option` angelegt. Vorgesehener Aktionsname auf ESPHome-Seite:
`cyd_sd_set_<image_widget_id>`. Die explizite Widget-ID liefert eine stabile
Zuordnung, auch wenn Select-IDs automatisch erzeugt werden.

Für einen beispielhaften Gerätenamen `display_panel` ergibt sich:

```yaml
# Geplanter HA-Aktionsaufruf innerhalb einer Automation:
action: esphome.display_panel_cyd_sd_set_sd_display_image
data:
  option: Nacht
```

Der tatsächliche HA-Aktionsname ist nach der Geräteerkennung zu prüfen.
ESPHome veröffentlicht benutzerdefinierte API-Aktionen für HA und unterstützt
Textparameter; siehe [Native API: User-defined Actions](https://esphome.io/components/api/#user-defined-actions).

Im HA-Automationseditor legt der Nutzer beispielsweise einen Zeit-, Temperatur-
oder Lichtauslöser an, wählt unter „Aktion ausführen“ die betreffende ESPHome-Aktion
und trägt den Bildnamen ein. Für unterschiedliche Zustände können „Auswahl“/`choose`
oder Templates genutzt werden. Eine neue Bildauswahl-Entität entsteht dadurch
nicht. Die erste Umsetzung verspricht keinen automatisch erzeugten Dropdown mit
Bildnamen im Aktionsformular; `option` ist ein Textfeld.

Temperaturgrenzen und Helligkeitsschwellen sollten bei Bedarf getrennte Ein- und
Ausschaltwerte erhalten. Für eine sofort korrekte Anzeige nach Neustart oder
Wiederverbindung kann die HA-Automation zusätzlich bei Geräteverfügbarkeit den
aktuellen Zustand auswerten. Das Gerät selbst lädt weiterhin `initial_image`.

## Umsetzungsschritte

1. **Schema und Codegenerierung erweitern**
   - In `components/cyd_sd_http_server/__init__.py` ein gemeinsames Select-Schema
     mit `hidden: false` für beide Bildkonfigurationen verwenden.
   - Das vorhandene `CydSdImageSelect` in allen Select-Modi erhalten. Im neuen
     Modus die interne Registrierung erzwingen und den Textsensor ergänzen.
   - Textsensor-Unterstützung und API-Aktionsunterstützung nur für den neuen Modus
     aktivieren. Den bisherigen reinen Dateiserver weiterhin ohne LVGL/API bauen.
   - Fehlendes `api:`, ungültige Bool-Werte und Kollisionen erzeugter Sensor-IDs
     oder Aktionsnamen mit verständlichen Validierungsfehlern abweisen.
   - Widersprüchliche oder nicht übertragbare Entitätsoptionen nachvollziehbar
     behandeln; bisher gültige YAML ohne `hidden: true` unverändert akzeptieren.

2. **Status an den bestehenden Bildwechsel anbinden**
   - `DisplayImageWidget` in `cyd_sd_http_server.h` um einen optionalen
     Textsensor-Verweis ergänzen.
   - In `select_image()` den zugewiesenen Bildnamen zusätzlich an diesen Sensor
     veröffentlichen, sobald der bestehende Auswahlvorgang erfolgreich ist.
   - Startbild, lokale Select-Steuerung und API-Aktion nutzen denselben Pfad.
     Kein optimistischer Statuswechsel vor erfolgreicher Zuweisung.
   - Unbekannte Optionen, fehlende Dateien und erkannte Speicherzugriffskonflikte
     lassen Bild und Status unverändert. Scheitert bereits das Startbild, bleibt
     der Sensor ohne bestätigten Wert; kein erfundener erfolgreicher Bildname.
   - Der Status bestätigt die vom Controller vorgenommene Quellenzuweisung.
     Die aktuelle Streaming-Implementierung garantiert keine spätere vollständige
     Decodierung: Eine existierende, beschädigte Datei kann erst beim Rendern
     scheitern. Diese Grenze nicht als erfolgreich geprüfte Darstellung ausgeben.

3. **API-Aktion automatisch registrieren**
   - Einen kleinen Adapter pro verstecktem Widget vorsehen, der `option` gegen die
     vorhandenen Select-Optionen prüft und den bestehenden Select-Steuerpfad nutzt.
     Nur freigegebene Bildnamen akzeptieren, niemals beliebige Dateipfade.
   - Im vorhandenen ESPHome/LVGL-Ausführungskontext bleiben. Keine zusätzliche
     SD-Zugriffsschicht und keine HTTP-Anfragen für Bildwechsel einführen.
   - Die lokal installierte Zielversion ESPHome 2026.8.0 bietet
     `api::CustomAPIDevice::register_service()`. Dieser Weg benötigt
     `USE_API_USER_DEFINED_ACTIONS` und `USE_API_CUSTOM_SERVICES`.
     Die Komponente soll diese Unterstützung bedingt selbst aktivieren, damit
     außer `hidden: true` keine manuell geschriebenen API-Aktionen oder Lambdas
     nötig sind. Das Zusammenspiel mit vorhandenen `api.actions` im Build prüfen.
   - Bei diesem Adapter sind Aktionsaufrufe ohne Ergebnisantwort. Fehler werden
     geloggt; der Textsensor bestätigt die angenommene Auswahl. Ein erfolgreicher
     HA-Aufruf allein bestätigt keinen erfolgreichen Bildwechsel.
   - Registrierung einmalig vor der API-Erkennung durchführen. Wiederverbindung
     darf keine zusätzlichen Aktionen oder Entitäten erzeugen.

4. **Dokumentation und Beispiele ergänzen**
   - README und `docs/DIRECT_SD_IMAGE_WORKFLOW.md` um die drei Betriebsarten,
     den HA-Aktionsaufruf und die Fehler-/Neustartsemantik erweitern.
   - Ein Beispiel mit statischem, sichtbarem und verstecktem Widget ergänzen;
     vorhandene Beispiele und Bedienmöglichkeiten erhalten.
   - HA-Beispiele für Tageszeit und temperaturabhängige Auswahl dokumentieren.

## Kompatibilität und Umstellung

Bestehende Konfigurationen behalten mit dem Standard `hidden: false` ihr Verhalten.
Gemeinsame Allowlist, zentrale Pfadprüfung, SD-Synchronisierung, PSRAM-Cache,
Streaming-Fallback, Uploads und Downloads bleiben erhalten. Erneute Auswahl
desselben Bildnamens muss weiterhin das Nachladen einer ersetzten Datei erlauben.

Beim bewussten Wechsel eines vorhandenen Widgets auf `hidden: true` entfällt dessen
externes `select.*`; stattdessen entsteht ein `sensor.*`. Das ist ein Wechsel der
HA-Entitätsart. Bestehende Automationen mit `select.select_option` müssen für dieses
Widget auf die neue Aktion umgestellt werden. Verwaiste HA-Registry-Einträge können
eine manuelle Bereinigung benötigen. Stabile Firmware-IDs garantieren keine
identische HA-Entitäts-ID über diesen Domänenwechsel hinweg. Der Rückwechsel auf
`hidden: false` muss ebenfalls geprüft und dokumentiert werden.

## Abnahme und Prüfungen bei späterer Implementierung

- Schema/Codegenerierung: fehlendes und explizit falsches `hidden`, neuer Modus,
  Mischbetrieb, Singularform, fehlende API sowie Namenskollisionen abdecken.
- Verhalten: richtige Widget-Zuordnung, erlaubte/unbekannte Namen, fehlende/busy
  Dateien, Startfehler, erneute Auswahl und übereinstimmenden Status testen.
- Vorhandene lokale Select-Referenzen und Callbacks auch im internen Modus prüfen.
- Host-Syntaxtests erfassen derzeit den LVGL/API-Zweig nicht vollständig; passende
  Testunterstützung ergänzen und nicht als Ersatz für den Firmware-Build behandeln.
- Hosttests, `esphome config`, `compile --only-generate` und vollständigen Build
  mit ESPHome 2026.8.0 / ESP-IDF 5.5.5 ausführen. Sowohl bisherige Konfigurationen
  als auch eine neue Mischkonfiguration prüfen.
- An einem ausdrücklich vorgesehenen Gerät mit HA prüfen: Textstatus ohne
  Dropdown, Aktionsformular im UI, tatsächlicher Bildwechsel, unabhängige Widgets,
  Wiederverbindung, Moduswechsel sowie unveränderte Web- und Cache-Funktionen.
  Hardwareergebnisse getrennt von Host-/Build-Ergebnissen festhalten.

Dieser Plan umfasst keine Implementierung und keine Hardwareänderung. Technische
Grundlage sind der aktuelle Repository-Code und die lokal installierten
ESPHome-2026.8.0-Quellen; die verlinkte Onlinedokumentation kann neuer sein.
