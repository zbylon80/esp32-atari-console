# Zbuduj własną konsolkę — instrukcja od zera

Instrukcja dotyczy Windows10/11 i sprawdzonej płytki Waveshare
ESP32-DEV-KIT-WROVER-E-M, SKU35528. Nie musisz znać Arduino ani programować.
Czytaj po jednym etapie; przed następnym sprawdź opisany wynik.

Repo nie zawiera plików gier ani oryginalnego ROM-u Atari. Można jednak
skompilować całą konsolkę i uruchomić otwarty Altirra BASIC bez tych plików.
Wgranie programu na ESP32 nie jest tym samym co kopiowanie gier na kartę.

## 1. Co przygotować

| Część | Ile i jaka |
| --- | --- |
| Płytka | 1× Waveshare SKU35528, z wlutowanymi pinami; klasyczny ESP32 ECO3, flash16MB i PSRAM8MB. |
| Pad | 1× Xbox Wireless Controller1914 z Bluetooth; drugi identyczny jest opcjonalny. |
| Ekran | TV z wejściem composite/CVBS lub HDMI. |
| Konwerter | Przy TV HDMI: **AV/RCA/CVBS → HDMI**, z wejściem video i własnym zasilaniem USB. **HDMI → AV działa w odwrotną stronę i nie pasuje.** |
| RCA | Gniazdo/złączka śrubowa dla video; 1lub2 kolejne dla audio, przewód RCA; HDMI do TV. |
| Rezystor audio | 1×1kΩ, np.1/4W; zmierzony około966Ω również działał. |
| Kondensator audio | 1×10nF ceramiczny, oznaczenie **103**. 104 to100nF, inna wartość. |
| microSD | Moduł SDTF-02 ze stabilizatorem3,3V, karta FAT32; 4/8GB w zupełności wystarczy. |
| Przewody | Dupont żeńsko-żeńskie do dwóch rzędów pinów oraz przewody do płytki stykowej i złączek. |
| Zasilanie | Kabel USB-C **z transmisją danych** do ESP32, zasilanie USB konwertera; docelowo stabilne5V. |
| Komputer | Windows10/11 z dostępem do internetu i wolnym USB. |
| Czytnik karty | Potrzebny do opisanej najprostszej metody kopiowania własnych gier z komputera. Nie jest częścią konsolki. |

Nie zastępuj płytki przypadkowym ESP32 z napisem „4MB” — często oznacza to
flash, a nie RAM. Ten projekt wymaga PSRAM i konkretnego układu partycji.
ESP32-S3/C3 mają inne możliwości Bluetooth i wyprowadzenia obrazu; nie są
zamiennikiem w tej instrukcji.

## 2. Jak czytać piny i płytkę stykową

**Odłącz zasilanie przed wkładaniem/wyjmowaniem przewodów i karty.**
GPIO25 to pin opisany25/IO25; nie „dwudziesty piąty pin od góry”.
G/GND oznacza wspólną masę, czyli punkt odniesienia wszystkich sygnałów.
3V3 i5V to zasilanie — nie masa.

