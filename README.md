# Bomberman

### Short project description in English

This is a project for "Computer Networks".
It contains a TCP client for a simple version of a popular game "bomberman".
It also contains a TCP "bomberman" server, but the server is unfinished.

# Bombowe roboty

### 0.1. GUI

Interfejs graficzny dla gry był udostępniony w ramach projektu

#### Sterowanie

```
W, strzałka w górę -  porusza robotem w górę.
S, strzałka w dół - porusza robotem w dół.
A, strzałka w lewo - porusza robotem w lewo.
D, strzałka w prawo - porusza robotem w prawo.
Spacja, J, Z - kładzie bombę.
K, X - blokuje pole.
```

## 1. Gra Bombowe roboty

### 1.1. Zasady gry

Gra rozgrywa się na prostokątnym ekranie. Uczestniczy w niej co najmniej jeden gracz. Każdy z graczy steruje ruchem robota. Gra rozgrywa się w turach. Trwa ona przez z góry znaną liczbę tur. W każdej turze robot może:

- nic nie zrobić
- przesunąć się na sąsiednie pole (o ile nie jest ono zablokowane)
- położyć pod sobą bombę
- zablokować pole pod sobą

Gra toczy się cyklicznie - po uzbieraniu się odpowiedniej liczby graczy rozpoczyna się nowa rozgrywka na tym samym serwerze. Stan gry przed rozpoczęciem rozgrywki będziemy nazywać `Lobby`.

### 1.2. Architektura rozwiązania

Na grę składają się trzy komponenty: serwer, klient, serwer obsługujący interfejs użytkownika. Należy zaimplementować serwer i klienta. Aplikację implementującą serwer obsługujący graficzny interfejs użytkownika (ang. _GUI_) dostarczamy.

Serwer komunikuje się z klientami, zarządza stanem gry, odbiera od klientów informacje o wykonywanych ruchach oraz rozsyła klientom zmiany stanu gry. Serwer pamięta wszystkie zdarzenia dla bieżącej partii i przesyła je w razie potrzeby klientom.

Klient komunikuje się z serwerem gry oraz interfejsem użytkownika.

Zarówno klient jak i serwer mogą być wielowątkowe.

Specyfikacje protokołów komunikacyjnych, rodzaje zdarzeń oraz formaty komunikatów i poleceń są opisane poniżej.

### 1.3. Parametry wywołania programów

Serwer:

```
    -b, --bomb-timer <u16>
    -c, --players-count <u8>
    -d, --turn-duration <u64, milisekundy>
    -e, --explosion-radius <u16>
    -h, --help                                   Wypisuje jak używać programu
    -k, --initial-blocks <u16>
    -l, --game-length <u16>
    -n, --server-name <String>
    -p, --port <u16>
    -s, --seed <u32, parametr opcjonalny>
    -x, --size-x <u16>
    -y, --size-y <u16>
```

Klient:

```
    -d, --gui-address <(nazwa hosta):(port) lub (IPv4):(port) lub (IPv6):(port)>
    -h, --help                                 Wypisuje jak używać programu
    -n, --player-name <String>
    -p, --port <u16>                           Port na którym klient nasłuchuje komunikatów od GUI
    -s, --server-address <(nazwa hosta):(port) lub (IPv4):(port) lub (IPv6):(port)>
```

Wystarczy zaimplementować rozpoznawanie krótkich (`-c`, `-x` itd.) parametrów.

## 2. Protokół komunikacyjny pomiędzy klientem a serwerem

Wymiana danych odbywa się po TCP. Przesyłane są dane binarne, zgodne z poniżej zdefiniowanymi formatami komunikatów. W komunikatach wszystkie liczby przesyłane są w sieciowej kolejności bajtów, a wszystkie napisy muszą być zakodowane w UTF-8 i mieć długość krótszą niż 256 bajtów.

Napisy (String) mają następującą reprezentację binarną: `[1 bajt określający długość napisu w bajtach][bajty bez ostatniego bajtu zerowego]`.

