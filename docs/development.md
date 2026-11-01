# Praca nad kodem

Wspierane środowisko to Windows, Arduino CLI, przypięty pakiet
`esp32-bluepad32:esp32@4.1.0` oraz klasyczny ESP32 ECO3 z PSRAM i flash16MB.
Instalację i pierwsze wgranie opisuje [instrukcja](getting-started.md).

`tools/build.py` pobiera sprawdzane SHA256 źródła wskazane przez
`tools/wrover/upstream-lock.json`, generuje skecz konsoli i osobny aktualizator,
kompiluje kolejno i sprawdza obrazy/partycje. Nie wgrywa firmware.
Automatyczny downloader nigdy nie pobiera upstreamowego media.h, data,
plików gier ani oryginalnego ROM-u Atari. Kod i noty Altirra są w źródłach
zależności. `local/`, `.tools/` i `.build/` są wyłączone z Gita.

Nie są potrzebne gry do kompilacji: tablice katalogu pochodzą z metadanych,
a wskaźniki obrazów są puste. Gra trafia do PSRAM dopiero z własnej microSD.
Opcjonalny `local/roms/atarixl.rom` jest sprawdzany SHA256 i dołączany wyłącznie
do lokalnego obrazu. Bez niego trzy zależne profile są blokowane.

## Pamięć i zgodność

Renderer zachowuje11 gorących funkcji ANTIC/GTIA w IRAM. Nie zmieniaj tego
na podstawie samego powodzenia kompilacji — kontrola artefaktu sprawdza ich
adresy. Budżet IRAM jest mały. Niezmieniony obiekt HAL SPI z tego samego SDK
jest wydobywany przez `ar x` i linkowany lokalnie, aby funkcje SPI używane
podczas ładowania pozostały we flash. Nie modyfikujemy zainstalowanego SDK.
Przerwania SPI nie są oznaczone IRAM; video pozostaje aktywne podczas odczytu,
a karta jest odmontowana przed emulacją gry.

Przed zwolnieniem buforów poprzedniej gry należy zamknąć SIO, cartridge,
cassette oraz BINLOAD_bin_file i stan rozpoczętego loadera XEX. Sprawdzenie
otwartych slotów `/game` odmawia zwolnienia pamięci przy aktywnych uchwytach.
To istotne także przy przełączaniu gry jeszcze podczas ładowania XEX.

Zachowaj: native opcje gier, A/LT/RT, zdublowany krzyżak/drążek, View jako
menu i Menu jako OPTION, pamiętane role graczy oraz ochronę kasowania NVS.
Nie dodawaj automatycznych testów grania do produkcyjnego firmware.

## Testy

Logika kontrolerów i mapowanie pada mają testy C++ na komputerze:

```powershell
cmake -S tests -B .build/host-tests
cmake --build .build/host-tests --config Release
ctest --test-dir .build/host-tests -C Release --output-on-failure
```

Wymagają CMake i kompilatora C++, np. Visual Studio Build Tools.
Opcjonalny podgląd UI na Windows wymaga też Pillow oraz MSVC:

```powershell
.\.venv\Scripts\python.exe tools/wrover/preview-ui.py
```

Sprawdza133stany menu/progresu oraz granice rysowania. Nie uruchamia emulatora
PC i nie dowodzi poprawnego parowania Bluetooth ani zachowania na TV.
Testy gier na rzeczywistej konsoli wykonuj na swoich plikach.

Przy publikacji sprawdzono kompilację obu programów bez gier i oryginalnego
ROM-u,2testy kontrolerów oraz133stany UI. Konsolka na sprawdzonym sprzęcie
działa; pełnej procedury pierwszej instalacji na drugiej, pustej płytce nie
zweryfikowano jeszcze fizycznie. Narzędzia publikacji nie wgrywały nic na
działający egzemplarz autora.

## Instalacja a aktualizacja

Nową płytkę instaluje wyłącznie `tools/install-new-board.py`, z podanym portem,
potwierdzonym MAC i flagą inicjalizacji. To narzędzie zapisuje układ partycji;
odmawia tego, jeśli konsola jest już zainstalowana. Nigdy nie stosuj go jako
zwykłego aktualizatora.

Aktualna konsola używa `tools/update.py` w osobnym trybie Wi-Fi, tokenu i SHA256.
Nie zmieniaj układu partycji i nie kasuj NVS w ramach rutynowego wdrożenia.
Archiwa wyników, prywatne kopie flash i pliki multimedialne pozostają lokalnie.
