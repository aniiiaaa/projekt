OPIS PROJEKTU TEMAT 12: AUTOBUS PODMIEJSKI

github: https://github.com/Aaannniiiaa/so_projekt_rotarska.git

O czym jest projekt:
Projekt polega na symulacji autobusu podmiejskiego. Na dworzec przychodzą pasażerowie (w róznym wieku, niektórzy są VIP, niektórzy mają rowery). Następnie kupują bilety w kasie i wchodzą do autobusu. Pojazd ma ograniczoną ilość miejsc dla ludzi i rowerów. Autobus jeździ i wraca po pewnym czasie.

Procesy w projekcie:
W projekcie mamy cztery procesy:

Kasa - sprzedaje bilety, obsługują VIP-ów, rejestruje wchodzące osoby

Dyspozytor - wysyła sygnały sterujące autobusem i pasażerami:
- sygnał 1 - autobus, który stoi mozę odjechać z niepełną liczbą pasażerów - sygnał 2 - pasażerowie nie mogą wsiaść do żadnego autobusu, nie mogą wejść na dworzec

Kierowca - pilnuje wejść w momencie odjazdu, sprawdza limity miejsc i rowerów, odjeżdża co określony czas

Pasażerowie - każdy jest innym procesem, kupuje bilet, przy wejściu do autobusu okazuje bilet, dzieci do lat 8 wchodzą z opiekunem

Zasady działania:
Autobus ma pojemność P osób i R rowerów. Ma dwa wejścia (jedno dla pasażerów z bagażem podręcznym, a drugie dla pasażerów z rowerami), które mieszczą jedną osobę. Odjeżdża co T czasu. W moemncie odjazdu wejścia muszą być puste. Autobus wraca po losowej wartosci Ti czasu i na jego miejsce pojawia się kolejny (maksymalnie N autobusów). Aby wejść do autobusu pasażer ma mieć wcześniej kupiony bilet.

Testy:
Test 1 - testuję czy przy zbyt dużej ilości pasażerów nadmiar ich wejdzie

Test 2 - testuję czy przy zbyt dużej ilości pasażerów z rowerami nadmiar ich wejdzie

Test 3 - testuję czy na sygnał 1 od dyspozytora autobus odjedzie (nawet jeśli jest niepełny)

Test 4 - testuję czy na sygnał 2 nikt nie wejdzie do autobusu

Test 5 - testuję czy autobus odjedzie co czas T

