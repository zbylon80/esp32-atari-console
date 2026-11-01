# Wykorzystane projekty i licencje

[← Strona projektu](README.md)

Własny kod konsoli jest dostępny na licencji [MIT](LICENSE).
Rdzeń emulatora, obsługa Bluetooth i pozostałe zależności zachowują swoje
licencje. MIT nie obejmuje całego wynikowego programu.

## Źródła kodu

| Projekt | Zastosowanie | Licencja |
| --- | --- | --- |
| [ESP_8_BIT](https://github.com/rossumur/esp_8_bit), Peter Barrett | Generowanie obrazu, adapter emulatora i font | ISC |
| [Atari800](https://github.com/atari800/atari800) | Emulacja Atari | GPL-2.0-or-later |
| AltirraOS i Altirra BASIC, Avery Lee | Otwarte zamienniki systemu i BASIC-a | Zezwolenia w nagłówkach poszczególnych plików |
| [Bluepad32](https://github.com/ricardoquesada/bluepad32), wersja 4.1.0 | Obsługa padów | Apache-2.0 |
| [BTstack](https://github.com/bluekitchen/btstack), BlueKitchen | Stos Bluetooth | Odrębne warunki BTstack |
| Arduino-ESP32 i ESP-IDF | Obsługa płytki i mikrokontrolera | Licencje dołączone do pakietu płytki |

Kod ESP_8_BIT i zawarty w nim rdzeń Atari800 pochodzą z rewizji zapisanej
w [upstream-lock.json](tools/wrover/upstream-lock.json). Zachowujemy oryginalne
nagłówki licencyjne, w tym noty ISC w kodzie obrazu i fontu.
Pełny tekst GPL znajduje się w [LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt).

AltirraOS i Altirra BASIC są dostarczane w plikach `roms_altirra*.c`.
Ich nagłówki zezwalają na kopiowanie i dystrybucję z zachowaniem noty.
Są to otwarte zamienniki, a nie oryginalne ROM-y Atari.

Bluepad32 jest instalowany jako zależność. Korzysta z BTstack, którego
warunki obowiązują niezależnie od licencji własnego kodu konsoli.
Wykorzystanie BTstack w produkcie komercyjnym wymaga ustalenia warunków
z BlueKitchen.

## Gry i ROM-y

Automatycznie pobieramy wyłącznie źródła wymienione w pliku blokady,
weryfikując ich sumy SHA-256. Obejmuje to emulator i zamienniki Altirra.
Pomijamy pakiet gier `media.h` oraz katalog `data` z oryginalnego projektu.

Repozytorium nie zawiera gier ani oryginalnych ROM-ów Atari. Nazwy tytułów,
instrukcje i sumy kontrolne służą do rozpoznawania plików dostarczonych
przez użytkownika. Opcjonalny, własny ROM Atari XL jest używany wyłącznie
przy lokalnej kompilacji trzech profili zgodności.

Nie publikujemy gotowych obrazów firmware. Dystrybucja programu zawierającego
Atari800 podlega obowiązkom GPL oraz warunkom pozostałych zależności.