Na tej płytce pin **między5V aD3 to CMD, NIE GND**. Napis bywa trudny do
odczytania. Prawdziwe masy są m.in. **między12 i13** oraz **między19 i21**.
Nie podłączaj nic do CMD, CLK, D0–D3; są związane z pamięcią modułu.
GPIO16/17 są zajęte przez PSRAM; RX/TX pozostaw dla USB.
Porównaj z [oficjalnym schematem Waveshare](https://files.waveshare.com/wiki/ESP32-DEV-KIT-XX/ESP32-DEV-KIT-XX.pdf).

W płytce stykowej kilka otworów jest wewnętrznie połączonych. Kierunek
połączeń zależy od modelu; mały blok5×5 może łączyć kolumny, a duża płytka
typowo grupy pięciu otworów przy szczelinie. Nie zgaduj.
Przy odłączonym zasilaniu użyj brzęczyka miernika: piszczenie/około0Ω oznacza
połączenie. Różne węzły poniższego układu nie mogą być zwarte.

## 3. Najpierw sam obraz

Jeszcze nie podłączaj modułu SD ani audio. Przygotuj jedną złączkę RCA.

| ESP32 | Połączenie |
| --- | --- |
| GPIO25 | „+” złączki RCA, czyli środkowy styk video. |
| G/GND | „−” złączki, czyli zewnętrzny ekran RCA. |

1. Kabel RCA połącz z żółtym **CVBS/VIDEO IN** konwertera.
2. HDMI konwertera połącz z TV i wybierz to wejście HDMI.
3. Podłącz osobne zasilanie USB konwertera. Sam HDMI go nie zasila.
4. Ustaw TV w trybie obrazu **Gra**. Nazwa wejścia „Komputer” nie gwarantuje
   tego samego; w sprawdzonym TV Philips dopiero tryb Gra usunął latencję.
5. ESP32 na razie podłącz kablem USB do komputera. Obraz pojawi się dopiero
   po wgraniu firmware w etapie8; pusty ekran teraz jest normalny.

Program używa **NTSC**. Przełącznik720/1080 w konwerterze wybiera rozdzielczość
HDMI, nie standard sygnału z ESP32. Dany tani konwerter może nie akceptować
każdego nietypowego sygnału; sprawdzony prototyp działał z NTSC.

## 4. Zainstaluj programy na komputerze

Potrzebujesz [Python3](https://www.python.org/downloads/windows/),
[Arduino IDE2](https://www.arduino.cc/en/software/) i opcjonalnie
[Git](https://git-scm.com/download/win). Użyj Python3.13 — tę wersję sprawdzono; przy instalacji
włącz dostęp do `py`/PATH. Arduino IDE dostarcza używane tu Arduino CLI.

Otwórz PowerShell. Jeśli masz Git, pobierz kod:

```powershell
cd "$env:USERPROFILE\Projects"
git clone https://github.com/zbylon80/esp32-atari-console.git
cd esp32-atari-console
```

Możesz też pobrać ZIP z GitHub przez Code → Download ZIP, rozpakować i otworzyć
PowerShell w katalogu zawierającym README.md. Dalsze polecenia wykonuj stąd.

Utwórz osobne środowisko Pythona; nie trzeba zmieniać zasad uruchamiania skryptów:

```powershell
py -3.13 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

Sprawdź lokalizację CLI:

```powershell
$cli = "$env:ProgramFiles\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
Test-Path $cli
& $cli version
```

Oczekiwany wynik: `True` i numer wersji. Przy instalacji IDE w innym miejscu
wpisz właściwą ścieżkę do `$cli` i ustaw `$env:ARDUINO_CLI = $cli`.

Zainstaluj dokładnie sprawdzony pakiet płytki, a nie jego najnowszą wersję:

```powershell
$urls = "https://espressif.github.io/arduino-esp32/package_esp32_index.json,https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json"
& $cli core update-index --additional-urls $urls
& $cli core install esp32:esp32@2.0.17 --additional-urls $urls
& $cli core install esp32-bluepad32:esp32@4.1.0 --additional-urls $urls
& $cli core list
```

Lista powinna zawierać `esp32-bluepad32:esp32` w wersji4.1.0.
[Dokumentacja Bluepad32](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
wyjaśnia dodatkowy indeks. Nie instaluj zamiast niego zwykłej biblioteki
Bluetooth i nie zmieniaj platformy na S3.

## 5. Zapisz lokalną konfigurację Wi-Fi

```powershell
.\.venv\Scripts\python.exe tools/configure.py
```

Wpisz nazwę sieci2,4GHz i jej hasło. Hasło nie będzie wyświetlane podczas
wpisywania. Program zapisze `local/config.json`, w tym losowy token aktualizacji.
Ten plik jest ignorowany przez Git. Zachowaj go: późniejszy aktualizator musi
korzystać z tego samego tokenu. Router nie musi dawać konsoli dostępu do internetu
podczas gry; Wi-Fi działa w osobnym trybie aktualizacji.

Nie udostępniaj `local/`, kopii flash ani zbudowanego aktualizatora — binarium
zawiera dane Twojej sieci. Publiczne zgłoszenie błędu nie wymaga tych plików.

## 6. Skompiluj bez gier i bez oryginalnego ROM-u

```powershell
.\.venv\Scripts\python.exe tools/build.py
```

Pierwsze uruchomienie pobiera wyłącznie przypięte, sprawdzane SHA256 pliki
źródłowe emulatora i otwarte zamienniki Altirra. Nie pobiera gier ani
oryginalnego systemu Atari. Następnie kompiluje osobno konsolę i aktualizator.
To może potrwać kilka minut.

Oczekiwane: dwie linie `PASS`, dla `archon` oraz `updater`, i komunikat
o zakończeniu bez wgrywania. Pliki znajdują się pod `.build/`.
Nie trzeba mieć żadnej gry, żeby ten etap zakończył się poprawnie.

## 7. Rozpoznaj port USB i konkretną płytkę

Podłącz ESP32 sprawdzonym kablem danych. W Menedżerze urządzeń rozwiń
„Porty (COM i LPT)”. Waveshare używa mostka CH343; numer może być inny na
każdym komputerze. `Communications Port (COM1)` zwykle nie jest ESP32.

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports -v
.\.venv\Scripts\python.exe tools/board-info.py --port COM7
```

Zastąp COM7 swoim portem. Odczyt powinien pokazać ESP32 rev3 lub nowszy,
unikalny adres **MAC** i pamięć flash16MB. Zapisz MAC z tej konkretnej płytki.
Jeśli nie pojawia się nowy port, wymień kabel, spróbuj bez huba USB i sprawdź
sterownik CH343 producenta płytki. Kabel wyłącznie do ładowania nie wystarczy.

## 8. Pierwsze wgranie — tylko na nową płytkę

To odrębny etap od późniejszych aktualizacji. Skrypt zapisuje bootloader,
tablicę partycji, aplikację i osobny aktualizator. Zmieni dotychczasowy program
nowej płytki; wcześniej wykonuje kopię flash do `.build/backups/`.

Zamknij monitory portu szeregowego. Przykład, z **Twoim** COM i MAC:

```powershell
.\.venv\Scripts\python.exe tools/install-new-board.py --port COM7 --confirm-mac AA:BB:CC:DD:EE:FF --initialize-new-board
```

Skrypt porównuje MAC, sprawdza rewizję i flash16MB. Odmówi pierwszej instalacji,
jeśli wykryje już nasz układ partycji. Zachowuje kopię starych danych,
inicjalizuje wyłącznie nowy obszar wyboru aplikacji i zapisuje sprawdzone obrazy.
Nie używaj `erase_flash`, standardowego Upload z IDE ani porad każących zapisać
`boot_app0.bin` pod0xE000 — w naszym układzie ten adres należy do NVS.

Oczekiwany wynik po restarcie bez karty: ekran startowy, krótki komunikat o braku
odpowiedzi microSD, a następnie **Altirra BASIC / Ready**. To potwierdza pracę
emulatora i wyjścia obrazu. Klawiatura BT i pisanie programów nie są jeszcze
obsługiwane w tej wersji.

Jeśli obraz nie działa, zacznij od tabeli diagnostycznej na końcu. Nie dokładaj
jeszcze pozostałych przewodów, dopóki nie uzyskasz poprawnego obrazu.

## 9. Sparuj pierwszy, potem drugi pad

1. Pierwszy pad włącz przyciskiem Xbox. Drugi zostaw wyłączony.
2. Przytrzymaj mały przycisk parowania przy USB pada, aż logo szybko miga.
3. Poczekaj na połączenie z konsolą i naciśnij A. Pierwszy kontroler zostanie
   zapamiętany jako gracz1; stan jest zapisany w pamięci płytki.
4. View (dwa nachodzące kwadraty) otwiera menu BASIC-a. Sprawdź ruch krzyżakiem.
5. Jeśli używasz drugiego pada, dopiero teraz włącz go, uruchom parowanie i A.
   Będzie graczem2, także jeśli przy kolejnym starcie połączy się wcześniej.

Odłącz pada od komputera/konsoli Xbox, jeśli automatycznie wraca do innego
urządzenia. Najprościej tymczasowo wyłączyć tam Bluetooth. Nie czyść kluczy
parowania ESP32 w ramach zwykłej aktualizacji.

## 10. Dołącz microSD

Wyłącz zasilanie. Poniższe zasilanie dotyczy **aktywnego SDTF-02 ze
stabilizatorem3,3V i buforem**, jak w sprawdzonym prototypie. Gołego gniazda
microSD bez stabilizatora nie wolno zasilać5V. Dla innego modułu sprawdź jego
schemat — nadruk „microSD” nie określa napięcia wejściowego.

| SDTF-02 | ESP32 |
| --- | --- |
| SCK | GPIO22 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| CS | GPIO21 |
| VCC | 5V |
| GND | prawdziwe G/GND, np. między19 i21 — **nie CMD obok5V** |

Każdy sygnał może iść przewodem bezpośrednio między modułem a ESP32. Brak
wolnych otworów na małej płytce stykowej nie oznacza braku GPIO; większa płytka
ułatwia jedynie rozprowadzenie masy. Staraj się utrzymać przewody krótkie.

Karta powinna mieć FAT32. exFAT nie jest obsługiwany przez przypięte środowisko.
Jeśli na karcie są ważne pliki, zrób ich kopię i nie formatuj jej „na próbę”.
Najprościej użyć nowej/pustej karty4/8GB i wybrać FAT32 w oknie formatowania
Windows. Nie ma potrzeby kupowania16GB. Duża karta może działać, jeśli ma
odpowiedni system plików; sama pojemność nie jest testem poprawności.

## 11. Dodaj własne gry

Firmware nie pobiera ich z internetu. Znane nazwy, dokładne rozmiary i SHA256
są w `tools/wrover/library.json`. Różne wydania tej samej gry mogą mieć inny
loader i sterowanie; sama zmiana nazwy pliku nie uczyni go zgodną wersją.

1. Na komputerze utwórz folder z legalnie posiadanymi plikami, nazwanymi jak
   pole `file` w katalogu, np. `archon.car` czy `dropzone.xex`.
2. Włóż kartę do czytnika komputera. Upewnij się, która litera dysku do niej
   należy; poniżej użyto przykładowego E:\.
3. Sprawdź pliki i skopiuj znane, zgodne wersje:

```powershell
.\.venv\Scripts\python.exe tools/copy-games.py --source C:\MojeGryAtari --card E:\ --verify-only
.\.venv\Scripts\python.exe tools/copy-games.py --source C:\MojeGryAtari --card E:\
```

Skrypt nie formatuje karty i nie nadpisuje istniejącego innego pliku. Kopiuje
tylko pasujące obrazy do `atari-console/games/`. Pozostałe katalogi karty
pozostawia w spokoju. Jeśli gra ma inny SHA256, zostanie pominięta z komunikatem;
nie usuwaj sprawdzania sum po to, żeby ominąć niezgodność.

4. Bezpiecznie wysuń kartę z Windows. Przy wyłączonej konsoli włóż ją do modułu.
5. Włącz konsolę. Szybki skan sprawdza dostępność; brakujące tytuły są
   przygaszone. A na poprawnej grze uruchamia odczyt i kontrolę SHA256.

Cała biblioteka nie jest wczytywana na starcie. Podczas zmiany gry poprzednie
bufory są zwalniane; restart tego samego tytułu używa pamięci podręcznej.
Dla Seksmisji potrzebne są obie strony, ładowane razem.

**Misja i oba Spy vs. Spy wymagają opcjonalnego, własnego ROM-u Atari XL rev2.**
Bez niego są niedostępne również przy obecnych plikach gier. Jeśli go posiadasz,
umieść zgodny plik w `local/roms/atarixl.rom`, sprawdź sumę według katalogu,
ponownie zbuduj firmware i zaktualizuj konsolę. Nie wysyłaj tego ROM-u do repo.
Pozostałe profile korzystają z AltirraOS. Dodanie dowolnego nowego pliku na kartę
nie dodaje automatycznie tytułu do menu — edytowalny katalog jest kolejnym etapem.

## 12. Dołącz dźwięk mono w obu kanałach

Wyłącz zasilanie. Rezystor ma dwie równoważne nóżki; nie jest spolaryzowany.
Ceramiczny103 również nie ma kierunku. Zbuduj trzy osobne węzły:

```text
GPIO18 ----[ 1kΩ ]----+---- środek białego RCA --> AUDIO L IN
                     +---- środek czerwonego RCA --> AUDIO R IN
                     |
                   10nF (103)
                     |
GND -----------------+---- ekrany obu RCA
```

Jeśli korzystasz z małego bloku z połączonymi kolumnami:

| Węzeł/kolumna | Elementy |
| --- | --- |
| 1: sygnał za rezystorem | Nóżka rezystora, nóżka103, środki obu audio RCA. |
| 2: wyjście ESP32 | GPIO18 i druga nóżka rezystora. |
| 3: masa | GND, druga nóżka103, ekrany obu audio RCA. |

Kolumna1 nie może być zwarta z2 ani3. Rezystor łączy2→1, kondensator1→3.
Na mierniku w zakresie6k odczyt np.0,966 oznacza około966Ω. Rezystor mierz
odłączony od zasilania i najlepiej poza pozostałymi połączeniami.
Jeśli cienkie nóżki luźno siedzą w płytce, zapewnij pewne złącze lub polutuj
docelowy układ; nie licz na przypadkowy styk. Ten układ daje ten sam dźwięk
w L i R, nie stereo. Nie zajmuje drugiego GPIO.

## 13. Codzienne sterowanie i aktualizacja

- Krzyżak i lewy drążek: ruch/wybór; A oraz LT/RT: akcja/potwierdzenie.
- View, dwa kwadraty: menu konsoli; przycisk Menu ☰: Atari OPTION.
- X w bibliotece/pauzie: instrukcja; X lub lewo/prawo zmienia jej stronę.
- Tryb1P/2P, strony i opcje wybierasz w oryginalnych ekranach gier.
- Nie wyjmuj karty przy włączonym zasilaniu. Ponowne wykrycie biblioteki wymaga
  restartu; samo wsunięcie karty podczas BASIC-a nie przełącza trybu.

Nową wersję najpierw zbuduj na komputerze. Dopiero potem włącz na konsoli
View → Ustawienia → Aktualizacja Wi-Fi → A. Zapisz adres z ekranu, np.:

```powershell
.\.venv\Scripts\python.exe tools/update.py --host 192.168.1.50
```

Komputer i konsola muszą być w tej samej sieci. Użyj faktycznego adresu z TV,
nie przykładu. Aktualizator sprawdza token, format obrazu i SHA256. Nie zmienia
układu partycji i nie kasuje przypisań padów. Nie odłączaj zasilania podczas
wgrywania. Tryb aktualizacji ma limit czasu; jeśli wygasł, uruchom go ponownie.
Przycisk „sprawdź aktualizację z internetu” nie jest jeszcze zaimplementowany.

## 14. Gdy coś nie działa

| Objaw | Sprawdź po kolei |
| --- | --- |
| Brak nowego COM | Kabel danych, inny port USB, sterownik CH343, zasilanie. COM1 z nazwą Communications Port to zwykle inny port. |
| Port zajęty | Zamknij monitor portu IDE/miniterm przed odczytem i wgrywaniem. |
| USB przestaje działać po podłączeniu SD | Wyłącz zasilanie i sprawdź masę: CMD obok5V nie jest GND. Sprawdź też, czy przewód nie trafił na RX/TX. |
| Konwerter pokazuje niebieskie „No signal” | Jego osobne USB, kierunek AV→HDMI, właściwe HDMI w TV, środek RCA do25 i ekran do GND, ciągłość kabla, NTSC. |
| Pad reaguje z opóźnieniem | Tryb obrazu Gra w TV, zamiast samej etykiety wejścia „Komputer”; następnie bateria i połączenie pada. |
| BASIC mimo włożonej karty | Karta nie odpowiada: zasilanie właściwego modułu, prawdziwe GND, CS21/SCK22/MISO19/MOSI23, styki. BASIC nie oznacza, że karta została poprawnie odczytana. |
| Błąd biblioteki | FAT32 zamiast exFAT, poprawne ścieżki i nazwy; nie formatuj karty z ważnymi danymi bez ich kopii. |
| Przygaszona gra | Brak pliku, inny rozmiar albo wymagany oryginalny ROM nie jest dołączony do lokalnej kompilacji. |
| Błąd zawartości przy ładowaniu | SHA256 różni się od katalogu; użyj zgodnego wydania i sprawdź odczyt karty. |
| Dźwięk tylko z jednej strony | Oba środki L/R muszą być połączone z tym samym węzłem za rezystorem; oba ekrany do GND. |
| Aktualizacja nie odpowiada | Tryb Wi-Fi jest włączony, komputer w tej samej sieci, poprawny adres z ekranu i zachowany local/config.json. |

Log można odczytać bez programu terminalowego IDE:

```powershell
.\.venv\Scripts\python.exe -m serial.tools.miniterm COM7 115200
```

Wyjście: Ctrl+]. Przed zgłoszeniem problemu skopiuj komunikat błędu i nazwy
wersji; nie dołączaj `local/config.json`, haseł, kopii flash ani plików gier.

## Źródła techniczne

- [Płytka i jej warianty](https://docs.waveshare.com/ESP32-DEV-KIT-XX)
- [Schemat Waveshare](https://files.waveshare.com/wiki/ESP32-DEV-KIT-XX/ESP32-DEV-KIT-XX.pdf)
- [Bluepad32 dla Arduino](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
- [Oryginalny backend ESP_8_BIT](https://github.com/rossumur/esp_8_bit)