Listy są serializowane w postaci `[4 bajty długości listy][elementy listy]`. Mapy są serializowane w postaci `[4 bajty długości mapy][klucz][wartość][klucz][wartość]...`.

Pola w strukturze serializowane są bezpośrednio po bajcie oznaczającym typ struktury.

### 2.1. Komunikaty od klienta do serwera

```
enum ClientMessage {
    [0] Join { name: String },
    [1] PlaceBomb,
    [2] PlaceBlock,
    [3] Move { direction: Direction },
}
```

Typ Direction ma następującą reprezentację binarną:

```
enum Direction {
    [0] Up,
    [1] Right,
    [2] Down,
    [3] Left,
}
```

Klient po podłączeniu się do serwera zaczyna obserwować rozgrywkę, jeżeli ta jest w toku. W przeciwnym razie może zgłosić chęć wzięcia w niej udziału, wysyłając komunikat `Join`.

Serwer ignoruje komunikaty `Join` wysłane w trakcie rozgrywki. Serwer ignoruje również komunikaty typu innego niż `Join` w `Lobby`.

### 2.2. Komunikaty od serwera do klienta

```
enum ServerMessage {
    [0] Hello {
        server_name: String,
        players_count: u8,
        size_x: u16,
        size_y: u16,
        game_length: u16,
        explosion_radius: u16,
        bomb_timer: u16,
    },
    [1] AcceptedPlayer {
        id: PlayerId,
        player: Player,
    },
    [2] GameStarted {
            players: Map<PlayerId, Player>,
    },
    [3] Turn {
            turn: u16,
            events: List<Event>,
    },
    [4] GameEnded {
            scores: Map<PlayerId, Score>,
    },
}
```

Wiadomość od serwera typu `Turn`

```
ServerMessage::Turn {
        turn: 44,
        events: [
            Event::PlayerMoved {
                id: PlayerId(3),
                position: Position(2, 4),
            },
            Event::PlayerMoved {
                id: PlayerId(4),
                position: Position(3, 5),
            },
            Event::BombPlaced {
                id: BombId(5),
                position: Position(5, 7),
            },
        ],
```

### 2.3. Definicje użytych powyżej rekordów

```
Event:
[0] BombPlaced { id: BombId, position: Position },
[1] BombExploded { id: BombId, robots_destroyed: List<PlayerId>, blocks_destroyed: List<Position> },
[2] PlayerMoved { id: PlayerId, position: Position },
[3] BlockPlaced { position: Position },

BombId: u32
Bomb: { position: Position, timer: u16 },
PlayerId: u8
Position: { x: u16, y: u16 }
Player: { name: String, address: String }
Score: u32
```

Pole `address` w strukturze `Player` może reprezentować zarówno adres IPv4, jak i adres IPv6.

Liczba typu `Score` informuje o tym, ile razy robot danego gracza został zniszczony.

### 2.5. Stan gry

Serwer jest „zarządcą” stanu gry, do klientów przesyła informacje o zdarzeniach. Klienci je agregują i przesyłają zagregowany stan do interfejsu użytkownika. Interfejs nie przechowuje w ogóle żadnego stanu.

Serwer powinien przechowywać następujące informacje:

- lista graczy (nazwa, adres IP, numer portu)
- stan generatora liczb losowych (innymi słowy stan generatora NIE restartuje się po każdej rozgrywce)

Oraz tylko w przypadku toczącej się rozgrywki:

- numer tury
- lista wszystkich tur od początku rozgrywki
- pozycje graczy
- liczba śmierci każdego gracza
- informacje o istniejących bombach (pozycja, czas)
- pozycje istniejących bloków

Lewy dolny róg planszy ma współrzędne `(0, 0)`, odcięte rosną w prawo, a rzędne w górę.

Klient powinien przechowywać zagregowany stan tak, aby móc wysyłać komunikaty do GUI. W szczególności klient powinien pamiętać, ile razy dany robot został zniszczony (aby móc wysłać tę informację w polu `scores`).

