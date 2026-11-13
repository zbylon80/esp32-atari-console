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
Bez niego sześć zależnych profili pozostaje niedostępnych: Alley Cat, Misja,
New York City - The Big Apple, Road Race i obie części Spy vs. Spy.

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

Powłoka konsoli, czcionka, katalog i instrukcje są w firmware. Błąd skanu SD
otwiera ekran diagnostyczny, a nie emulację BASIC-a. BASIC jest wybierany jawnie
w ustawieniach. `LibraryStatus` jest migawką ostatniego skanu/ładowania;
otwieranie diagnostyki nie odpytuje karty. A na tym ekranie wywołuje jawny skan
metadanych, zachowując `cached`, `cachedGame` i uchwyty rdzenia. Tablice skanu
są alokowane tylko raz. Nie przenoś skanu do pętli grania ani nie zwalniaj
cache w ramach diagnostyki. Ten moduł nie identyfikuje fizycznie wypiętego pinu.

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

## Globalne przyciski Atari

`tools/wrover/archon/system-controls.h` oddziela fizyczne Menu/View/Share od
akcji START/SELECT/OPTION. Wspólna konfiguracja obu padów jest zapisywana jako
wersjonowana permutacja w NVS `atari_ui/syskeys`; nie zmienia przypisań graczy.
Nieprawidłowy zapis powoduje użycie układu domyślnego.

View rozróżnia krótkie puszczenie i przytrzymanie 700 ms. Długie naciśnięcie
otwiera menu bez wysyłania przypisanej akcji Atari. Powrót z UI oraz utrata
połączenia wymagają puszczenia trzymanych środkowych przycisków.

Katalog instrukcji używa tokenów `@S`, `@L`, `@O` oznaczających przyciski dla
START, SELECT i OPTION. Renderer podstawia aktualne nazwy. Przygotowanie
firmware sprawdza długość instrukcji PL/EN we wszystkich sześciu układach.
Test `console_system_controls` sprawdza próg przytrzymania, puszczenie,
przepełnienie zegara, krótkie impulsy i niezależność obu padów.
