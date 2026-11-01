# Korzystanie z konsolki

Krzyżak i lewy drążek mają to samo zadanie. A jest główną akcją; LT i RT
działają jak A. View (dwa kwadraty) otwiera menu konsoli. Menu ☰ zawsze
wysyła Atari OPTION. RB/LB nie otwierają nakładek emulatora.

W bibliotece A wczytuje wybraną grę. X pokazuje instrukcję „Cel i zasady”
oraz „Sterowanie”; zmieniasz stronę X lub lewo/prawo. B/View zamyka instrukcję.
Lewo/prawo w bibliotece zmienia stronę listy.

Liczbę graczy, stronę i wariant wybierasz na oryginalnych ekranach gry.
Konsolka nie wymusza własnego trybu Archona. Pierwszy zapisany pad jest
graczem1, drugi graczem2, niezależnie od kolejności ponownego połączenia.

## Biblioteka

Katalog zawiera nazwy i instrukcje: Archon, Archon II, Blue Max, Boulder Dash,
Bruce Lee, Dropzone, Fred, Hans Kloss, International Karate, Misja,
Montezuma's Revenge, RealSports Tennis, River Raid, Robbo, Seksmisja,
Spy vs. Spy, Spy vs. Spy II, The Goonies, Tron (Mirage) i Zorro.
Ich pliki nie są dołączone ani pobierane. Brakujące pliki są przygaszone.

Przy starcie wykonywany jest skan obecności/rozmiarów, nie odczyt całych gier.
Dopiero wybór wczytuje tytuł do PSRAM i sprawdza SHA256. Podczas gry nie ma
odczytów microSD. Restart używa bieżącej kopii w pamięci.

Seksmisja ma dwie strony, wczytywane razem. Gdy gra poprosi o zmianę dysku,
otwórz View i wybierz WŁÓŻ STRONĘ A/B. Nie restartuj gry.
Misja i oba Spy wymagają zgodnego, samodzielnie dostarczonego ROM-u Atari XL
w lokalnej kompilacji. Bez tego te trzy profile pozostają niedostępne.

Bez odpowiedzi karty konsolka uruchamia Altirra BASIC. „Brak odpowiedzi”
może też oznaczać odłączony moduł lub błąd przewodów; nie jest fizycznym
czujnikiem obecności karty. Błędy FAT/plików pozostają komunikatami błędów.
View w BASIC-u daje powrót do programu i ustawienia. Klawiatura BT i zapis
własnych programów nie są jeszcze zaimplementowane.

## Aktualizacje i ograniczenia

Najpierw zbuduj nowy firmware. Potem wybierz View → Ustawienia → Aktualizacja
Wi-Fi → A i uruchom `tools/update.py --host ADRES_Z_EKRANU` z komputera
w tej samej sieci. Zachowaj `local/config.json` z pierwotnym tokenem.
Poza tym trybem konsola nie uruchamia Wi-Fi.

Nie ma jeszcze edytowalnego katalogu gier, panelu zarządzania padami ani
sprawdzania nowych wydań z internetu. Samo dodanie dowolnego pliku do karty
nie tworzy nowej pozycji menu. Nazwy, profile i instrukcje są we firmware.

Kartę zmieniaj przy wyłączonym zasilaniu; pełne ponowne wykrycie wymaga startu
konsoli. Zasilany osobno konwerter AV→HDMI i tryb obrazu Gra w TV są istotne
dla poprawnego obrazu i małego opóźnienia. Dźwięk L/R to wspólne mono.

Budowę i polecenia opisuje [instrukcja od zera](getting-started.md).