### 2.6. Podłączanie i odłączanie klientów

Klient wysyła komunikat `Join` do serwera po otrzymaniu dowolnego (poprawnego) komunikatu od GUI, o ile klient jest w stanie `Lobby` (tzn. nie otrzymał od serwera komunikatu `GameStarted`).

Po podłączeniu klienta do serwera serwer wysyła do niego komunikat `Hello`. Jeśli rozgrywka jeszcze nie została rozpoczęta, serwer wysyła komunikaty `AcceptedPlayer` z informacją o podłączonych graczach. Jeśli rozgrywka już została rozpoczęta, serwer wysyła komunikat `GameStarted` z informacją o rozpoczęciu rozgrywki, a następnie wysyła wszystkie dotychczasowe komunikaty `Turn`.

Jeśli rozgrywka nie jest jeszcze rozpoczęta, to wysłanie przez klienta komunikatu `Join` powoduje dodanie go do listy graczy. Serwer następnie rozsyła do wszystkich klientów komunikat `AcceptedPlayer`.

Graczom nadawane jest ID w kolejności podłączenia (tzn. odebrania komunikatu `Join` przez serwer). Dwoje graczy może mieć taką samą nazwę. Ponieważ klienci łączą się z serwerem po TCP, wiadomo który komunikat przychodzi od którego klienta.

Odłączenie gracza w trakcie rozgrywki powoduje tylko tyle, że jego robot przestaje się ruszać. Odłączenie klienta-gracza przed rozpoczęciem rozgrywki nie powoduje skreślenia go z listy graczy. Odłączenie klienta-obserwatora nie wpływa na działanie serwera.

### 2.7. Rozpoczęcie partii i zarządzanie podłączonymi klientami

Partia rozpoczyna się, gdy odpowiednio wielu graczy się zgłosi. Musi być dokładnie tylu graczy, ile jest wyspecyfikowane przy uruchomieniu serwera.

Inicjacja stanu gry przebiega następująco:

```
nr_tury = 0
zdarzenia = []

dla każdego gracza w kolejności id:
    pozycja_x_robota = random() % szerokość_planszy
    pozycja_y_robota = random() % wysokość_planszy

    dodaj zdarzenie `PlayerMoved` do listy

tyle razy ile wynosi parametr `initial_blocks`:
    pozycja_x_bloku = random() % szerokość_planszy
    pozycja_y_bloku = random() % wysokość_planszy

    jeśli nie ma bloku o pozycji (pozycja_x_bloku, pozycja_y_bloku) na liście:
        dodaj zdarzenie BlockPlaced do listy

wyślij komunikat `Turn`
```

### 2.8. Przebieg partii

Zasady:

- Nie ma ograniczenia na liczbę bloków i bomb.
- Gracze nie mogą wchodzić na pole, które jest zablokowane. Mogą natomiast z niego zejść, jeśli znajdą się na nim, wskutek zablokowania go lub „odrodzenia" się na nim.
- Gracze nie mogą wychodzić poza planszę.
- Wielu graczy może zajmować to samo pole.
- Bomby mogą zajmować to samo pole.
- Gracze mogą położyć bombę, nawet jeśli stoją na zablokowanym polu (czyli na jednym polu może być blok, wielu graczy i wiele bomb)
- Na danym polu może być maksymalnie jeden blok

```
zdarzenia = []

dla każdej bomby:
    zmniejsz jej licznik czasu o 1
    jeśli licznik wynosi 0:
        zaznacz, które bloki znikną w wyniku eksplozji
        zaznacz, które roboty zostaną zniszczone w wyniku eksplozji
        dodaj zdarzenie `BombExploded` do listy
        usuń bombę

dla każdego gracza w kolejności id:
    jeśli robot nie został zniszczony:
        jeśli gracz wykonał ruch:
            obsłuż ruch gracza i dodaj odpowiednie zdarzenie do listy
    jeśli robot został zniszczony:
        pozycja_x_robota = random() % szerokość_planszy
        pozycja_y_robota = random() % wysokość_planszy

        dodaj zdarzenie `PlayerMoved` do listy

zwiększ nr_tury o 1
```

