# Praca nad kodem

[← Strona projektu](../README.md) · [Pierwsze uruchomienie](getting-started.md)

## Środowisko i kompilacja

Sprawdzona konfiguracja to Windows, Python 3.13, Arduino CLI oraz pakiet
`esp32-bluepad32:esp32@4.1.0`. Sprzęt docelowy: klasyczny ESP32 w rewizji ECO3,
16 MB flash i pamięć PSRAM.

```powershell
.\.venv\Scripts\python.exe tools/build.py
```

Polecenie przygotowuje i kolejno kompiluje konsolę oraz osobny aktualizator
Wi-Fi. Sprawdza obrazy i układ partycji, ale niczego nie wgrywa.

## Układ projektu

| Katalog | Zawartość |
| --- | --- |
| `src/` | Obsługa kontrolerów i normalizacja wejścia |
| `tools/wrover/archon/` | Menu, sterowanie i uruchamianie gier |
| `tools/wrover/updater/` | Osobny program aktualizatora Wi-Fi |
| `tools/` | Przygotowanie, kompilacja, instalacja i aktualizacja |
| `tests/` | Testy kontrolerów i mapowania przycisków |
| `local/` | Prywatna konfiguracja i pliki użytkownika; poza Gitem |
| `.build/` | Wygenerowane źródła, kompilacje i kopie; poza Gitem |

## Zależności i pliki użytkownika

Lista źródeł z przypiętą rewizją i sumami SHA-256 znajduje się w
`tools/wrover/upstream-lock.json`. Pobierany jest kod emulatora oraz otwarte
zamienniki Altirra. Plik `media.h` z pakietem gier i katalog `data` są pomijane.

Kompilacja nie wymaga obrazów gier. Firmware zawiera metadane, a pliki trafiają
do PSRAM z karty dopiero po wybraniu tytułu. Opcjonalny ROM systemowy
`local/roms/atarixl.rom` jest sprawdzany i dołączany tylko do lokalnego obrazu.
Bez niego trzy zależne profile pozostają niedostępne.

## Pamięć i ładowanie

Przy starcie sprawdzane są nazwy i rozmiary znanych plików. Po wybraniu gry
konsola zamyka uchwyty poprzedniego tytułu, zwalnia jego bufory i wczytuje nowy
zestaw do PSRAM. Zawartość jest sprawdzana przez SHA-256. Czytnik zostaje
odmontowany przed wznowieniem emulacji; system plików `/game` korzysta
z buforów w pamięci.

Przed zwolnieniem buforów muszą być zamknięte dyski SIO, kartridż, kaseta oraz
`BINLOAD_bin_file` rozpoczętego programu XEX. Trzeba też wyzerować stan loadera
XEX. Kontrola otwartych slotów `/game` odmawia zwolnienia pamięci, jeśli
pozostał aktywny uchwyt.

Jedenaście funkcji ANTIC/GTIA pozostaje w szybkiej pamięci IRAM. Kontrola
artefaktu sprawdza ich adresy. Budżet jest mały, dlatego sama udana kompilacja
nie wystarcza do oceny zmiany.

Niezmieniony obiekt HAL SPI z przypiętego SDK jest wydobywany poleceniem
`ar x` i linkowany lokalnie. Dzięki temu kod SPI używany podczas ładowania
pozostaje we flash zamiast zajmować IRAM renderera. SDK nie jest modyfikowany.
Przerwania SPI nie mają flagi IRAM; obraz jest aktywny podczas odczytu,
a aplikacja nie zapisuje wtedy pamięci flash.

## Testy

Testy C++ wymagają CMake i kompilatora, na przykład Visual Studio Build Tools:

```powershell
cmake -S tests -B .build/host-tests
cmake --build .build/host-tests --config Release
ctest --test-dir .build/host-tests -C Release --output-on-failure
```

Podgląd interfejsu na Windows wymaga Pillow i MSVC:

```powershell
.\.venv\Scripts\python.exe tools/wrover/preview-ui.py
```

Narzędzie renderuje 216 stanów menu i ekranów postępu, sprawdzając granice
rysowania. Testy na komputerze nie zastępują parowania padów ani prób na TV.

W zestawie publikacyjnym sprawdzono kompilację obu programów bez gier
i oryginalnego ROM-u oraz podgląd interfejsu. Testy kontrolerów obejmują także
zarządzanie przypisaniami, migrację ustawień i nieudany zapis. Pełna
procedura pierwszej instalacji na drugiej, pustej płytce wymaga jeszcze próby
sprzętowej.

## Zasady zmian

- Zachowaj oryginalne ekrany wyboru trybu gry i obecne mapowanie pada.
- Nie dodawaj automatycznych prób grania do produkcyjnego firmware.
- Pierwszą instalację wykonuj tylko na nowej płytce, z potwierdzonym MAC.
- Działającą konsolę aktualizuj przez Wi-Fi, bez zmiany partycji i kasowania
  pamięci NVS z ustawieniami oraz przypisaniami padów.
- Konfigurację sieci, gry, ROM-y, obrazy firmware i kopie flash pozostawiaj
  w ignorowanych katalogach lokalnych.

Licencje zależności opisuje [THIRD_PARTY.md](../THIRD_PARTY.md).
