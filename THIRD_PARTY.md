# Licencje i pochodzenie

MIT w pliku LICENSE dotyczy naszego własnego kodu. Nie zmienia licencji
zależności ani warunków dystrybucji połączonego firmware.

| Składnik | Pochodzenie i licencja |
| --- | --- |
| ESP_8_BIT: backend obrazu, adapter, font | Peter Barrett, [repozytorium](https://github.com/rossumur/esp_8_bit), rewizja wskazana w `tools/wrover/upstream-lock.json`; nagłówki ISC zachowano, także w `backend/video_out.h`. |
| Atari800 | [Projekt Atari800](https://github.com/atari800/atari800), kod z powyższej przypiętej rewizji ESP_8_BIT; GPL-2.0-or-later, oryginalne nagłówki pozostają w pobranych źródłach. Tekst licencji: `LICENSES/GPL-2.0.txt`. |
| AltirraOS i Altirra BASIC | Avery Lee; otwarte zamienniki ROM-u dostarczane jako pliki `roms_altirra*.c` z Atari800. Każdy plik zawiera pozwolenie na kopiowanie i dystrybucję z zachowaniem noty. Nie są oryginalnym ROM-em Atari. |
| Bluepad32 4.1.0 | [Bluepad32](https://github.com/ricardoquesada/bluepad32), Apache-2.0; instalowany jako zależność, nie kopiowany do repo. |
| BTstack | [BlueKitchen](https://github.com/bluekitchen/btstack); odrębne warunki BTstack. MIT naszego kodu nie daje licencji komercyjnej BTstack. Warunki produktu komercyjnego należy ustalić z BlueKitchen. |
| Arduino-ESP32 / ESP-IDF | Dostarczane z przypiętym pakietem płytki; zachowują własne licencje i noty. |

Automatyczne pobieranie ogranicza się do plików źródłowych wymienionych w
blokadzie SHA256. Pobierany jest kod emulatora, w tym otwarte zamienniki Altirra.
Nie pobieramy upstreamowego `media.h`, folderu `data`, gier ani oryginalnego
ROM-u Atari. Ich obrazy i katalogi robocze nie wchodzą do Gita.

Samodzielnie dostarczony oryginalny ROM jest opcjonalny i służy lokalnej
kompilacji trzech profili zgodności. Nazwy, instrukcje i sumy plików gier są
metadanymi; nie stanowią pakietu gier. Połączony program z Atari800 podlega
obowiązkom GPL i licencjom innych zależności — nie należy traktować go jako
firmware dostępnego w całości na MIT. Nie publikujemy gotowych binariów.
