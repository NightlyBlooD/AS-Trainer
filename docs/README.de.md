# AS-Trainer 1.0.0

[English](../README.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Українська](README.uk.md) · [简体中文](README.zh-CN.md)

Ein C++20-/Dear-ImGui-Trainer für Azeroth Survivors unter Windows x64. Er bietet 25 Charakterwerte, Einstellungen für den nächsten Durchlauf, INI-Profile, automatische Belohnungswahl, das legendäre Klassenset vom Endboss und 1–16× Simulationsgeschwindigkeit.

## Herunterladen und starten

Lade **`AS-Trainer-1.0.0-windows-x64.zip`** von [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases) herunter. Entpacke das gesamte Archiv. `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, Dokumentation und Lizenzhinweise müssen zusammenbleiben. DLL, Loader und `SHA256SUMS.txt` werden auch einzeln angeboten.

1. Starte die unterstützte Spielversion im Fenster oder im randlosen Fenstermodus.
2. Starte `load_overlay.cmd` oder `load_overlay.exe` mit denselben Berechtigungen wie das Spiel.
3. **Insert** blendet das Fenster ein oder aus. Bearbeite Werte oder lade unter **Konfigurationen** ein Profil. Bestätige Automatisierungsänderungen mit **Regeln anwenden**.
4. **End** verwirft ausstehende Schreibbefehle, Fixierungen und Start-Markierungen, deaktiviert Automatisierung und Gruppenmodus, setzt die Zeit auf 1× und blendet das Fenster aus. Bereits geänderte Spielwerte und Konfigurationsdateien bleiben erhalten.

Bei mehreren Spielprozessen: `load_overlay.exe --pid 12345`. Die DLL bleibt bis zum Spielende geladen. **Starte das Spiel vor einem Update neu**; ein erneuter Loader-Aufruf ersetzt die geladene DLL nicht.

## Kompatibilität und Sprache

Unterstützte EXE-Profile werden durch den exakten SHA-256 von `AzerothSurvivors.exe` bestimmt:

| Spielversion | SHA-256 |
|---|---|
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

Die DLL benötigt einen verifizierten EXE-Hash, ein geprüftes Objektlayout und die passende native Aufrufkonvention (ABI). Vor dem Einrichten der Hooks prüft sie wichtige Codebytes. Andere Builds werden abgelehnt; die EXE auf dem Datenträger bleibt unverändert. Das aktuelle Profil wurde anhand der EXE und mit Offline-Testobjekten geprüft. Vollständige Live-Durchläufe mit allen Funktionen sowie echte Koop-Sitzungen sind noch nicht bestätigt.

Die Oberfläche folgt der `language.txt` des Clients, normalerweise `%APPDATA%\wenilla-rogue\language.txt`. Englisch, Russisch, Deutsch, Französisch, Spanisch, Ukrainisch und vereinfachtes Chinesisch werden unterstützt. Unbekannte Werte verwenden Englisch. Sprachänderungen werden ohne Trainer-Neustart übernommen.

## Werte und Profile

**Anwenden** setzt einen absoluten Wert einmalig. Im Menü bereitet es den nächsten Durchlauf vor und aktiviert **Start**. **Fixieren** stellt den Wert nach Neuberechnungen des Spiels wieder her und aktiviert ebenfalls Start. **Start** übernimmt den eingestellten Wert in jeden neuen Durchlauf. Ohne Start wird die Fixierung dieses Feldes nicht in den nächsten Durchlauf übernommen. Aktuelle Werte erscheinen, sobald ein aktiver Charakter verfügbar ist.

Die 25 Felder umfassen Schaden, Lauftempo, Abklingzeit, Sammelradius, Wirkungsbereich, Glück, Gold, Truhenkosten, Neuwürfe, Überspringen, aktuelle/maximale Gesundheit, kritische Trefferchance/-schaden, Erfahrungs-/Goldgewinn, Schadensminderung, Regeneration, zusätzliche Ziele, Echo, Heilung pro Tötung, Dornen, DoT-Stapel, Buff-Stärke und Wiederbelebungen.

| Wert | Bedeutung |
|---|---|
| Schaden / Tempo / Bereich | `1` = normal; `2` = doppelter Multiplikator |
| Abklingzeit | `0.5` = doppelt so schnell; `0.1` = zehnmal so schnell |
| Sammelradius | Yards |
| Glück | `0.2` = +20 %; `100` = +10000 % |
| Kritische Chance / Echo / Minderung | Anteil: `0.5` = 50 % |
| Truhenkosten / Überspringen | `0` = kostenlose Truhe; Überspringen ist ein Berechtigungsflag |

Profile liegen in `configs` neben der geladenen DLL. Gespeichert werden bestätigte Werte, Fixieren/Start und angewendete Regeln, kein unbestätigter Eingabetext. Laden ersetzt die Einstellungen; ungültige Dateien werden vollständig abgelehnt. **`default.ini`** wird beim nächsten DLL-Start automatisch geladen. Die Beispiele werden manuell ausgewählt:

- [normal.ini](../configs/normal.ini): Automatisierung aus, Zeit 1×.
- [legendary_16x.ini](../configs/legendary_16x.ini): 16×, automatische Auswahl, legendäres Set und ausgewählte Fixierungen.
- [party_boost.ini](../configs/party_boost.ini): alle 25 Felder für die eigene Gruppe, Zeit 1×.

Die Zahlen sind **absolute Zielwerte**, keine Zuschläge: Schaden `20` bedeutet einen Schadensmultiplikator von 20×.

## Belohnungen und Zeit

Zauber und Schmuck haben getrennte Schalter und Prioritäten. Verwende interne Schlüssel aus [reference_keys.txt](../configs/reference_keys.txt), etwa `wrath`, `hurricane`, `chosen` oder `legendary:wrath`. Ein früherer Schlüssel gewinnt vor höherer Seltenheit; bei Gleichstand gewinnt die erste Karte. Ohne passenden Schlüssel zählt die Seltenheit. Die Verzögerung beträgt **50–5000 ms Echtzeit**. Nur geöffnete Angebote werden ausgewählt; gespeicherte Stufenaufstiege musst du zuerst mit der Spielschaltfläche öffnen.

Die Endboss-Regel vergibt das legendäre Rüstungs-/Waffenset der **aktuellen Klasse** aus der Ahn'Qiraj-/Naxxramas-Tabelle nach dem Endboss **jeder Zone**: meist 9 Gegenstände, bei Schurke und Schamane 10. Die normale Beuteverarbeitung fügt sie der Ausrüstungssammlung hinzu. Schmuck im Durchlauf und Zauberseltenheit sind davon getrennt.

Die Beschleunigung nutzt native Simulationsschritte. Bei 16× entsprechen 16 aktive Spielminuten ungefähr einer realen Minute, sofern die Leistung reicht. Pausen und Belohnungsauswahl behalten ihre normalen Unterbrechungen; die Auswahlverzögerung bleibt Echtzeit. Die CPU-Last steigt, daher ist die tatsächliche Dauer leistungsabhängig.

## Koop: nur der Raum-Host

Aktiviere im eigenen Raum **Werte auf die ganze Gruppe anwenden (nur Host)** und bestätige die Regeln. Dieselben Werte und Fixieren/Start-Regeln gelten dann für die Gruppencharaktere. Freunde behalten ihre Klassen, Inventare, Ausrüstung, Zauber und Fortschritte. Host-Inhalte werden nicht kopiert; der Schalter teilt Zahlenwerte, keine automatische Auswahl oder Inventare.

Ein nur beim Gast geladener Trainer kann die Simulation eines fremden Hosts nicht ändern. Das Fenster zeigt den Host-Charakter und Gruppendiagnosen. Echte Koop-Sitzungen mit Freunden sind **noch nicht verifiziert**; automatisierte Tests verwenden Testobjekte und native Funktionsstubs.

## Offline-Signatursuche

Das ZIP enthält `signature_scan.exe` und die Profile unter `signatures`; sie werden nicht als eigene Release-Dateien veröffentlicht. Aus dem entpackten Archivverzeichnis lässt sich eine EXE untersuchen, ohne das Spiel zu starten oder die DLL zu laden:

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-0.132.0.ini --output scan.json
```