W wyniku eksplozji bomby zostają zniszczone wszystkie roboty w jej zasięgu oraz jedynie najbliższe bloki w jej zasięgu. Eksplozja bomby ma kształt krzyża o długości ramienia równej parametrowi `explosion_radius`. Jeśli robot stoi na bloku, który zostanie zniszczony w wyniku eksplozji, to taki robot również jest niszczony.

Intuicyjnie oznacza to, że można się schować za blokiem, ale położenie bloku pod sobą nie chroni przed eksplozją.

Eksplozja jednej bomby nie powoduje eksplozji bomb sąsiednich. Jeśli kilka bomb wybucha w jednej turze, to skutki eksplozji są sumą teoriomnogościową pojedynczych eksplozji rozpatrywanych osobno. W powyższym przykładzie widać że blok o współrzędnych (0, 1) nie został zniszczony.

### 2.9. Wykonywanie ruchu

Serwer przyjmuje informacje o ruchach graczy w następujący sposób: przez `turn_duration` milisekund oczekuje na informacje od graczy. Jeśli gracz w tym czasie nie wyśle odpowiedniej wiadomości, to w danej turze jego robot nic nie robi. Jeśli w tym czasie gracz wyśle więcej niż jedną wiadomość, to pod uwagę brana jest tylko ostatnia.

To serwer decyduje o tym, czy dany ruch jest dozwolony czy nie. Jeśli gracz stojący na krawędzi planszy wyśle komunikat, który spowodowałby wyjście robota poza planszę, to serwer komunikat ignoruje. Podobnie jeśli spróbuje wejść na zablokowane pole.

### 2.10. Kończenie rozgrywki

Po `game_length` turach serwer wysyła do wszystkich klientów wiadomość `GameEnded` i wraca do stanu `Lobby`. Klienci, którzy byli do tej pory graczami, przestają nimi być, ale oczywiście mogą się z powrotem zgłosić przy pomocy komunikatu `Join`. Wszystkie komunikaty otrzymane w czasie ostatniej tury rozgrywki są ignorowane.

## 3. Protokół komunikacyjny pomiędzy klientem a interfejsem użytkownika

Komunikacja z interfejsem odbywa się po UDP przy użyciu komunikatów serializowanych tak jak wyżej.

Klient wysyła do interfejsu graficznego następujące komunikaty:

```
enum DrawMessage {
    [0] Lobby {
        server_name: String,
        players_count: u8,
        size_x: u16,
        size_y: u16,
        game_length: u16,
        explosion_radius: u16,
        bomb_timer: u16,
        players: Map<PlayerId, Player>
    },
    [1] Game {
        server_name: String,
        size_x: u16,
        size_y: u16,
        game_length: u16,
        turn: u16,
        players: Map<PlayerId, Player>,
        player_positions: Map<PlayerId, Position>,
        blocks: List<Position>,
        bombs: List<Bomb>,
        explosions: List<Position>,
        scores: Map<PlayerId, Score>,
    },
}
```

Explosions w komunikacie `Game` to lista pozycji, na których robot by zginął, gdyby tam stał.

Klient powinien wysłać taki komunikat po każdej zmianie stanu (tzn. otrzymaniu wiadomości `Turn` jeśli rozgrywka jest w toku lub `AcceptedPlayer` jeśli rozgrywka się nie toczy).

Interfejs wysyła do klienta następujące komunikaty:

```
enum InputMessage {
    [0] PlaceBomb,
    [1] PlaceBlock,
    [2] Move { direction: Direction },
}
```

Są one wysyłane za każdym razem, gdy gracz naciśnie odpowiedni przycisk.

Można założyć, że komunikaty zmieszczą się w jednym datagramie UDP. Każdy komunikat wysyłany jest w osobnym datagramie.
