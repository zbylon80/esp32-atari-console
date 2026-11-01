# ESP32 Atari Console

Konsolka Atari XL na klasycznym ESP32 WROVER: obraz NTSC przez RCA,
dźwięk mono w obu kanałach, dwa pady Xbox Bluetooth, gry na microSD,
menu z instrukcjami i Altirra BASIC przy braku odpowiadającej karty.

**Zacznij od [instrukcji krok po kroku](docs/getting-started.md).**
Prowadzi od wyboru części przez przewody i kartę po kompilację oraz pierwszą grę.

Repo zawiera aktualny kod, katalog nazw/ustawień i instrukcje 20 tytułów.
**Nie zawiera gier, oryginalnych ROM-ów Atari, haseł, gotowych binariów,
archiwów projektu ani prototypów PC.** Firmware można zbudować bez plików
gier i bez oryginalnego ROM-u, korzystając z otwartych zamienników Altirra.
Brakujące gry są przygaszone; użytkownik dostarcza własne pliki.

## Sprawdzony sprzęt

- Waveshare ESP32-DEV-KIT-WROVER-E-M, SKU35528; ESP32 ECO3, flash16MB,
  PSRAM8MB (około4MB w bieżącej mapie pamięci).
- Xbox Wireless Controller model1914; maksymalnie dwa kontrolery.
- SDTF-02 ze stabilizatorem3,3V, zasilany5V; karta FAT32.
- CVBS → HDMI z własnym zasilaniem, telewizor w trybie obrazu Gra.

To projekt dla klasycznego ESP32 z PSRAM, nie gotowa konfiguracja dla S3/C3.
Skan biblioteki trwał około1,1sekundy w sprawdzonym zestawie. Dopiero wybór
gry ładuje jej pliki do PSRAM i sprawdza SHA256; podczas gry czytnik nie pracuje.

## Dokumentacja

- [Budowa od zera](docs/getting-started.md)
- [Sterowanie, gry i ograniczenia](docs/user-guide.md)
- [Praca nad kodem i testy](docs/development.md)
- [Licencje zależności](THIRD_PARTY.md)

Nasz własny kod: MIT. Zależności zachowują swoje licencje; połączony firmware
nie jest w całości na MIT. Czytaj [THIRD_PARTY.md](THIRD_PARTY.md).