Profile verwenden hexadezimale Bytes im IDA-Format mit `?` / `??` als Platzhalter für ein ganzes Byte. Jede Regel muss genau einen Treffer liefern. Die Auflösung unterstützt den Treffer selbst, ein RIP-relatives Ziel, den Funktionsanfang über `.pdata` und einen vorzeichenbehafteten Feldoffset. Der UTF-8-JSON-Bericht enthält Hashes, Trefferstatus, Dateioffset, RVA/VA und gegebenenfalls den Feldoffset.

Ein abweichender Hash wird als `hash_matches: false` gemeldet und verhindert die Offline-Suche nicht. Eine Code-RVA bezeichnet eine Anweisung oder Funktion, keinen Fließkommawert einer Eigenschaft. Ein Feldoffset ist eine Position im Objekt, keine Live-Adresse. Der Scanner kann andere Builds untersuchen, doch passende Muster allein bestätigen weder Layout noch ABI und bieten keine automatische Unterstützung künftiger Spielversionen.

## Bauen und veröffentlichen

Benötigt werden Visual Studio 2022 mit C++-x64-Werkzeugen, Windows SDK und CMake 3.24+. Im Repository-Stamm:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions verwendet Windows 2022 x64 Release für Pushes und Pull Requests. Tags `vX.Y.Z` veröffentlichen Release-Dateien nach erfolgreichem Build und Tests. Vollständige Live-Durchläufe mit allen Funktionen sind noch nicht verifiziert.

## Abhängigkeiten

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b): MIT. [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4): BSD 2-Clause. Mitgelieferte Schriftarten behalten ihre Ursprungslizenzen. Lizenzhinweise müssen bei Weitergabe erhalten bleiben. Win32, Direct3D 11 und BCrypt stammen aus dem Windows SDK.
