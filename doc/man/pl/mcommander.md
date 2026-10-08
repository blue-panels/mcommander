---
date: wrzesień 2026
---

# NAZWA <!-- help:skip -->

mcommander - dwupanelowy menedżer plików w trybie tekstowym

# UŻYTKOWANIE <!-- help:skip -->

**mcommander**
[-abcCdfPstuUVx] [-l log] [kat1 [kat2]] [-v plik]

# OPIS <a id="description"></a>

M-Commander jest dwupanelowym menedżerem plików w trybie tekstowym, opartym na
GNU Midnight Commanderze. Jego architekturę tworzy zwarte jądro i dynamicznie
wczytywane wtyczki paneli. Wtyczki zapewniają jednolity interfejs panelowy dla
archiwów, zdalnych systemów plików, repozytoriów i innych źródeł danych.
Polecenia są wykonywane we wbudowanym terminalu. M-Commander zawiera także
edytor tekstu z podświetlaniem składni oraz przeglądarkę obsługującą formaty
tekstowe i binarne.


# OPCJE <a id="options"></a>

*-a*
: Wyłącza używanie symboli graficznych przy rysowaniu ramek.

*-b*
: Wymusza wyświetlanie czarno-białe.

*-c*
: Wymusza wyświetlanie w kolorze, zobacz sekcję
**Kolory**
żeby zasięgnąć szerszej informacji.

*-d*
: Wyłącza używanie myszy.

*-f*
: Wyświetla wkompilowane ścieżki, w których M-Commander szuka swoich
plików.

*-k*
: Resetuje "miękkie" klawisze do ich standardowych funkcji z termcap/terminfo.
Użyteczne tylko przy terminalach HP, kiedy klawisze funkcyjne nie działają.

*-l plik*
: Zachowuje logi z serwerów ftp do pliku
*plik.*

*-P*
: Przy zakończeniu programu, M-Commander wydrukuje na ekranie katalog,
w którym pracowaliśmy na końcu; to w połaczeniu z funkcją napisaną poniżej
pozwoli ci na przeglądanie swoich katalogów i automatyczne przejście do
tego,
w którym byłeś ostatnio (dziękuję Torbenowi Fjerdingstadowi i Sergeyowi za wkład
w tę funkcję oraz za kod źródłowy, który wprowadzili w życie).

<!-- -->

```
użytkownicy basha i zsh:

mcommander ()
{
        MC=$HOME/tmp/mc$$-"$RANDOM"
        {{bindir}}/mcommander -P "$@" > "$MC"
        cd "`cat $MC`"
        rm "$MC"
        unset MC;
}

użytkownicy tcsh:
alias mcommander 'setenv MC `{{bindir}}/mcommander -P !*`; cd $MC; unsetenv MC'
```

Wiem, że ta funkcja mogłaby być krótsza dla basha i zsh, ale małe cudzysłowy
nie zaakceptowały by zawieszenia programu kombinacją
**C-z**.

*-s*
: Włącza tryb powolnego terminala, w którym program nie będzie rysował zbyt
obciążających znaków graficznych oraz wyłączy opcję weryfikacji.

*-t*
: Używane tylko jeśli kod był skompilowany przy użyciu S-Langa i terminfo:
powoduje, że M-Commander będzie używać zmiennej środowiskowej
**TERMCAP**
do pokazywania informacji terminala, zamiast informacji w systemowej bazie
typów terminali.

*-u*
: Wyłącza używanie równoległej powłoki (ma sens tylko jeśli
M-Commander był kompilowany z obsługą równoległych powłok).

*-U*
: Włącza użycie jednoczesnego inerpretatora poleceń (ma sens tylko jeśli
M-Commander był zbudowany z ustawieniem powłoki w tle jako opcji dodatkowej).

*-v plik*
: Włącza wbudowany podgląd w celu obejrzenia wybranego pliku
*plik.*

*-V*
: Wyświetla wersję programu.

*-x*
: Wymusza włączenie trybu xterm. Używane kiedy działa się na terminalach wyposażonych
w opcje xterm (dwa tryby ekranu i możliwość wysyłania myszą sygnałów wyjścia).

*-X, --no-x11*
: Do not use X11 to get the state of modifiers Alt, Ctrl, Shift

*-g, --oldmouse*
: Force a "normal tracking" mouse mode. Used when running on
xterm-capable terminals (tmux/screen).

Jeśli wybrano, pierwszy katalog używany jest do wyświetlenia w pierwszym panelu.
Drugi wyświetlany jest w drugim panelu.

# Opis <a id="overview"></a>

Ekran M-Commandera podzielony jest na cztery części. Prawie cały obszar
ekranu zajmują dwa panele. Standardowo przedostatnia od dołu linijka ekranu,
przeznaczona jest do wpisywania poleceń, a ostatnia pokazuje klawisze funkcyjne.
Najwyższy wiersz jest wierszem menu. Może on być niewidoczny, ale pojawia się zawsze
po kliknięciu w najwyższą linię ekranu, albo po wciśnięciu klawisza F9.

M-Commander pozwala na oglądanie dwóch paneli w tym samym czasie.
Jeden z nich jest panelem aktywnym (podświetlona linia wyboru znajduje się właśnie
w nim). Niemal wszystkie operacje wykonuje się na panelu aktywnym.
Niektóre operacje, jak
np. kopiowanie, zmiana nazwy używają jako domyślnego miejsca docelowego
katalogu otwartego w panelu nieaktywnym
(nie martw się, zawsze zostaniesz poproszony o
potwierdzenie takiej operacji). W celu zasięgnięcia szerszych informacji zajrzyj
do działów
**Panele katalogów**, **Lewe i prawe menu** oraz **Menu plików**.

Możesz wywoływać dowolne komendy systemowe po prostu wpisując je. Wszystko co
piszesz pojawia się w linii poleceń i po naciśnięciu klawisza Enter zostanie
wykonane przez M-Commandera. Przeczytaj sekcję
**Linia powłoki i Linia wejściowa klawiszy**,
żeby nauczyć się więcej na ten temat.

# Obsługa myszy <a id="mouse-support"></a>

M-Commander obsługuje mysz. Moduł ten jest uruchamiany wtedy kiedy
korzystasz z terminala
**xterm**(1)
(działa nawet wtedy, kiedy łączysz się przez telnet albo rlogin z innym komputerem
z terminala xterm) lub jeśli korzystasz z linuksa na konsoli z zainstalowanym
serwerem
**gpm**(1).

Kiedy klikniesz lewym przyciskiem na panel z katalogami, plik zostanie
wybrany jako aktywny; jeśli klikniesz prawym przyciskiem zostanie on
zaznaczony [lub odznaczony - w zależności od jego aktualnego stanu -
działanie podobne do klawisza
**Insert**
\- przyp. tłumacza].

Podwójne kliknięcie w plik spowoduje wykonanie pliku, jeśli jest on wykonywalny,
a jeśli rozszerzenie pliku jest rozpoznawane przez M-Commander'a i dostępny
jest odpowiedni program, jest on uruchamiany.

Możliwe jest również wykonywanie komend przypisanych klawiszom funkcyjnym
przez kliknięcie w nie.

Jeśli kliknięcie odbędzie się w rejonie górnej lini panelu z katalogami, zostanie
on przewinięty jedną stronę wstecz. Podobnie kliknięcie na dolną ramkę przewija
tekst jedną stronę do przodu. Ta opcja klikania w ramki działa również przy
przeglądaniu pomocy i przy drzewie katalogów.

Standardowo czas autopowtórzenia przy klikaniu myszą wynosi 400 milisekund.
Tę wartość można zmienić edytując plik
~/.config/mc6/ini
i zmieniając parametr
*mouse_repeat_rate.*

Jeśli używasz M-Commandera z obsługą myszy, możesz "przeszczepiać"
kawałki tekstów i używać standardowych zastosowań myszki (kopiowanie i
wklejanie) za pomocą klawisza Shift.

<!-- help:break -->

# Klawisze <a id="keys"></a>

Niektóre komendy M-Commandera wywołuje się kombinacją klawiszy
*Control*
(czasem opisywanego jako CTRL lub CTL) lub
*Meta*
(opisywanego ALT lub nawet Compose). W tym manualu (pliku pomocy) będziemy
używać następujących kombinacji:
C-\<klawisz> - znaczy: trzymając klawisz Control naciśnij
\<klawisz>. Więc C-f będzie oznaczać: trzymając Control, naciśnij f.

M-\<klawisz> - znaczy, że trzymając klawisz Meta lub alt naciskamy \<klawisz>.
Jeśli na twojej klawiaturze nie ma ani klawisza Alt ani Meta, naciśnij ESC, puść
go i wtedy naciśnij \<klawisz> [skutek ten sam, acz jednak użycie trochę
mniej przyjemne i bardziej skomlikowane - przyp. tłumacza].

Wszystkie linie wprowadzające M-Commandera używają w przybliżeniu tych
samych przypisań klawiszy co wersja GNU edytora Emacs.

Jest wiele sekcji mówiących o klawiszach. Ta następująca jest najważniejsza.

Sekcja
[Menu plików](#file-menu)
opisuje skróty klawiszowe do komend pojawiających się w menu plików. Ta sekcja
zawiera funkcję klawiszy. Większość z tych komend wywołuje jakąś akcję przede
wszystkim na jednym lub kilku wybranych plikach.

Sekcja
[Panele katalogowe](#directory-panels)
opisuje klawisze, które zaznaczają plik lub pliki jako docelowe do dalszych
działań (akcją jest najczęściej jedna z tych przedstawionych w menu plików).

Sekcja
*Komendy linii poleceń*
wypisuje listę klawiszy, które są używane do wprowadzania lub edytowania
tekstów w wierszu poleceń. Większość z nich kopiuje nazwy, i inne tego typu,
z panelu katalogów do linii poleceń (żeby uniknąć ich przepisywania), lub
pozwala zwiedzić historię komend linii poleceń.

*Klawisze linii wejściowych*
są używane do edytowania linii na wejściu (przy wpisywaniu). Oznacza,
to że stosuje się je zarówno
do linii poleceń jak do okien dialogowych.

## Zmiana przypisań klawiszy <a id="keys_redefine"></a>

To samo można zrobić w samym programie, z menu
**Opcje**.
Okno
[Przypisania klawiszy](#key-bindings)
wypisuje każde działanie wraz z klawiszami, na które ono odpowiada, zmienia
je i zapisuje wynik do
**~/.config/mc6/keymap.ini**,
czyli do tego pliku, którego szuka opcja. Okno
[Nauka klawiszy](#learn-keys)
zajmuje się drugą stroną sprawy: uczy program tych ciągów, które terminal
wysyła dla źle rozpoznawanych klawiszy.
[Podsłuch klawiszy](#key-sniffer)
pokazuje, co przychodzi po naciśnięciu klawisza, wraz z działaniem, do
którego ten klawisz jest przypisany w bieżącej mapie; od tego warto zacząć,
kiedy przypisanie zdaje się nic nie robić.

Przypisania klawiszy można wczytać z zewnętrznego pliku. Na początku program
tworzy mapę z przypisań podanych w kodzie źródłowym. Potem zawsze wczytywane
są dwa pliki,
**{{pkgdatadir}}/keymap.ini**
i
**{{sysconfdir}}/mcommander/keymap.ini**,
kolejno zmieniając wcześniejsze przypisania.
Pakiet trzyma własne mapy w
**{{sysconfdir}}/mcommander**:
**keymap.default.ini**,
**keymap.emacs.ini**
i
**keymap.vim.ini**,
przy czym
**keymap.ini**
jest dowiązaniem do domyślnej.
Opcja
**--nokeymap**
nie czyta żadnego pliku i zostawia przypisania z kodu źródłowego.

Plik użytkownika szukany jest w następującej kolejności (do pierwszego
znalezionego):

```
1) opcja wiersza poleceń -K <mapa>, --keymap=<mapa>
2) zmienna środowiskowa MC_KEYMAP
3) parametr keymap sekcji [Midnight-Commander]
4) plik ~/.config/mc6/keymap.ini
```

Pierwsze trzy przyjmują nazwę albo ścieżkę bezwzględną. Do nazwy, która nie
kończy się na
**.keymap**,
dodawane jest to rozszerzenie, a plik szukany jest w (do pierwszego
znalezionego):

```
1) ~/.config/mc6/
2) {{pkgdatadir}}/
```

Przez to rozszerzenie map z pakietu, których nazwy kończą się na
**.ini**,
nie da się wybrać w ten sposób. Aby użyć jednej z nich, skopiuj ją albo
zrób dowiązanie do
**~/.config/mc6/keymap.ini**,
który czytany jest jako ostatni i nie wymaga żadnej opcji:

```
ln -s {{sysconfdir}}/mcommander/keymap.vim.ini ~/.config/mc6/keymap.ini
```

## Klawisze różne <a id="miscellaneous-keys"></a>

Jest tu kilka klawiszy, które nie kwalifikują się do żadnej z wymienionych
powyżej grup:

**Enter**.
Jeśli jest wpisany jakiś tekst w linii poleceń (na samym dole, pod panelami),
to wpisana komenda jest wykonywana. Jeśli nic nie jest wpisane, i linia wyboru
jest na jakimś katalogu, M-Commander wykonuje komendę
**chdir**(2)
(zmiana katalogu) do wybranego katalogu i odświeża zawartość panelu; jeśli
linia wyboru jest na pliku wykonywalnym jest on wykonywany. I wreszcie jeśli
rozszerzenie pliku zgadza się z obługiwanym przez programy zewnętrzne, które
są obsługiwane prze M-Commandera, są one wywoływane z owym programem.

**C-l**.
Od nowa rysuje wszystkie informacje okna M-Commandera.

**C-x c**.
Uruchamia komendę Chmod dla aktualnego pliku lub zaznaczonych plików.

**C-x o**.
Uruchamia komendę Chown dla aktualnego pliku lub zaznaczonych plików.

**C-x l**.
Uruchamia komendę dowiązywania.

**C-x s**.
Uruchamia komendę miękkiego dowiązywania.

**C-x i**.
Zmienia aktywny panel.

**C-x q**.
Przełacza nieaktywny panel w tryb "quick view".

**C-x !**.
Wykonuje komendę z zewnętrznego panelu.

**C-x h**.
Uruchamia komendę dodawania katalogów do hotlisty.

**M-!**.
Uruchamia komendę filtrowanego podglądu, opisanego w sekcji
*Podgląd.*

**M-?**.
Uruchamia komendę szukania pliku.

**M-c**.
Włącza okno dialogowe quick cd (szybkiej zmiany katalogów)

**C-o**.
Jeśli program jest uruchamiany na konsoli typu Linux lub FreeBSD lub też
na konsoli xterm, pokaże wyjście ostatnio wykonywanego programu. Jeśli
uruchomiono M-Commandera na konsoli type Linux, M-Commander używa
zewnętrznego programu (cons.saver) w celu zachowywyania i odzyskiwania
informacji na ekranie komputera.

Jeśli użycie trybu powłoki w tle jest wkompilowane, możesz nacisnąć
C-o w dowolnej chwili i zostataniesz przeniesiony z powrotem bezpośrednio
do głównego okna M-Commandera, żeby powrócić do wykonywania aplikacji
po prostu naciśnij znów C-o. Jeśli masz zawieszoną aplikację właśnie przez
użycie tego triku, nie będziesz mógł "odpalać" innych programów spod
M-Commandera dopóki nie zamkniesz zawieszonego programu.

Aby dowiedzieć się czegoś na temat polskiech liter w M-Commanderze
przeczytaj sekcję
*Polskie litery.*

## Panel Katalogów <a id="directory-panels"></a>

Sekcja opisuje klawisze, które operują na panelu katalogów. Jeśli chcesz
wiedzieć jak zmienić panele zobacz sekcję
*Lewe i prawe menu.*

**Tab**, **C-i**.
Zmienia aktywny panel. Stary panel staje się w tym momencie aktywnym panelem,
a aktywny staje się starym. Linia wyboru zmienia swoje położenia do aktywnego
panelu.

**Insert**, **C-t**.
DEPRECATED! Do zaznaczania plików możesz używać klawisza Insert lub C-t. Żeby odznaczyć plik
po prostu zaznacz jakiś już zaznaczony.

**Insert**
: to tag files you may use the Insert key (the kich1 terminfo sequence).
To untag files, just retag a tagged file.

**Shift-Up, Shift-Down**
: zaznacza plik pod paskiem wyboru albo odznacza zaznaczony i przesuwa pasek
na następny plik w górę lub w dół.

**Shift-PgUp, Shift-PgDn, Shift-Home, Shift-End**
: przesuwają pasek wyboru tak jak klawisz bez Shift i zaznaczają pliki,
przez które przechodzi. Jeśli plik, od którego zaczyna się seria tych
klawiszy, jest zaznaczony, odznaczają je.

**Shift-Left, Shift-Right**
: robią to samo dla kolumny plików w górę lub w dół.

Dopóki w wierszu poleceń jest tekst, Shift-Left, Shift-Right, Shift-Home i
Shift-End należą do niego i nie zaznaczają plików.

**M-e**
: to change charset of panel you may use M-e (Alt-e).
Recoding is made from selected codepage into system codepage. To
cancel the recoding you may select "directory up" (..) in active panel.
To cancel the charsets in all directories, select "No translation " in
the dialog of encodings.

**M-g**, **M-r**, **M-j**.
Używane do wybierania najwyższego, środkowego i najniższego pliku w panelu.

**M-t**.
Przełącza tryb wyświetlania do następnego możliwego. Używając tej opcji
łatwo jest przejść szybko z długiego do krótkiego trybu wyświetlania
jak również do tego zdefiniowanego przez użytkownika.

`C-\`
(control-backslash).
Pokazuje hotlistę katalogów i zmienia katalog do wybranego przez użytkownika.

**+**
(plus).
Używane do zaznaczania grupy plików. M-Commander zapyta o
wyrażenie opisującą grupę. Jeśli opcja
*Shell Patterns*
jest włączona, typ wyrażeń jest bardzo podobny do tego w powłoce
(\* dla zera i więcej znaków i ? dla jednego znaku). Jeśli zaś opcja
*Shell Patterns*
jest wyłączona, sposób zaznaczania plików jest zgodny z ustawieniami
(zobacz
**ed**(1)).

`\`
(backslash).
Używaj znaków "\\" do odznaczania grupy plików. Jest to przeciwieństwo klawisza
plus.

**strzałka do góry**, **C-p**.
Przenosi linię wyboru do poprzedniej pozycji w panelu.

**strzałka do dołu**, **C-n**.
Przenosi linię wyboru do następnej pozycji w panelu.

**home**, **a1**, **M-<**.
Przenosi linię wyboru do pierwszej pozycji w panelu.

**end**, **c1**, **M->**.
Przenosi linię wyboru do ostatniej pozycji w panelu.

**PageDown**, **C-v**.
Przenosi linię wyboru jedną stronę do dołu.

**PageUp**, **M-v**.
Przenosi linię wyboru jedną stronę do góry.

**M-o**.
Jeśli drugi panel jest zwykłym panelem wyświetlającym i w aktywnym panelu
stoisz na katalogu, drugi panel będzie
pokazywać zawartość
akutalnego katalogu (tak jak w Emacsie kombinacja C-o). Jeśli nie stoisz
na katalogu zawartością drugiego katalogu stanie się katalog o jedno piętro
wyższy od aktualnego.

**C-PageUp**, **C-PageDown**.
Działa tylko na konsoli typu Linux: wykonuje przejście do katalogu ".." lub
do aktualnie wybranego, w zależności od kombinacji.

**M-y**.
Przenosi do poprzedniego katalogu w historii, podobne do kliknięcia myszką.
'<'.

**M-u**.
Przechodzi do następnego katalogu w historii, podobne do kliknięcie myszką
w '>'.

**M-S-h**, **M-H**.
Wyświetla historię katalogów, podobne działanie do kliknięcia myszką 'v'.

## Szybkie wyszukiwanie i szybki filtr <a id="quick-search"></a>

Tryb szybkiego wyszukiwania pozwala szybko znaleźć plik w panelu.
**C-s**
albo
**Alt-s**
zaczyna szukanie nazwy w liście katalogu.
**Alt-Shift-s**
włącza szybki filtr, który używa tego samego wzorca, ale ukrywa pozycje,
które go nie zawierają. Pozycja katalogu nadrzędnego widoczna jest zawsze.

Kiedy jeden z tych trybów jest włączony, naciskane klawisze dopisują się do
wspólnego wzorca, a nie do wiersza poleceń. Jeśli opcja
*Pokaż mini-status*
jest włączona, wzorzec widać w wierszu mini-statusu. W trakcie pisania linia
wyboru przechodzi do następnego pliku, którego nazwa zaczyna się od wpisanych
liter; w trybie filtra lista dodatkowo zawęża się do pasujących pozycji.
Klawisze
**Backspace**
albo
**Del**
służą do poprawiania błędów.

Naciśnięcie
**C-s**
albo
**Alt-s**
przy włączonym szybkim filtrze przełącza na szybkie wyszukiwanie i pokazuje
wszystkie pozycje, nie gubiąc ani wzorca, ani bieżącego pliku. Naciśnięcie
**Alt-Shift-s**
przy włączonym szybkim wyszukiwaniu wraca do filtra. Powtórzenie skrótu
włączonego trybu przechodzi do następnego trafienia.

Klawisze ruchu, strzałki,
**Home**,
**End**,
**PageUp**
i
**PageDown**,
poruszają się wewnątrz przefiltrowanej listy, nie zamykając filtra.

Pliki można zaznaczać i odznaczać przy włączonym filtrze. Zaznaczenia
zostają po zmianie trybu i po zamknięciu filtra.

Jeśli któryś z trybów włączysz podwójnym naciśnięciem jego skrótu, wróci
poprzedni wzorzec.

Poza znakami nazwy pliku można używać także znaków wieloznacznych '\*' i '?'.

## Linia Powłoki <a id="shell-command-line"></a>

Ta sekcja opisuje klawisze, które są użyteczne do efektywniejszego
wpisywania podczas podawania komend powłoki.

**M-Enter**.
Kopiuje nazwę aktualniego wybranego pliku do linii poleceń.

**C-Enter**.
To samo co M-Enter, działa tylko na konsoli typu Linux.

**M-Tab**.
Wykonuje dokończenie nazw plików, komend, zmiennych, użytkowników, nazw hostów
za Ciebie.

**C-x t**, **C-x C-t**.
Kopiuje nazwy zaznaczonych plików (lub jeśli nie ma zaznaczonych - aktywnego)
w aktywnym (C-x t) lub nieaktywnym panelu (C-x C-t) do linii poleceń.

**C-x p**, **C-x C-p**.
Pierwsza kombinacja kopiuje pełną ścieżkę z aktywnego, a druga z nieaktywnego
panelu.

**C-q**.
Komenda 'quote' (cytuj) może być używana do wpisywania do wiersza poleceń znaków, które
normalnie przechwytywane są przez Commandera (tak jak znak '+').

**M-p**, **M-n**.
Używaj tych klawiszy, żeby przeglądać historię komend. M-p wyświetla poprzednią,
a M-n następną komendę.

**M-h**.
Wyświetla historię aktualnej linii poleceń.

## Podstawowe klawisze ruchu <a id="general-movement-keys"></a>

Przeglądarka pomocy, podgląd plików i drzewo katalogów używają podobnych
klawiszy do przemieszczania. Przez to akceptują dokładnie te same klawisze.
Każde z nich z resztą traktują je jako swoje własne.

Niektóre partie M-Commandera również używają tych klawiszy,
więc niniejsza sekcja może być użyteczna również dla tych partii.

**strzałka w górę**, **C-p**.
Przechodzi jedną linię wstecz.

**strzałka w dół**, **C-n**.
Przechodzi jedną linię naprzód.

**Page Up**, **M-v**.
Przechodzi jedną stronę wstecz.

**Next Page**, **Page Down**, **C-v**.
Przechodzi jedną stronę naprzód.

**Home**, **A1**.
Przechodzi do początku.

**End**, **C1**.
Przechodzi na koniec.

Przeglądarka pomocy i podgląd plików akceptują następujące klawisze
(poza tymi opisanymi powyżej).

**b**, **C-b**, **C-h**, **Backspace**, **Delete**.
Przechodzi jedną stronę wstecz.

**klawisz spacji**.
Przechodzi jedną stronę naprzód.

**u**, **d**.
Przechodzi pół strony naprzód lub wstecz.

**g**, **G**.
Przechodzi do początku lub do końca.

## Linia wejściowa klawiszy <a id="input-line-keys"></a>

Linie wejściowe (te używane w
[wierszu poleceń](#shell-command-line)
i w oknach dialogowych) przyjmują następujące klawisze:

**C-a**
: umieszcza kursor na początku wiersza.

**C-e**
: umieszcza kursor na końcu wiersza.

**C-b, Left**
: przenosi kursor o jedną pozycję w lewo.

**C-f, Right**
: przenosi kursor o jedną pozycję w prawo.

**M-f**
: przesuwa kursor o jedno słowo naprzód.

**M-b**
: przesuwa kursor o jedno słowo wstecz.

**C-h, Backspace**
: kasuje poprzedni znak.

**C-d, Delete**
: kasuje znak w miejscu kursora.

**C-@**
: wstawia zaznaczenie do wycinania.

**C-w**
: kopiuje tekst między kursorem a zaznaczeniem do bufora i usuwa go z wiersza
wprowadzania.

**M-w**
: to samo co C-w, tylko bez usuwania tekstu z wiersza.

**C-y**
: wstawia z powrotem zawartość bufora.

**C-k**
: wycina tekst od kursora do końca wiersza.

**Ctrl-Insert**
: kopiuje zaznaczony tekst do pliku wymiany i do schowka systemu. Bez
zaznaczenia: zaznaczone pliki panelu widocznego na ekranie, po jednym w
wierszu; inaczej cały wiersz; inaczej plik pod kursorem panelu.

**Shift-Delete**
: wycina zaznaczony tekst do pliku wymiany i do schowka systemu.

**Shift-Insert**
: wkleja plik wymiany do wiersza jako jeden wiersz: końce wierszy i pozostałe
znaki sterujące zamieniają się w spacje. W wierszu poleceń działa i przy
widocznych, i przy schowanych panelach. Więcej niż 2 KB tekstu wkleja się
dopiero po potwierdzeniu.

**M-p, M-n**
: używaj tych klawiszy, żeby przeglądać historię poleceń. M-p pokazuje
poprzednie, a M-n następne.

**M-C-h, M-Backspace**
: kasuje jedno słowo wstecz.

**M-Tab**
: wykonuje
[dokończenie](#completion)
nazw plików, poleceń, zmiennych, użytkowników i nazw hostów.

<!-- help:break -->

# Linia menu <a id="menu-bar"></a>

Linia menu uaktywnia się kiedy wciskasz klawisz F9 lub kiedy klikasz myszką
na najwyższy wiersz ekranu. Linia menu ma sześć podmenu: "left", "file",
"attributes", command", "options" i "right" (po polsku to jest "lewe", "plik",
"atrybuty", "komendy", "opcje", "prawe").

Lewe i prawe menu pozwalają ci na modyfikacje wyglądu lewego i prawego panelu
katalogowego.

Menu plik pozwala na wykonanie akcji na aktualnym lub zaznaczonych plikach.

Menu atrybutów zmienia prawa dostępu, właściciela i atrybuty systemu plików
tych samych plików.

Menu komend mieści w sobie możliwe do wykonania akcje, które są dużo bardziej
globalne i nie mają związku z aktualnym i zaznaczonymi plikami.

## Lewe i prawe menu <a id="left-and-right-menus"></a>

Wygląd panelu katalogowego może zostać zmieniony poprzez menu
**left**
i
**right**.

### Tryby wyświetlania (Listing modes) <a id="listing-format"></a>

Tryby wyświetlania są używane do zmienia ustawień przy wyświetlaniu.
Dostępne są cztery różne tryby:
**Full**,
**Brief**,
**Long**
i
**User**.
Tryb "Full" pokazuje nazwę, rozmiar i czas modyfikacji pliku.

Tryb "Brief" pokazuje tylko nazwę pliku i ma dwie kolumny (dzięki temu
może pokazywać nawet dwa razy więcej niż inne tryby). Tryb "Long" jest
podobny do wyniku polecenia
**ls -l**.
Zabiera on szerokość całego ekranu.

Jeśli wybierzesz tryb "user" (użytkownika), będziesz mógł wybrać własny
sposób wyświetlania.

Tryb użytkownika musi zaczynać się od określenia wielkości panelu. Może
to być "half" (pół) lub "full" (cały) i określa, czy ma być widoczny
jeden duży panel na cały ekran czy dwa mniejsze.

Po rozmiarze panelu możesz włączyć tryb dwóch kolumn panelu. Robi się
to dodając liczbę "2" do tekstu formatu.

Po tym wpisujesz już nazwy pól z podaniem opcjonalnej wielkości.
Wszystkie możliwe pola jakich możesz użyć to:

**name**
: wyświetla nazwę pliku.

**size**
: wyświetla wielkość pliku.

**bsize**
: jest alternatywą dla format
**size**.
Wyświetla rozmiar plików, a dla katalogów po prostu wyświetla tekst
"SUB-DIR" lub "UP--DIR".

**type**
: wyświetla jednoznakowy opis typu pliku. Ten znak jest taki sam co ten
wyświetlany prze komendę ls z flagą -F. Wyświetlana jest gwiazdka
dla plików wykonywalnych,
ukośnik dla katalogów, małpa (@) dla dowiązań, znak równości dla gniazd,
minus dla urządzeń niestniejących, znak plus dla urządzeń istniejących,
pionową kreskę (|) dla kolejek FIFO, tyldę dla dowiązań
symbolicznych, i wykrzyknik dla dowiązań wskazujących na nieistniejący plik.

**mark**
: Gwiazdka jeśli plik jest zaznaczony, spacja jeśli nie jest.

**mtime**
: czas ostatniej modyfikacji pliku.

**atime**
: czas ostatniego dostępu do pliku.

**ctime**
: czas utworzenia pliku.

**perm**
: tekst reprezentujący aktualne uprawnienia do pliku.

**mode**
: wartość (cyfrowa) przedstawiająca prawa do pliku.

**nlink**
: liczba dowiązań do pliku.
**ngid**
GID (numeryczny).

**nuid**
: UID (numeryczny).

**owner**
: właściciel pliku.

**group**
: grupa pliku.

**inode**
: numer i-węzła pliku.

Możesz również używać następujących znaków dla zmiany wyświetlania:

**space**
: spacja w formacie wyświetlania.

**|**
: Ten znak jest używany w celu dodania pionowej linii od formatu wyświetlania.

Żeby wymusić szerokość pola, po prostu dodaj ':' a potem ilość znaków jakie
chcesz żeby miało pole. Jeśli numer zaczyna się od '+', to szerokość nie może
być mniejsza od podanej, jeśli program zobaczy, że jest jeszcze trochę
miejsca na ekranie, rozszerzy to pole.

Na przykład tryb
**Full**
wyświetla w formacie:

half type name | size | mtime

A format
**Long**
wyświetla w formacie:

full perm space nlink space owner space group space size space mtime
space name

A to jest całkiem ładny tryb użytkownika:

half name | size:7 | type mode:3

Panele mogą być również przestawione do następujących trybów:

**Info**
: Tryb info wyświetla informację o aktualnie zaznaczonym pliku i (jeśli
to możliwe) o systemie plików.

**Tree (drzewo)**
: Widok drzewa jest całkiem podobny do widoku
[Drzewa katalogów](#directory-tree).
Zobacz tę sekcję jeśli chcesz się dowiedzieć czegoś na ten temat.

**Quick View**
: W tym trybie, panele zostaną przełączone w tryb zredukowanego podglądu
wyświetlającego zawartość aktualnego pliku. Jeśli zaznaczysz panel
(klawiszem tab lub myszką), będziesz miał dostęp do większości komend
podglądu.

### Porządek sortowania (Sort order...) <a id="sort-order"></a>

Istnieje osiem porządków sortowania. Przez: nazwę, rozszerzenie,
datę modyfikacje, datę odczytu, datę zmiany, rozmiar,
numeru i-węzła i niesortowane. Porządek sortowanie możesz wybrać w oknie
dialogowym porządku sortowania. Możliwe jest również wybranie porządku
wstecznego (od tyłu).

Standardowo, katalogi są sortowane przed plikami, ale może to być zmienione
przez opcję
**Mix all files (mieszaj wszystkie pliki)**.

### Filtry (Filter...) <a id="filter"></a>

Polecenie filtra pozwala podać wzorzec (na przykład
**\*.tar.gz**),
do którego pliki i katalogi muszą pasować, aby były pokazane.
[Wiersz wprowadzania](#input-line-keys)
przyjmuje wzorzec nazw pokazywanych w panelu.

Przy włączonym polu
*Tylko pliki*
filtr dotyczy tylko plików, a wszystkie katalogi są widoczne. Inaczej
filtrowane są zarówno pliki, jak i katalogi. Przy włączonym polu
*Wzorce powłoki*
wzorzec działa jak rozwijanie nazw w powłoce (\* oznacza zero lub więcej
znaków, a ? jeden). Inaczej porównanie idzie zwykłymi wyrażeniami regularnymi
(zobacz ed(1)). Przy włączonym polu
*Rozróżniaj wielkość liter*
filtr odróżnia duże i małe litery, inaczej ich nie rozróżnia.

### Odśwież (Reread) <a id="reread"></a>

Komenda odśwież odświeża widok wszystkich plików w katalogów. Jest to użyteczne
jeśli inny proces stworzył lub usunął jakiś pliki. Jeśli użyłeś panelu
zewnętrznego, wszystkie informacje zostaną przywrócone do prawdziwego stanu.

## Menu plików (File menu) <a id="file-menu"></a>

M-Commander używa klawiszy F1 - F10 jako skrótów klawiszowych do komend
występujących w menu plików. Na terminalach bez funkcji klawiszowych (F1 - F10)
można używać kombinacji klawisza Escape i numeru ( odpowiednio 1 dla F1,
2 dla F2 itd. )

Menu plików ma następujące komendy (skróty klawiszowe umieszczone są
na dole ekranu):

**Pomoc (F1)**

Wywołuje wbudowaną przeglądarkę plików pomocy. Wewnątrz niej można używać
klawisza Tab żeby przejść do następnego dowiązania, Enter
żeby przejść do wybranego dowiązania. Klawisze Spacji i Backspace są używane
do poruszania się naprzód i wstecz na stronach pomocy. Naciśnij klawisz
F1 żeby uzyskać pełną listę dostępnych klawiszy w pomocy.

**Menu (F2)**

Wywołuje menu użytkownika. Menu użytkownika jest łatwym w użyciu narzędziem
służącym do obsługi zewnętrznych programów i dodatkowych opcji
M-Commandera.

**Podgląd (F3, Shift-F3)**

Włącza podgląd aktualnie wybranego pliku. Standardowowo wywoływany jest
wbudowany podgląd plików, ale jeśli opcja "Use internal view" jest wyłączona,
wywoływany jest zewnętrzny program do poglądu, wskazywany przez zmienną
**PAGER**.
Jeśli jednak zmienna
**PAGER**
nie została jeszcze zdefiniowana, wywoływana jest komenda "view". Jeśli użyjesz
kombinacji klawiszy
**Shift-F3**,
pogląd zostanie wywołany bez jakiegokolwiek
formatownia pliku.

**Filtrowany podgląd (M-!)**

Ta kombinacja klawiszy oczekuje na komendę i jej argument (argumentem standardowo
jest wybrany aktualnie plik), całe wyjście programu przekierowywane jest do pliku,
który zostaje automatycznie wyświetlony na ekranie w trybie podglądu.

**Edycja (F4, F14)**

F4 edytuje plik pod kursorem, a F14 uruchamia edytor z nowym, pustym plikiem.
Wywoływany jest edytor
**vi**(1),
edytor podany w zmiennej środowiskowej
**EDITOR**
albo
[wbudowany edytor plików](mcedit6.md#internal-file-editor),
jeśli opcja use_internal_edit jest włączona.

O tym, jak podać dodatkowe opcje wiersza poleceń zewnętrznych edytorów, mówi
rozdział
[parametry zewnętrznego edytora](#parameters-for-external-editor-or-viewer).

**Kopiuj (F5, F15)**

Włącza okno dialogowe, w którym standardowo znajduje się ścieżka do
katalogu w
nieaktywnym panelu, po czym kopiuje aktualny plik (lub wybrane
jeśli wybrano jakiekolwiek) do katalogu, który wybraliśmy w oknie dialogowym.
Miejsce na plik docelowy może być z góry zajęte, zależnie od opcji
konfiguracji preallocate_space.
Podczas procesu kopiowania możesz go w każdej chwili przerwać wciskając C-c lub
Esc. Żeby dowiedzieć się czegoś więcej na temat jokerów w ścieżce źródłowej
(którymi najczęściej będą \* lub ^\\(.\*\\)$) i innych możliwych określeń w
katalogu docelowym zobacz rozdział
[Maski kopiowania/przenoszenia](#mask-copyrename).

F15 działa podobnie, ale domyślnie podpowiada katalog panelu aktywnego, i
zawsze dotyczy pliku pod kursorem, niezależnie od zaznaczonych plików.

Na niektórych systemach możliwe jest kopiowanie w tle, robi się to klikając
na przycisk backgorund (lub naciskając kombinację M-b w oknie dialogowym).
Background Jobs jest używane do kontrolowania prac w tle.

**Link (C-x l)**

Tworzy sztywne dowiązanie do aktualnego pliku.

**Dowiązanie bezwzględne (C-x s)**

Tworzy bezwzględne dowiązanie symboliczne do aktualnego pliku.

**Dowiązanie względne (C-x v)**

Tworzy względne dowiązanie symboliczne do aktualnego pliku.

Dla tych, którzy nie wiedzą
co to jest dowiązanie: tworzenie dowiązania do pliku jest tak jak kopiowanie
pliku, z tym tylko,
że zarówno plik źródłowy i docelowy reprezentują ten sam plik. Na przykład,
jeśli edytujesz jeden z tych plików, zmiany, które czynisz pojawiają się w obu
plikach. Niektórzy mówią na dowiązania aliasy lub skróty.

Twarde dowiązanie wydaje się być prawdziwym plikiem. Po stworzeniu go
nie ma możliwości
rozróżnienia, który z plików jest oryginalny, a który jest dowiązaniem.
Jest bardzo
ciężko zauważyć, że wskazują one na ten sam plik.
Używaj dowiązań twardych wtedy kiedy nie chcesz tego wiedzieć.

Dowiązanie symboliczne jest tylko odwołaniem do oryginalnego pliku.
Jeśli ten plik
zostanie wyrzucony, dowiązanie stanie się bezużyteczne. Jest całkiem łatwo
zauważyć,
że pliki odnoszą się w gruncie rzeczy do tego samego. M-Commander
pokazuje znak "@" przed nazwą pliku jeśli jest dowiązaniem
symbolicznym do innych
(poza katalogami, przed którymi pokazuje tyldę (~)). Oryginalny plik wskazywany
przez dowiązanie jest pokazywany w linii mini-statusu, jeśli opcja
*Show mini-status*
jest włączona. Używaj dowiązań symbolicznych, jeśli chcesz unikąć problemów z
rozpoznawaniem twardych dowiązań.

**Zmiana nazwy/przeniesienie (F6, F16)**

Włącza okno dialogowe, gdzie standardowo wpisana jest nazwa katalogu w
nieaktywnym panelu, i przenosi aktualnie wybrany plik (lub zaznaczone jeśli
choć jeden jest zaznaczony) do katalogu wpisanego w oknie dialogowym. Podczas
procesu przenoszenia możesz użyć kombinacji klawiszy C-c lub ESC, żeby przerwać
operację. Po więcej szczegółów zobacz operację
**Kopiuj**
opisaną powyżej. Większość rzeczy jest całkiem podobna.

Na niektórych systemach możliwe jest przenoszenie w tle, robi się to klikając
na przycisk background (lub naciskając kombinację M-b w oknie dialogowym).
Background Jobs jest używane do kontrolowania prac w tle.

**Utwórz katalog (F7)**

Włącza menu dialogowe i zakłada katalog o podanej nazwie

**Kasuj (F8)**

Kasuje aktualnie wybrany lub zaznaczone pliki w aktywnym panelu. Podczas
procesu możesz nacisnąć C-C lub Esc żeby przerwać operację. [skasowane pliki
nie będą jednak odzyskane - przyp. tłumacza].

**Szybka zmiana katalogu (Alt-c)**

Otwiera okno
[szybkiej zmiany katalogów](#quick-cd).

**Zaznacz grupę (+)**

Używane do zaznaczania grupy plików. M-Commander będzie żądał tekstu
opisującego grupę plików. Jeśli opcja
*Shell Patterns*
jest włączona, tekst będzie traktowany jako globalny dla interpretatora (\*
oznacza zero lub więcej znaków a ? oznacza jeden znak). Jeśli opcja
*Shell Patterns*
jest wyłączona, wtedy zaznaczanie plików jest robione z zastosowaniem norm
zewnętrznych (zobacz ed (1)).

**Odznacz grupę (\\)**

Używane do odznaczania grupy plików. Jest przeciwieństwem komendy
*Zaznacz pliki.*

**Odwróć zaznaczenie (\*)**

Odwraca zaznaczenie: zaznaczone pliki zostają odznaczone, a pozostałe
zaznaczone. Czy dotyczy to także katalogów, zależy od opcji
*Odwracaj tylko pliki*
w
[opcjach paneli](#panel-options).

**Wyjdź (F10, Shift-F10)**

Zamyka M-Commandera. Shift-F10 jest używany jeśli używasz
"wrappera" powłoki. Shift-F10 nie przeniesie cię do katalogu, w którym
byłeś ostatnio w M-Commanderze, zamiast tego przejdzie do katalogu,
z którego uruchomiłeś program.

### Szybka zmiana katalogów (Quick cd) M-c <a id="quick-cd"></a>

To polecenie przydaje się, kiedy masz już pełny wiersz poleceń, a chcesz
[zmienić katalog](#the-cd-internal-command),
nie wycinając i nie wklejając tego, co w nim stoi. Otwiera małe okno, w
którym podajesz to, co podałbyś po poleceniu
**cd**
w wierszu poleceń, i naciskasz Enter. Działa w nim wszystko to, co daje
[wewnętrzne polecenie cd](#the-cd-internal-command).

## Menu atrybutów (Attributes Menu) <a id="attributes-menu"></a>

Komendy tego menu zmieniają nie zawartość pliku, a to, co wie o nim system
plików: prawa dostępu, właściciela i grupę oraz atrybuty systemu plików. Każda
z nich działa na aktualnym pliku albo na zaznaczonych plikach, jeśli takie są.

**Chmod... (C-x c)**
: Zmiana praw dostępu w oknie
[Chmod](#chmod).

**Chown... (C-x o)**
: Zmiana właściciela i grupy w oknie
[Chown](#chown).

**Zaawansowane chown...**
: Zmiana praw dostępu, właściciela i grupy w jednym oknie, zobacz
[Zaawansowane chown (Advanced Chown)](#advanced-chown).

**Flagi chattr... (C-x e)**
: Zmiana atrybutów systemu plików ext2, ext3 albo ext4 w oknie
[Atrybuty pliku](#chattr). Pozycja jest tylko wtedy, gdy program zbudowano z
obsługą tych atrybutów.

## Menu komend (Command Menu) <a id="command-menu"></a>

Komenda drzewo katalogów (Directory tree) pokazuje drzewo katalogów.

Komenda "Find file" szuka pliku spełniającego podane warunki, natomiast komenda
"Swap panels" zamienia zawartości obu paneli.

Komenda "Panels on/off" pokazuje wyjście ostatniej komendy interpetatora
poleceń. Działa ona tylko na terminalach typu Linux lub FreeBSD.

Komenda porównywania katalogów (Compare directories) (C-x d) porównuje
zawartości panelu katalogowego z drugim. Możesz potem użyc Kopiuj (F5)
żeby stworzyć dwa dokładnie identyczne panele. Metoda "quick" porównuje
tylko i wyłącznie rozmiary plików i ich daty. Metoda "thorough" porównuje
pliki bajt po bajcie. Metoda "size-only" zwraca uwagę tylko na rozmiar plików.
Nie ma dla niej żadnego znaczenia czy plik ma inną datę lub zawartość, liczy
się tylko rozmiar.

Komenda historii komend (Command history) pokazuje listę wpisanych komend.
Ta, którą wybierzesz, jest kopiowana do linii poleceń. Do historii komend
można mieć dostęp również przy użyciu kombinacji M-p lub M-n.

Komenda hotlisty katalogów (Directory hotlist) (C-\\) pozwala na zmienianie
katalogów do tych najczęściej używanych dużo szybciej.

Komenda panelu zewnętrznego (External panelize) pozwala na wykonywania programów
zewnętrznych i ustawienia zawartości paneli na to co zwróciła wywołana
komenda.

### Drzewo katalogów (Directory Tree) <a id="directory-tree"></a>

Możesz wybierać katalogi z drzewa katalogów i M-Commander przejdzie do
wybranego przez Ciebie katalogu.

Są dwa sposoby wywoływania drzewa. Prawdziwa komenda drzewa katalogów jest
dostępna z menu komend. Inną metodą jest wybranie drzewa z menu "lewego" bądź
"prawego".

Żeby nie mieć zbyt dużych opóźnień M-Commander skanuje tylko małą
ilość katalogów (tę potrzebną w danej chwili). Jeśli jakiegoś katalogu nie
widać przejdź do jego katalogu nadrzędnego i naciśnij C-r (lub F2).

Możesz używać następujących klawiszy:

Generalne klawisze ruchu są akceptowane.

**Enter**.
W drzewie katalogów, wychodzi z trybu drzewa i przechodzi znów do trybu
zwykłego panelu. W podglądzie drzewa zmienia katalog w drugim panelu i zostaje
w trybie podglądu drzewa w panelu aktywnym.

**C-r**, **F2** (Rescan).
Odświeża aktualny katalog. Używane jeśli drzewo nie jest już aktualne. Nie
pokazuje katalogów, które już istnieją lub pokazuje te, których już nie ma.

**F3** (Forget).
Usuwa aktualny katalog z drzewa katalogów. Używaj tego jeśli chcesz usunąć
"śmiecące" i niepotrzebne katalogi z wyświetlania. Żeby były one znów
widoczne wystarczy nacisnąć F2.

**F4** (Static/Dynamic).
Przełącza pomiędzy dynamicznym (standardowo) i statycznym trybem nawigacji.

W trybie statycznym możesz używać strzałek do dołu i do góry do wybierania
katalogu. Wszystkie zwiedzone katalogi są widoczne.

W trybie dynamicznym możesz używać strzałek w celu wybrania równorzędnego
katalogu, strzałki w lewo żeby dostać się do katalogu domowego, strzałki
w prawo w celu dostania się do katalogu podrzędnego. Widoczne jest tylko
najbardziej aktualne drzewo katalogów. Drzewo zmienia się więc dynamicznie
podczas twojego przemieszczania.

**F5**
(Copy).
Kopiuje katalog.

**F6**
(RenMov).
Przenosi katalog.

**F7**
(Mkdir).
Tworzy nowy katalog poniżej aktualnego.

**F8**
(Delete).
Kasuje katalog z systemu plików.

**C-s**, **M-s**.
Szuka natępnego katalogu spełniającego podane warunki szukania. Jeśli taki
nie istnieje te klawisze spowodują przemieszczenie się o jedną linię w dół.

**C-h**, **Backspace**.
Kasuje ostatni znak w ciągu znaków do poszukiwania.

**Jakikolwiek inny klawisz**.
Dodaje klawisz do ciągu znaków do szukania i przenosi do najbliższego
katalogu, którego nazwa zaczyna się od tych znaków. W podglądzie drzewa musisz
najpierw uaktywnić szukanie naciskając C-s. Ciąg szukający jest pokazywany
w linii mini-statusu.

Następujące klawisze są dostępne tylko w drzewie katalogów. Nie działają one
w poglądzie katalogów.

**F1**
(Help).
Wywołuje podgląd pomocy i pokazuje tę sekcję.

**Esc**, **F10**.
Wychodzi z drzewa. Nie zmienia katalogów.

Mysz jest obsługiwana. Podwójne kliknięcie ma znaczenie identyczne do
klawisza Enter. Zobacz również sekcję
*Obsługa myszy.*

### Znajdź plik (Find File) <a id="find-file"></a>

Polecenie Znajdź plik najpierw pyta o katalog, od którego zacząć szukanie, a
potem o nazwę szukanego pliku. Przyciskiem Drzewo można wybrać katalog
początkowy z
[drzewa katalogów](#directory-tree).

Pole "Nazwa pliku" zawiera wzorzec szukanej nazwy. Program rozumie go jako
wzorzec powłoki albo jako wyrażenie regularne, zależnie od stanu pola "Wzorce
powłoki". Pusta wartość też jest poprawna i pasuje do każdej nazwy.

Pole "Zawartość" zawiera tekst szukany wewnątrz plików. Puste pole oznacza,
że program nie szuka w zawartości.

Opcja "Całe słowa" zawęża szukanie do plików, w których znaleziona część
tworzy całe słowo, tak jak robi to grep -w.

Szukanie zaczyna przycisk Ok. W trakcie można je zatrzymać przyciskiem Stop i
wznowić przyciskiem Kontynuuj.

Lista pokazuje przy każdym znalezionym pliku czas zmiany, rozmiar i prawa
obok nazwy. Przy szukaniu w zawartości plik pojawia się raz: pojedyncze
trafienie widać obok nazwy jako "plik.c:12", a plik z więcej niż jednym
trafieniem podaje ich liczbę i jest oznaczony "[+]". Trafienia takiego pliku
rozwija klawisz Left albo kliknięcie w znacznik: numer wiersza i sam wiersz.
Tam Enter przechodzi do pliku, F3 go pokazuje, a F4 edytuje na wybranym
trafieniu.

Po liście można chodzić strzałkami. Przycisk Zmień katalog przechodzi do
katalogu wybranego pliku. Przycisk Jeszcze raz pyta o parametry nowego
szukania. Przycisk Wyjdź kończy szukanie. Przycisk Panelizuj wstawia
znalezione pliki do bieżącego panelu, dzięki czemu można na nich wykonywać
dalsze operacje (podgląd, kopiowanie, przenoszenie, kasowanie i tak dalej).
Aby wrócić do zwykłej listy, przejdź do katalogu ".."; aby znów zobaczyć
wynik, wybierz tryb Panelizuj w menu lewego albo prawego panelu.

Pole "Pomijaj katalogi" i pole pod nim podają listę katalogów, które szukanie
ma pominąć (na przykład CD-ROM albo katalog NFS zamontowany po wolnym łączu).
Elementy listy rozdziela się dwukropkiem:

```
/cdrom:/nfs/wuarchive:/afs
```

Ścieżki względne też są dozwolone. Poniższy przykład pomija dodatkowo
katalogi systemów kontroli wersji:

```
/cdrom:/nfs/wuarchive:/afs:.svn:.git:CVS
```

Uwaga: pole może zawierać kropkę (.), która oznacza bieżącą ścieżkę
bezwzględną.

Do niektórych zadań warto użyć polecenia
[Panel zewnętrzny](#external-panelize).
Znajdź plik służy do prostych zapytań, a Panelem zewnętrznym można zrobić
dowolnie złożone szukanie.

### Panel zewnętrzny <a id="external-panelize"></a>

Panel zewnętrzny pozwala ci na wykonywanie zewnętrznych programów i
oglądanie ich wyjścia jako zawartości aktywnego panelu.

Na przykład, jeśli chcesz aby w aktywnym panelu wyświetlone zostały
wszystkie dowiązania w aktywnym katalogu, możesz użyć panelu zewnętrznego
i następującej komendy:

```
find . -type l -print
```

Zanim komenda zakończy działanie, zawartość katalogów nie będzie już dłużej
zawartością aktualnego katalogu, ale wszystkie pliki będą symbolicznymi
dowiązaniami.

Jeśli chcesz wyświetlić wszystkie pliki, które ściągnąłeś ze swoich
serwerów ftp, możesz użyć tej komendy awk żeby wypisać nazwę pliku z
logów transferu:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

Możesz zapisać sobie często używane komendy pod jakąś nazwą, po to
żeby móc ich potem używać dużo łatwiej. Robisz to po prostu wpisując komendę
w linii wejściowej, a potem naciskająć przycisk Add. Potem wpisujesz nazwę,
pod jaką ta komenda ma być widoczna. Następnym razem po prostu wybierasz
tę komendę z listy i nie musisz już wpisywać jej ponownie.

### Hotlista katalogów <a id="hotlist"></a>

Hotlista pokazuje nazwy miejsc wprowadzonych do niej, a program przechodzi do
miejsca wybranej nazwy. Miejscem może być katalog, ścieżka wewnątrz
wirtualnego systemu plików albo adres wtyczki panelu, na przykład
*sftp:host/katalog.*
Z okna można wyrzucać już dodane pary nazwa/miejsce i dodawać nowe. Bieżące
miejsce, katalog albo panel wtyczki, najszybciej dodaje polecenie Dodaj do
hotlisty (C-x h), które pyta tylko o nazwę. Miejsce, które już jest na
liście, nie jest dodawane drugi raz: okno pokazuje istniejącą pozycję.

Klawisze okna:

```
Enter        przechodzi do wybranego miejsca
Alt-o        otwiera wybrane miejsce w drugim panelu
Ctrl-Enter   wstawia "cd miejsce" do wiersza poleceń
Alt-Enter    to samo, dla terminali bez Ctrl-Enter
Insert       dodaje bieżące miejsce
Shift-F4     nowa pozycja: pyta o nazwę i miejsce
F7           nowa grupa
F4           edytuje nazwę i miejsce pozycji
Delete       kasuje pozycję
Ctrl-Up      przesuwa pozycję o wiersz w górę
Ctrl-Down    przesuwa pozycję o wiersz w dół
F6           przenosi pozycję do innej grupy: okno wypisuje
             grupy, Enter otwiera grupę, Przenieś albo kolejne
             F6 wstawia pozycję na koniec pokazanej grupy,
             także tej wyjściowej
F9           porządkuje bieżącą grupę po nazwie, grupy pierwsze
Ctrl-s       szuka na liście w trakcie pisania, Ctrl-s dalej
Right, Left  wchodzi do grupy i z niej wychodzi
```

Dzięki temu przechodzenie do często używanych katalogów jest szybsze. Można
też skorzystać ze zmiennej CDPATH, opisanej przy
[wewnętrznym poleceniu cd](#the-cd-internal-command).

### Prace w tle (Background jobs) <a id="background-jobs"></a>

Pozwalają ci one kontrolować status jakichkolwiek procesów wykonywanych
w tle
przez M-Commandera (tylko operacje kopiowania i przenoszenia, mogą
być wykonywane w tle). Z tego menu możesz zastopować, zresetować i "zabić"
proces w tle.

### Edycja menu użytkownika (Edit Menu File) <a id="edit-menu-file"></a>

Menu użytkownika to menu przydatnych działań, które użytkownik sam sobie
układa. Występuje w dwóch postaciach: menu, które samo siebie edytuje,
trzymane w pliku kluczy, oraz starszy plik menu pisany ręcznie. Tam, gdzie
plik kluczy istnieje, to jego otwiera F2; gdzie go nie ma, czytany jest stary
plik jak dawniej.

**Menu, które samo siebie edytuje**

Pozycje stoją w pliku .mc6menu bieżącego katalogu oraz w
~/.config/mc6/menu.ini i pokazywane są razem. Plik .mc6menu czytany jest
tylko wtedy, gdy należy do tego użytkownika albo do roota i nikt inny nie
może do niego pisać, bo jego pozycje uruchamiają polecenia. Nic innego nie
jest czytane: w menu stoi to, co włożył jego właściciel, a program nie wnosi
żadnej pozycji, więc menu nowego użytkownika jest puste i prosi o pierwszą.
Wewnątrz menu:

```
Enter        uruchamia pozycję
Ins          dodaje pozycję
F4           edytuje pozycję
F5           wnosi pozycje z menu pisanego ręcznie
Shift-F4     otwiera plik, w którym stoi pozycja
Del          kasuje pozycję
Ctrl-Up      przesuwa pozycję wyżej
Ctrl-Down    przesuwa pozycję niżej
Alt-A        pokazuje też pozycje ukryte w tym miejscu
```

Pozycja to klawisz skrótu, napis, polecenia oraz warunki, które mówią,
gdzie jest pokazywana. W poleceniach działają te same podstawienia co w
starym menu, %f, %s, %{prompt} i pozostałe, opisane w rozdziale
[obsługa makr](#macro-substitution).
Dwa pola wyboru mówią, co zrobić z wyjściem: czy ma iść do podglądu i czy
polecenie ma działać bez powłoki panelu.

Wiersz "Show when" okna pokazuje warunki pozycji, a przycisk Conditions
otwiera dla nich osobne okno: maski ścieżki, typy plików, dla których
jest pozycja (żadne zaznaczone pole oznacza dowolny typ), tylko
pliki wykonywalne, tylko gdy są zaznaczone pliki, programy potrzebne
poleceniom, panel, na który patrzą warunki, oraz to, czy menu otwiera się
na tej pozycji: nigdy, zawsze albo wtedy, gdy są spełnione warunki, które
otwiera przycisk obok "When". Pozycji, której warunki nie są spełnione,
nie ma na liście. Okno pokazuje jeden panel naraz; warunki, które patrzą
na oba panele, zmienia się w pliku.

Gdy pole "Regular expression" jest zaznaczone, pole ścieżki przyjmuje
wyrażenie regularne zamiast masek; w pliku jest to klucz path~=.

Tytuł menu mówi, ile pozycji jest ukrytych w tym miejscu. Alt-A pokazuje
także je, przygaszonym kolorem: można je wtedy edytować, przesuwać i
usuwać, ale nie uruchamiać.

Napis pokazywany jest już z wykonanymi podstawieniami, więc napis "print %f"
stoi na liście z nazwą pliku pod kursorem. To, co zawiera plik, przy tym się
nie zmienia, a %{...} zostaje tak, jak zapisano: lista nie jest miejscem na
pytania.

Pozycja może być podmenu zamiast polecenia: Ins pyta, które z dwojga dodać.
Podmenu widać z ukośnikiem po nazwie, tak jak katalog; Enter je otwiera, a
tytuł nazywa te podmenu, w których się znajdujesz. Esc wychodzi o poziom
wyżej, a na najwyższym wychodzi z menu. Skasowanie podmenu kasuje też to, co
w nim stoi. W pliku podmenu to grupa z submenu=true i bez polecenia, a
pozycja w nim nazywa podmenu w parent=.

Polecenia to pole z wielu wierszy: Enter otwiera nowy, a strzałki, Home i End
chodzą po tekście. Shift z ruchem zaznacza to, po czym ruch przechodzi, mysz
zaznacza przeciąganiem, a Ctrl-Insert, Shift-Insert i Shift-Delete kopiują,
wklejają i wycinają przez plik wymiany, tak samo jak w wierszu wprowadzania.
Przycisk Edytor wychodzi z okna i otwiera plik, w którym stoi pozycja, do
tego, co łatwiej napisać tam.

Pozycja zapisywana jest z powrotem do pliku, z którego przyszła, a kolejność
listy to kolejność pliku. Zapisywane jest tylko to, co zmieniono:
komentarze, puste wiersze i klucze, których menu nie zna, zostają tak, jak
były, a komentarz nad pozycją przesuwa się razem z nią. Shift-F4 otwiera
plik.

**Plik menu**

Plik jest pomyślany także do czytania i zmieniania ręcznie. Każda pozycja
to grupa: jej napis w nawiasach kwadratowych, a potem po jednym
klucz=wartość w każdym wierszu. Wartość z jednego wiersza stoi po "=" tak,
jak jest, bez cudzysłowów i znaków ucieczki. Polecenie z wielu wierszy
stoi między dwoma wierszami z trzema odwrotnymi apostrofami, a tekst
między nimi brany jest dokładnie tak, jak zapisano:

````
# mc menu format 2

[Open in vim]
hotkey=v
on=file
command=vim %f

[Pack the directory into tar.gz]
hotkey=t
on=dir
command=```
echo -n "Archive name [%f]: "
read name
name=${name:-%f}
tar czf "$name.tar.gz" %f
```
````

Jeśli same polecenia zawierają wiersz z trzema odwrotnymi apostrofami,
blok otwiera się i zamyka czterema. Wiersze zaczynające się od '#' to
komentarze. Pierwszy wiersz to zawsze "# mc menu format 2", a menu czyta
tylko pliki, które go mają. Dla pliku bez niego, zapisanego przez starszą
wersję lub ręcznie, menu raz proponuje przekształcenie i zachowuje plik w
dawnej postaci jako menu.ini.old.

Te klucze mówią, gdzie pozycja jest pokazywana. Kilka kluczy musi być
spełnionych naraz.

*path=*
: Maski ścieżki tego, na czym stoi kursor, czytane tak, jak git czyta
wzorce z pliku .gitignore; patrz niżej.

*path~=*
: Wyrażenie regularne zamiast masek, szukane w dowolnym miejscu ścieżki:
path~=^/home/me/dev/. Jeśli nie ma w nim '/', patrzy tylko na ostatnią
część ścieżki: path~=^ttyS. '!' na początku je odwraca. Wartość to jedno
wyrażenie, więc ';' i '|' są jego częścią.

*on=*
: To, na czym stoi kursor: file, dir, link, broken, char, block, fifo,
socket, rozdzielone ';'; wystarczy dowolne z nich. Dowiązanie do katalogu
liczy się jako katalog, dowiązanie do pliku jako plik, a link pasuje do
każdego dowiązania. '!' odwraca typ: on=!dir to wszystko oprócz katalogu.

*exec=true*
: Plik pod kursorem nie jest katalogiem i jest wykonywalny.

*marked=true*
: Panel ma zaznaczone pliki; marked=false, nie ma żadnych.

*needs=*
: Programy, które muszą się znaleźć w PATH, albo pełne ścieżki; wszystkie.

*other.path=, other.path~=, other.on=, other.exec=, other.marked=*
: To samo dla drugiego panelu.

*default=true*
: Menu otwiera się na tej pozycji wszędzie tam, gdzie jest ona pokazana.

*default.path=, default.on=, default.other.path=, ...*
: Dowolny klucz z powyższych z "default." na początku: menu otwiera się na
tej pozycji tam, gdzie te klucze są spełnione, a pozycja jest nadal
pokazana tam, gdzie mówią jej własne klucze. default.path=ttyS\* otwiera
menu na tej pozycji, gdy kursor stoi na porcie szeregowym. Gdy można
wybrać kilka pozycji, wygrywa pierwsza.

Maska z path= porównywana jest ze ścieżką tego, na czym stoi kursor, tak,
jak .gitignore porównuje swoje wzorce:

```
*.c               plik .c w dowolnym miejscu
~/dev/mc/*.c      plik .c wprost w ~/dev/mc
~/dev/mc/**/*.c   plik .c tam lub w dowolnym katalogu niżej
~/dev/mc/**       cokolwiek w tym drzewie
**/src/*          cokolwiek wprost w katalogu src
build/            katalog o nazwie build
*.c;!test_*.c     źródła C oprócz testów
```

Maska bez '/' porównywana jest tylko z ostatnią częścią ścieżki, na
dowolnym poziomie: \*.1 jest spełniona na katalogu rrr.1, ale nie na
pliku uu.2 w nim. Maska z '/' na początku porównywana jest z całą
ścieżką; maska z '/' w środku zaczyna od katalogu pliku .mc6menu, a w
menu.ini jest spełniona na dowolnym poziomie. '\*' i '?' nie przechodzą
przez '/', '\*\*' przechodzi przez dowolną liczbę katalogów, [abc] i
[a-z] to zbiory znaków, '/' na końcu wymaga katalogu, a '~' na początku
to katalog domowy. Inaczej niż w .gitignore, maska spełniona na katalogu
nic nie mówi o plikach w nim.

Maski czytane są od lewej do prawej, a rozstrzyga ostatnia pasująca;
maska z '!' na początku mówi nie. Gdy żadna maska nie pasuje, pozycja
nie jest pokazywana: samo !\*.o nie pokazuje niczego, a "wszystko oprócz
plików obiektowych" to \*;!\*.o. Na ".." ścieżką jest katalog panelu z
dopisanym "/..", więc maska tego katalogu jest na nim spełniona, a maska
nazwy nie.

Pliku, którego menu nie umie przeczytać, nie pokazuje się jako pustego
menu: komunikat nazywa błędny wiersz i proponuje otwarcie pliku, a menu
nie pisze do pliku, dopóki nie zostanie poprawiony.

**Plik menu pisany ręcznie**

Instalacja już takiego nie wnosi; to, co następuje, czytane jest tam, gdzie
ktoś trzyma własne menu w starszej postaci: brany jest plik .usermenu z
bieżącego katalogu, jeśli istnieje, ale tylko wtedy, gdy należy do
użytkownika albo do roota i nie każdy może do niego pisać. Jeśli takiego
pliku nie ma, tak samo próbuje się z ~/.config/mc6/menu.

Jeśli menu, które samo siebie edytuje, nie ma jeszcze pliku, a znajdzie się
inne menu (własne pisane ręcznie, usermenu zainstalowanego programu albo
menu.ini starszej wersji), program raz na sesję proponuje jego wniesienie; F5
w menu i przycisk Wnieś w pustym menu proszą o to w dowolnej chwili, dla tego
pliku albo dla wskazanego ręcznie. Potem pokazuje, co plik zawiera: spacja
zaznacza pozycję, Ins zaznacza ją i schodzi niżej, '\*' odwraca wszystkie
zaznaczenia, a Enter przenosi zaznaczone do ~/.config/mc6/menu.ini, gdzie
można je już edytować oknem. Plik źródłowy zostaje na miejscu.

Warunki nad pozycją stają się jej kluczami tam, gdzie klucze umieją je
powiedzieć: "+ f \\.c$ | f \\.h$ & t r" staje się path=\*.c;\*.h i
on=file, a wyrażenie regularne staje się maskami tam, gdzie pasują do tych
samych nazw. Wiersz "=", który wybierał pozycję, na której otwiera się
menu, staje się tak samo kluczami default.*. Warunek, którego klucze nie
umieją powiedzieć, na przykład alternatywa po "&", zostaje jako komentarz
nad pozycją, a wnoszenie mówi, ile ich było.

Wyrażenie regularne, którego żadna maska nie wyrazi, trafia bez zmian do
path~=.

Format pliku z menu użytkownika jest bardzo prosty. Linie zaczynające się
od czegokolwiek innego niż spacja lub tabulacja, są traktowane jako
wtyczki do menu (aby móc używać ich potem jako gorących klawiszy, dobrze
jest aby pierwszy znak był literą). Wszystkie linie zaczynające od spacji
lub tabulacji, są komendami, które mają być wykonane jeśli wtyczka zostanie
wybrana.

Kiedy opcja zostaje wybrana, wszystkie komendy należące do tej opcji
kopiowane są do pliku w katalogu tymczasowym (najczęściej do /usr/tmp), a
potem plik jest wykonywany. Pozwala to użytkownikowi wkładać normalne
konstrukcje powłoki do konstrukcji kodu wykonywanego. Po więcej
informacji zobacz, używania makr.

To jest przykładowy plik usermenu:

```
A	Wyrzuć aktualny plik.
	od -c %f

B	Stwórz raport o błędzie i wyślij do roota.
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
	vi $I
	mail -s "Błąd M-Commandera" root < $I
	rm -f $I

M	Przeczytaj pocztę.
	emacs -f rmail

N	Przeczytaj grupę dyskucyjną.
	emacs -f gnus

J	Skopiuj rekursywnie cały aktualny katalog.
	tar cf - . | (cd %D && tar xvpf -)

= f *.tar.gz | f *.tgz & t n
X       Zdekompresuj skompresowany plik tar.
	tar xzvf %f
```

**Standardowe warunki**

Każda opcja może być opatrzona w warunki. Warunek musi zaczynać się od
pierwszej kolumny i od znaku '='. Jeśli warunek jest prawdziwy, opcja
stanie się opcją domyślną.

```
Składnia warunku: 	= <warunek>
	    lub:	= <warunek> | <warunek> ...
	    lub:	= <warunek> & <warunek> ...

Warunek jest jednym z następujących:

  f <wzorzec>           aktualny plik zgodny z wzorcem?
  F <wzorzec>           plik w drugim panelu zgodny z wzorcem?
  d <wzorzec>           aktualny katalog spełniający wzorzec?
  D <wzorzec>           katalog w drugim panelu spełniający wzorzec?
  t <typ>               aktualny pliku typu typ?
  T <typ>               plik w drugim panelu typu typ?
  ! <warunek>           zaprzeczenie warunku
```

Wzorzec jest normalnym wzorcem powłoki lub wyrażeniem,
podobnym do wzorca powłoki. Możesz zmienić globalne ustawienia
wzorców powłoki pisząc "shell_patterns=x" w pierwszej linii menu
użytkownika (x jest równe 0 lub 1).

```
Typ jest jednym lub więcej z podanych znaków:

  n	nie katalog
  r	zwykły plik
  d	katalog
  l	dowiązanie
  c	specjalny znak
  b	specjalny blok
  f	fifo
  s	gniazdo
  x	wykonywalny
  t	zaznaczony
```

Na przykład 'rlf' oznacza zwykły plik, dowiązanie lub fifo. Typ 't' jest
trochę odmienny ponieważ dotyczy panelu a nie pliku. Warunek '=t t' jest
prawdziwy jeśli są jakieś zaznaczone pliki w aktywnym panelu, a fałszywy jeśli
nie ma.

Jeśli warunek rozpoczyna się od '=?' zamiast '=' droga przechodzenia
przez warunki będzie pokazywana
za każdym razem kiedy warunek będzie obliczany [przydatne do wyszukiwania błędów
\- przyp. tłumacza].

Warunki są obliczane od lewej do prawej. Oznacza to, że

```
	= f *.tar.gz | f *.tgz & t n
```

jest liczone tak samo jak

```
	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

To jest prosty przykład zastosowania tych warunków:

```
= f *.tar.gz | f *.tgz & t n
L	Listuje zawartość skompresowanego archiwum tar
	gzip -cd %f | tar xvf -
```

**Warunki dodania**

Jeśli warunek rozpoczyna się od znaku '+' (lub '+?') zamiast od '=' (lub '=?')
jest to warunek dodania. Jeśli warunek jest prawdziwy, opcja menu będzie
dołączona do menu. Jeśli nie jest prawdziwy, nie będzie ona w nim zawarty.

Możesz łączyć ze sobą standardowe i dodane warunki zaczynając warunek od
kombinacji
'+=' lub '=+' (lub '+=?' lub '=+?' jeśli chcesz zobaczyć trasę błędów).
Jeśli chcesz użyć różnych warunków, dodanego i standardowego,
możesz poprzedzić wpis menu dwoma wierszami warunkowymi. Jednym zaczynającym
się od znaku '+', a drugim od '='.

Wszelkie komentarze rozpoczynają się od znaku '#'.

## Menu opcji (Options Menu) <a id="options-menu"></a>

Program ma opcje, które można włączać i wyłączać w oknach dostępnych z tego
menu. Opcja jest włączona, jeśli stoi przed nią gwiazdka albo "x". Menu
zawiera, w tej kolejności:

Polecenie
[Konfiguracja](#configuration)
otwiera okno, w którym można zmienić większość ustawień programu.

Polecenie
[Układ](#layout)
otwiera okno z ustawieniami tego, jak program wygląda na ekranie.

Polecenie
[Opcje paneli](#panel-options)
otwiera ustawienia paneli menedżera plików.

Polecenie
[Tryby panelu plików](#panel-modes)
otwiera listę nazwanych układów listy, gdzie się je tworzy, edytuje i usuwa.

Polecenie
[Potwierdzenia](#confirmation)
otwiera okno, w którym ustala się, przy których działaniach program ma pytać
o potwierdzenie.

Polecenie
[Wygląd](#appearance)
służy do wyboru skórki.

Polecenie
[Nauka klawiszy](#learn-keys)
uczy program tych klawiszy, których niektóre terminale nie wysyłają jak
trzeba.

Polecenie
[Przypisania klawiszy](#key-bindings)
otwiera listę działań wraz z klawiszami, na które odpowiadają; tam zmienia
się klawisz, a wynik trafia do pliku przypisań.

Polecenie
[Podsłuch klawiszy](#key-sniffer)
pokazuje, co terminal wysyła dla naciśniętego klawisza, i działanie, do
którego ten klawisz jest przypisany.

Polecenia
**Opcje podglądu różnic**,
[Opcje przeglądarki](mview.md#viewer-options)
i
**Opcje edytora**
otwierają okna trzech programów, które pokazują plik: porównania, podglądu i
edytora. Te same okna są w menu Opcje każdego z nich; tutaj sięga się do nich
bez otwierania pliku. Porównanie bierze swoje opcje przy starcie, więc to już
otwarte zostaje przy tych, z którymi je otwarto.

Polecenie
[Zarządzanie wtyczkami](#panel-plugins)
wypisuje wczytane wtyczki, wyłącza wybraną i otwiera jej ustawienia.

Komenda edycji rozszerzeń plików (Edit Extension File) pozwala na własny wybór
programów, które mają być używane do wykonywania plików z podanymi
rozszerzeniami.

Komenda
**Modyfikuj plik wyróżniania grup**
otwiera plik, który mówi, jakie nazwy i jakie typy plików panel pokazuje jakim
kolorem, zobacz
[Podświetlanie nazw plików](#filenames-highlight).

Polecenie
[Zapisz ustawienia](#save-setup)
zapisuje bieżące ustawienia menu Lewy, Prawy i Opcje. Zapisywana jest też
niewielka liczba innych ustawień.

Polecenie
**O programie**
pokazuje wersję programu i to, kto go napisał.

### Konfiguracja <a id="configuration"></a>

Opcje w tym oknie są podzielone na trzy grupy:
opcje panelu (Panel Options), zatrzymaj po uruchomieniu (Pause after run) i
inne opcje (Other Options).

**Opcje panelu**

*Show Backup Files.*
Standardowo M-Commander nie wyświetla plików kończących się znakiem
'~' (tak jak komenda ls -B w wersji GNU).

*Show Hidden Files.*
Standardowo M-Commander wyświetla wszystkie pliki zaczynające się
od kropki (tak jak ls -a).

*Mark moves down.*
Standardowo kiedy zaznaczasz plik (zarówno przy klawisze Insert)
linia wyboru przenosi się o jedno w dół.

*Drop down menus.*
Kiedy ta opcja jest włączona, kiedy naciskasz klawisz
**F9**
menu będzie aktywowane, w przeciwnym wypadku zostaniesz tylko przeniosiony
do tytułów w tym menu i będziesz musiał wybrać opcję ręcznie przy użyciu
strzałek bądź też przy użyciu pierwszej litery z nazwy konkretnego menu.

*Mix all files.*
Jeśli ta opcja jest włączona, wszystkie pliki i katalogi są pomieszane razem.
Jeśli zaś jest wyłączona, katalogi (i dowiązania do nich), są listowane na
początku a pozostałe pliki dopiero za nimi.

*Fast directory reload.*
Standardowo ta opcja jest wyłączona. Jeśli ją włączysz M-Commander
będzie używał triku do sprawdzenia czy zawartość katalogu się zmieniła.
Trik polega na tym, że sprawdza się i-węzeł katalogu i jeśli się on zmienił
to katalog jest ładowany na nowo. Oznacza to przeładowywanie zawartości panelu
tylko wtedy, kiedy tworzysz lub kasujesz pliki. Jeśli robisz inne zmiany
(rozmiaru, właściciela, uprawnień, grupy itp.) będziesz musiał ręcznie przeładować
widok (np. używając kombinacji klawiszy C-r).

**Zatrzymaj po uruchomieniu**

Po wykonaniu komendy, M-Commander może zrobić pauzę, po to abyś
mógł spokojnie przejrzeć wyjście ostatniej komendy. Są trzy możliwe wartości
dla tej zmiennej:

> *Nigdy (Never)*
> Oznacza, że nie chcesz widzieć wyjścia twojej komendy. Jeśli używasz
> termianala typu Linux lub FreeBSD czy też xterm, będziesz mógł jednak
> zobaczyć jej wyjście naciskając C-o.

> *On dumb terminals*
> Będziesz miał pauzę po uruchomieniu na terminalach, które nie są w stanie
> pokazywać widoku ostatniej komendy (na wszystkich terminalach, które nie są
> xtermami lub Linux).

> *Zawsze (Always)*
> Program zatrzyma się po wykonaniu każdej z twoich komend.

**Inne opcje**

*Operacje weryfikacji (Verbose operation).*
Przełącza czy podczas kopiowania, kasowania, przenoszenia plików ma być
pokazywane okno dialogowe pokazujące stopień zaawansowania. Jeśli masz powolny
terminal, możesz chcieć wyłączyć weryfikację. Jest to wykonywane automatycznie
za ciebie jeśli twój terminal jest wolniejszy niż 9600 bps.

*Zliczaj wszystko (Compute totals).*
Jeśli ta opcja jest włączona, M-Commander zlicza wszytkie bajty
plików, które są przeznaczone do kopiowania, przenoszenia, kasowania. Spowoduje
to wyświetlanie dużo bardziej zaawansowanego wskaźnika postępu w zamian
zmiejszając trochę prędkość. Ta opcja nie ma żadnego znaczenia jeśli opcja
*Verbose operation*
jest wyłączona.

*Wzorce powłoki (Shell patterns).*
Standardowo komendy zaznacz (Select), odznacz (Unselect), i filtruj (Filter)
będą używać wyrażeń takich samych jak powłoka. Oznacza to, że
gwiazdka oznacza zero lub więcej znaków, znak zapytania dokładnie jeden znak,
a każdy inny znak sam siebie. Jeśli ta opcja jest wyłączona, stosowane są
te, których używa w komenda
**ed**(1).

*Auto Save Setup.*
Jeśli ta opcja jest włączona, kiedy wychodzisz z M-Commandera
konfiguracja M-Commander zostanie zachowana automatycznie (bez pytania)
do pliku ~/.config/mc6/ini.

*Auto menus.*
Jeśli ta opcja jest włączona, menu użytkownika będzie włączone na starcie.
Użyteczne do budowania menu dla nie unixowców.

*Używaj wewnętrznego edytora (Use internal editor).*
Jeśli ta opcja jest włączona, do edycji plików używany jest wbudowany
edytor plików. Jeśli ta  opcja jest wyłączona, używany będzie edytor wybrany
w zmiennej
**EDITOR**.
Jeśli żaden edytor nie został wybrany, używany będzie
**vi**(1).
Zobacz sekcję Wewnętrzny edytor plików.

*Używaj wewnętrznego podglądu (Use internal viewer).*
Jeśli ta opcja jest włączona, wbudowany podgląd pliku jest używany do oglądania
pliku. Jeśli ta opcja jest wyłączona, używany jest podgląd wybrany w zmiennej
**PAGER**.
Jeśli żaden podgląd nie został wybrany, wywoływana jest komenda
**view**.
Zobacz sekcję Wbudowany podgląd plików.

*Dokańczanie: pokaż wszystkie (Complete: show all).*
Standardowo M-Commander pokazuje wszystkie możliwe dokończenia
jeśli jest ich więcej, kiedy naciśniesz drugi raz klawisz
**M-Tab**,
za pierwszym razem, po prostu dokańcza to na ile można i wydaje krótki
dźwięk. Jeśli chcesz widzieć wszystkie możliwości po pierwszym naciśnięciu
**M-Tab**
włącz tę opcję.

*Obrotowy myślnik (Rotating dash).*
Jeśli ta opcja jest włączona, M-Commander będzie pokazywał obracający
się myślnik w lewym górnym rogu, jeśli będzie akurat w trakcie wykonywania
jakiegoś procesu.

*Lynx-like motion.*
Jeśli ta opcja jest włączona, możesz używać strzałek przemieszczenia
żeby automatycznie zmieniać katalog jeśli aktualnie wybrany katalog jest
podkatalogiem, a linia poleceń jest pusta. Standardowo ta opcja jest wyłączona.

*Dowiązania podążające cd (Cd follows links).*
Ta opcja, jeśli jest włączona, zmusza M-Commandera żeby podążał
za łańcuchem katalogów przy zmienianiu go w panelu czy za pomocą komendy cd.
To jest standardowe zachowanie basha. Jeśli jest wyłączona, M-Commander
podąża za prawdziwą strukturą katalogów, więc cd .. jeśli wszedłeś do
katalogu poprzez dowiązanie, przeniesie cię do prawdziwego katalogu na dysku, a nie
tam gdzie wskazywało dowiązanie.

*Bezpieczne kasowanie (Safe delete).*
Jeśli ta opcja jest włączona, nieumyślne kasowanie plików stanie się
dużo trudniejsze. Standardowy wybór w linii potwierdzenia zmienia się z
"Yes" na "No". Standardowo ta opcja jest wyłączona.

### Układ (Layout) <a id="layout"></a>

To okno pozwala zmienić ogólny układ ekranu. Ustawienia podzielone są na trzy
grupy: "Podział paneli", "Wyjście konsoli" i "Inne ustawienia".

**Podział paneli**

Resztę ekranu zajmują dwa panele. Można podać, czy podział ma być
*pionowy*
czy
*poziomy*.
Podział zmienia też skrót Alt-, (Alt-przecinek).

*Równy podział.*
Domyślnie panele mają ten sam rozmiar. Tą opcją można podzielić ekran
nierówno.

**Wyjście konsoli**

Na konsoli Linuksa albo FreeBSD można podać, ile wierszy widać w oknie
wyjścia. Ta opcja jest dostępna tylko na konsoli systemowej.

**Inne ustawienia**

*Pasek menu widoczny.*
Przy włączonej opcji menu główne jest zawsze widoczne w górnym wierszu
ekranu, nad panelami. Domyślnie włączona.

*Wiersz poleceń.*
Przy włączonej opcji wiersz poleceń jest dostępny. Domyślnie włączona.

*Pasek klawiszy widoczny.*
Przy włączonej opcji dziesięć napisów klawiszy F1-F10 stoi w dolnym wierszu
ekranu. Domyślnie włączona.

*Pasek podpowiedzi widoczny.*
Przy włączonej opcji jednowierszowe podpowiedzi widać pod panelami.
Domyślnie włączona.

*Tytuł okna XTerm.*
W emulatorze terminala dla X11 program ustawia tytuł okna na bieżący katalog
i uaktualnia go, kiedy trzeba. Jeśli twój emulator terminala jest zepsuty i
przy starcie albo zmianie katalogu widać dziwne wyjście, wyłącz tę opcję.
Domyślnie włączona.
Dopóki w terminalu za panelami działa polecenie, tytuł zaczyna się od jego
nazwy.

*Pokaż wolne miejsce.*
Przy włączonej opcji wolne i całkowite miejsce bieżącego systemu plików widać
w dolnej ramce panelu. Domyślnie włączona.

### Potwierdzanie (Confirmation) <a id="confirmation"></a>

W tym menu możesz skonfigurować opcje potwierdzania dla kasowania,
zastępowania, wykonywania przez naciśnięcie klawisza Enter, jak również
wychodzenia z programu.

### Nauka klawiszy (Learn keys) <a id="learn-keys"></a>

To okno uczy program tych ciągów sterujących, które twój terminal wysyła dla
klawiszy funkcyjnych, strzałek i klawiszy ruchu.

Wybierz polami wyboru zestaw modyfikatorów (Ctrl, Alt, Shift), a potem
naciśnij przycisk szukanego klawisza. Naciśnij sam klawisz na klawiaturze i
poczekaj, aż komunikat o przechwyceniu zniknie. Nauczony ciąg pojawi się obok
przycisku.

**Del**
\- zapomina nauczony klawisz.

**Zapisz**
\- zapisuje nauczone klawisze do ~/.config/mc6/term/\<TERM>.

**Edytuj plik terminala**
\- otwiera w edytorze plik z definicjami klawiszy terminala.

Stare definicje z sekcji [terminal:TERM] pliku ~/.config/mc6/ini są
przenoszone same przy pierwszym uruchomieniu.

### Zarządzanie wtyczkami <a id="manage-plugins"></a>

Wtyczki, które program wczytał, w tabeli: rodzaj, nazwa i to, co wtyczka mówi
o sobie. Pole wyboru w wierszu wyłącza ją i włącza; to, co wyłączone, nie
wczytuje się także następnym razem.

**Enter, F4**
: Otwiera ustawienia wtyczki, na której stoi kursor. Ta, która ich nie ma, sama
o tym mówi.

Wymienione są tu
[wtyczki paneli](#panel-plugins)
razem z wtyczkami edytora i pakietami skryptów Lua; skrypty pakietu pokazuje
okno
[Skrypty Lua](#lua-scripts)
z jego ustawień.

### Skrypty Lua <a id="lua-scripts"></a>

Skrypty pakietu Lua w tabeli: nazwa, identyfikator, gdzie skrypt się znajduje,
co daje i co robi. Pole wyboru w wierszu wyłącza go i włącza.

**Ustawienia**
: Uruchamia skrypt z ustawieniami pakietu, jeśli taki jest.

### Plik istnieje <a id="plugin-file-exists"></a>

Kopiowanie do panelu wtyczki zastało tam plik o tej samej nazwie. Okno pokazuje
ścieżkę, rozmiar i czas tego, co jest kopiowane, oraz tego, co już tam jest, i
pyta, co zrobić: nadpisać, pominąć, wznowić kopiowanie od miejsca przerwania,
jeśli wtyczka to potrafi, albo przerwać całą operację.

### Wybór kodowania <a id="codepages-translation"></a>

Lista kodowań, które program zna, z pliku
**{{pkgdatadir}}/charsets**.
Wybór kodowania mówi programowi, w jakim zapisane są nazwy lub tekst, a pozycja
**\<Bez tłumaczenia>**
zostawia je jako bajty. Listę otwiera
**Alt-e**
w panelu, w przeglądarce i w edytorze, a także odpowiednia pozycja ich menu.

### Historia linii wejściowej <a id="history-query"></a>

Lista tego, co wpisywano wcześniej w linię wejściową, od ostatniego wpisu;
otwiera ją
**Alt-h**
dla tej linii, w której stoi kursor. Enter wstawia do linii wpis, na którym
stoi kursor, Esc zostawia linię bez zmian, a
**F8, Del**
usuwa wpis z historii.

### Opcje paneli <a id="panel-options"></a>

**Główne opcje paneli**

*Pokaż mini-status.*
Przy włączonej opcji na dole paneli widać wiersz informacji o pozycji pod
kursorem. Domyślnie włączona.

*Jednostki SI.*
Przy włączonej opcji program używa przedrostków SI (podstawa 10) przy
pokazywaniu rozmiarów. Przy wyłączonej (domyślnie) używa przedrostków IEC
(podstawa 2).

*Mieszaj wszystkie pliki.*
Przy włączonej opcji pliki i katalogi widać wymieszane. Przy wyłączonej
(domyślnie) katalogi (i dowiązania do katalogów) stoją na początku listy, a
pozostałe pliki pod nimi.

*Pokaż pliki zapasowe.*
Przy włączonej opcji widać też pliki kończące się tyldą, inaczej nie (jak
opcja -B polecenia ls). Domyślnie włączona.

*Pokaż pliki ukryte.*
Przy włączonej opcji widać też pliki zaczynające się kropką (jak ls -a).
Domyślnie wyłączona.

*Szybkie odświeżanie katalogów.*
Przy włączonej opcji program używa sztuczki, aby stwierdzić, czy zawartość
katalogu się zmieniła: czyta katalog na nowo tylko wtedy, gdy zmienił się
jego i-węzeł, czyli gdy powstał albo zniknął plik. Jeśli zmienia się i-węzeł
pliku (rozmiar, prawa, właściciel), obraz nie jest odświeżany; wtedy trzeba
odczytać katalog ręcznie (C-r). Domyślnie wyłączona.

*Zaznaczanie przesuwa w dół.*
Przy włączonej opcji linia wyboru schodzi niżej, gdy zaznaczasz plik
(klawiszem Insert). Domyślnie włączona.

*Odwracaj tylko pliki.*
Przy włączonej opcji "Odwróć zaznaczenie" z menu Plik dotyczy tylko plików, a
nie także katalogów. Domyślnie włączona.

*Prosta zamiana.*
Jeśli oba panele pokazują listę plików, prosta zamiana oznacza, że panele
zamieniają się miejscami na ekranie: lewy staje się prawym i odwrotnie. Przy
wyłączonej opcji panele zamieniają się zawartością, zachowując układ listy i
sortowanie. Domyślnie wyłączona.

*Automatyczny zapis ustawień paneli.*
Przy włączonej opcji przy wyjściu program zapisuje bieżące ustawienia paneli
do pliku ~/.config/mc6/panels.ini. Domyślnie wyłączona.

*Obserwuj katalogi.*
Przy włączonej opcji program prosi jądro, aby informowało go o zmianach w
katalogach pokazywanych przez panele, i czyta panel na nowo, gdy plik w nim
powstaje, znika albo zmienia się za sprawą czegoś innego: innego terminala,
kompilacji albo powłoki z okna terminala. Panel poza ekranem jest czytany na
nowo, gdy wraca. Jedna obserwacja obejmuje cały katalog, więc koszt nie
zależy od liczby plików, a seria zmian daje jedno ponowne czytanie. Katalogi
wirtualnego systemu plików i te na systemie plików, którego jądro nie potrafi
obserwować, takim jak NFS, działają jak wcześniej: tam robi to C-r. Dopóki ta
opcja jest włączona, szybkie odświeżanie nie ma czego oszczędzać i widać je
jako wyłączone. Domyślnie włączona.

**Poruszanie się**

*Ruch w stylu lynksa.*
Przy włączonej opcji strzałkami można zmieniać katalog, gdy pod kursorem stoi
podkatalog, a wiersz poleceń jest pusty. Domyślnie wyłączona.

*Przewijanie stronami.*
Przy włączonej opcji (domyślnie) panel przewija się o pół ekranu, gdy kursor
dojdzie do końca albo początku panelu, inaczej przewija się po jednym pliku.

*Przewijanie wyśrodkowane.*
Przy włączonej opcji panel przewija się, gdy kursor dojdzie do środka, a do
góry albo do dołu panelu dochodzi tylko na pierwszym albo ostatnim pliku.
Dotyczy przewijania po jednym pliku, nie klawiszy stron.

*Przewijanie stronami myszą.*
Określa, czy kółko myszy przewija panele stronami, czy wiersz po wierszu.

**Podświetlanie plików**

Można podać, czy
*prawa*
i
*typy plików*
mają być podświetlane osobnymi
[kolorami](#colors).
Jeśli podświetlanie praw jest włączone, te części pól
*perm*
i
*mode*
[układu listy](#listing-format),
które dotyczą użytkownika uruchamiającego program, dostają kolor podany
słowem kluczowym
*marked*.
Jeśli włączone są
*kolory praw*,
każdy znak pola
*perm*
dostaje kolor tego, co oznacza: kolory
*permread ,*
*permwrite ,*
*permexec ,*
*permspecial*
i
*permnone*
skórki dla r, w, x, s/t i -. Oba ustawienia mogą działać naraz; trójka
dotycząca użytkownika zachowuje wtedy kolor
*marked*.
Jeśli podświetlanie typów jest włączone, nazwy plików są kolorowane według
reguł opisanych w pliku {{sysconfdir}}/mcommander/filehighlight.ini. Więcej
podaje rozdział
[Podświetlanie nazw plików](#filenames-highlight).

**Szybkie wyszukiwanie i szybki filtr**

Można podać, jak mają działać
[szybkie wyszukiwanie](#quick-search)
i szybki filtr: bez rozróżniania wielkości liter, z rozróżnianiem, albo
zgodnie z porządkiem sortowania panelu, który też może je rozróżniać lub nie.

### Wygląd (skin) <a id="appearance"></a>

Wybór skóry nadającej programowi wygląd. Lista pokazuje skóry z katalogów
**{{pkgdatadir}}/skins**
i
**~/.local/share/mc6/skins**;
wybrana zaczyna obowiązywać od razu. Budowę skór opisuje dział
[Skins](mcommander.md#skins)
podręcznika angielskiego.

### Edycja rozszerzeń pliów (Edit Extension File) <a id="edit-extension-file"></a>

Ta komenda wywoła twój edytor na plik
*~/.config/mc6/extensions.ini.*
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.
PP

### Zapisz ustawienia (Save Setup) <a id="save-setup"></a>

Na starcie M-Commander będzie próbował odczytać opcje startowe
z pliku ~/.config/mc6/ini. Jeśli on nie istnieje, odczyta on konfiguracje z
ogólnodostępnego pliku {{pkgdatadir}}/mc.ini. Jeśli on też nie istnieje M-Commander
użyje swoich domyślnych ustawień.

Komenda
*Save Setup*
tworzy plik ~/.config/mc6/ini zachowując aktualne ustawienia lewego, prawego menu,
jak również menu opcji.

Jeśli właczysz opcję
*auto save setup,*
M-Commander zawsze będzie zachowywał standardowe ustawienie podczas wychodzenia.

Istnieją również ustawienia, które nie mogą być zmienione z poziomu menu.
Dla tych ustawień musisz wyedytować swój plik konfiguracyjny za pomocą
twojego ulubionego edytora. Zobacz sekcję Specjalne ustawienia po więcej
informacji.

<!-- help:break -->

# Wykonywanie poleceń systemu operacyjnego (Executing operating system commands) <a id="executing-operating-system-commands"></a>

Możesz wykonywać komendy wpisując je bezpośrednio do linii poleceń
M-Commandera, lub wybierając program, który chcesz wykonać za pomocą klawiszy
przemieszczenia i nacisnąć Enter.

Jeśli naciśniesz Enter na pliku, który nie jest wykonywalny, M-Commander
sprawdzi rozszerzenie pliku i porówna je z rozszerzeniami wybranymi w pliku
rozszerzeń (Extensions File). Jeśli jakaś pozycja się zgadza, wykonywana
jest komenda (raczej bardziej rozszerzone makro) powiązana z tym rozszerzeniem.

## Wbudowana komenda cd (The cd internal command) <a id="the-cd-internal-command"></a>

Komenda cd jest interpretowana przez M-Commandera, nie
dokładnie tak samo jak wykonuje to powłoka. Przez to rozkaz cd nie może zawierać
wielu składników makr, które są standardowo dostępne, jednak niektórych
potrafi używać:

*Tylda*
Znak tyldy (~) jest zawsze równoznaczny z wpisaniem nazwy katalogu domowego.
Jeśli po znaku tyldy dodasz jakiś login użytkownika, zostanie on zastąpiony
przez katalog domowy wybranego użytkownika.

Na przykład, ~guest jest katalogiem domowym użytkownika guest, podczas
kiedy ~/guest jest katalogiem guest w twoim katalogu domowym.

*Poprzedni katalog (Previous directory)*
Możesz przeskakiwać do katalogu, w którym byłeś poprzednio, używając specjalnej
nazwy katalogu '-' tak jak:
**cd -**

*katalogi CDPATH*
Jeśli katalog wybrany do przejścia nie jest w naszym aktualnym katalogu, to
M-Commander używa ścieżki w zmiennej
**CDPATH**
do szukania w jakimkolwiek z wymienionych tam katalogów.

Na przykład, możesz ustawić swoją zmienną
**CDPATH**
na katalogi ~/src:/usr/src, pozwalając na zmianę katalogów na jakikolwiek
inny wewnątrz ~/src i /usr/src, z miejsca w którym jesteś (np. cd linux
przeniesie cię do katalogu /usr/src/linux).

## Obsługa makr (Macro Substitution) <a id="macro-substitution"></a>

Kiedy korzystamy z
[menu użytkownika](#edit-menu-file),
wykonujemy
[polecenie zależne od rozszerzenia](#edit-extension-file)
albo polecenie z wiersza poleceń, wykonywane jest proste podstawianie makr.

Makra to:

*%i*
: Wcięcie z białych znaków, równe kolumnie kursora. Tylko w menu edytora.

*%y*
: Rodzaj składni bieżącego pliku. Tylko w menu edytora.

*%b*
: Nazwa pliku bloku.

*%e*
: Nazwa pliku błędów.

*%m*
: Nazwa bieżącego menu.

*%f* i *%p*
: W menu użytkownika menedżera plików: nazwa bieżącego pliku w aktywnym
panelu. W menu użytkownika mcedit6: nazwa otwartego pliku.

*%x*
: Rozszerzenie nazwy bieżącego pliku.

*%n*
: Nazwa bieżącego pliku bez rozszerzenia.

*%d*
: Nazwa bieżącego katalogu.

*%F*
: Bieżący plik w nieaktywnym panelu.

*%D*
: Nazwa katalogu nieaktywnego panelu.

*%t*
: Aktualnie zaznaczone pliki.

*%T*
: Pliki zaznaczone w nieaktywnym panelu.

*%v* i *%V*
: Jak %t i %T, ale podstawiane są pełne nazwy zaznaczonych plików.

*%u* i *%U*
: Jak %t i %T, z tym że pliki zostają odznaczone. Tego makra można użyć tylko
raz na pozycję pliku menu albo pliku rozszerzeń, bo następnym razem nie
będzie już zaznaczonych plików.

*%s* i *%S*
: Wybrane pliki: zaznaczone, jeśli jakieś są, a w przeciwnym razie bieżący
plik.

*%cd*
: To jest specjalne makro, które zmienia bieżący katalog na ten podany przed
nim. Używa się go przede wszystkim jako interfejsu do
[wirtualnych systemów plików](#virtual-file-system).

*%view*
: To makro uruchamia wbudowany podgląd. Może stać samo albo z argumentami.
Jeśli podajesz argumenty, trzeba je wziąć w nawiasy.

> Argumentami są:
> *ascii*
> aby wymusić podgląd w trybie ascii;
> *hex*
> aby wymusić podgląd w trybie szesnastkowym;
> *nroff*
> aby podgląd interpretował pogrubienie i podkreślenie programu nroff;
> *unformatted*
> aby podgląd nie interpretował poleceń nroff robiących tekst pogrubiony albo
> podkreślony;
> *structured*
> aby otworzyć plik w trybie strukturalnym (drzewo).

*%%*
: Znak %

*%{jakiś tekst}*
: Pyta o podstawienie. Pokazuje się okienko wejściowe, a tekst wewnątrz
klamer służy jako zachęta. Makro zastępowane jest tekstem wpisanym przez
użytkownika. Użytkownik może nacisnąć Esc albo F10, aby przerwać. To makro
nie działa jeszcze w wierszu poleceń.

*%var{ENV:wartość}*
: Jeśli zmienna środowiskowa
*ENV*
nie jest ustawiona, podstawiana jest
*wartość*.
Jeśli jest, podstawiana jest wartość
*ENV*.

## Terminal <a id="the-terminal"></a>

Program trzyma twoją powłokę w pseudoterminalu za panelami. Działa z
powłokami bash, ash (BusyBox i Debian), (o/m)ksh, tcsh, zsh i fish.

Powłoka to ta podana w zmiennej
**SHELL**,
a jeśli jej nie ma, ta z pliku /etc/passwd. Zamiast uruchamiać nową powłokę
przy każdym poleceniu, program przekazuje polecenie tej powłoce tak, jakbyś
sam je wpisał. Dzięki temu można zmieniać zmienne środowiskowe, korzystać z
funkcji powłoki i zakładać aliasy, które są ważne do wyjścia z programu.

**bash**
: polecenia startowe w ~/.local/share/mc6/bashrc (inaczej ~/.bashrc), własna
mapa klawiatury w ~/.local/share/mc6/inputrc (inaczej ~/.inputrc).

**ash/dash**
: (BusyBox albo Debian) polecenia startowe w ~/.local/share/mc6/ashrc
(inaczej ~/.profile).

**ksh/oksh**
: polecenia startowe w ~/.local/share/mc6/kshrc (inaczej
*ENV*
albo ~/.profile).

**mksh**
: (MirBSD ksh) polecenia startowe w ~/.local/share/mc6/mkshrc (inaczej
*ENV*
albo ~/.mkshrc).

**zsh**
: polecenia startowe w ~/.local/share/mc6/.zshrc (inaczej ~/.zshrc).

**tcsh, fish**
: na razie nie mają własnych plików startowych dla tego programu, działają
tylko pliki samej powłoki.

Działającą aplikację można w każdej chwili odłożyć skrótem
**C-o**
i wrócić do programu. Jeśli w ten sposób przerwałeś polecenie, nie
uruchomisz innego polecenia zewnętrznego, dopóki przerwana aplikacja się nie
skończy.

Za panelami terminal przechowuje wszystko, co powłoka wypisała, i dopóki
panele są schowane, można to czytać, zaznaczać i czyścić. Strzałki chodzą po
wyjściu, a te same z Shiftem je zaznaczają, jedno i drugie dopóki sam
terminal dostaje klawisze; klawisze, które tylko przesuwają widok, działają
niezależnie od tego, kto pisze. W wierszu poleceń Shift-Up i Shift-Down
przesuwają widok o wiersz, Ctrl-Up i Ctrl-Down przeglądają tam historię, a
pozostałe klawisze z Shiftem nic nie robią: powłoka nie ma zaznaczenia.
Każdy klawisz nie wymieniony poniżej trafia do powłoki.

```
PgUp, PgDn     przesuwa widok o stronę
Ctrl-Up        przesuwa widok o wiersz w górę
Ctrl-Down      przesuwa widok o wiersz w dół
Ctrl-Home      idzie do najstarszego wiersza
Ctrl-End       wraca do najnowszego wiersza
Ctrl-Insert    kopiuje zaznaczenie do schowka
Ctrl-Shift-u   zdejmuje zaznaczenie
Alt-s          szuka w wyjściu tego, co wpiszesz dalej
Alt-Shift-s    pokazuje tylko pasujące wiersze
Ctrl-l         czyści ekran, zachowując wyjście
Ctrl-Shift-l   czyści ekran i całe wyjście
               (również Ctrl-Alt-l)
```

Alt-s i Alt-Shift-s biorą wzorzec tak samo jak w panelach: pisze się go w
górnym wierszu ekranu, a wyjście podąża za nim, w miarę jak rośnie. Wielkość
liter nie ma znaczenia. Szukanie idzie w dół od kursora i zaznacza
najbliższe trafienie; kolejne Alt-s zaznacza to niżej, a za najnowszym
wierszem szukanie wraca do najstarszego. Tam, gdzie pisze powłoka, nic
jeszcze nie czytano, więc szukanie bierze wyjście od najstarszego wiersza.
Filtr pokazuje tylko pasujące wiersze, a strzałki chodzą po nich jeszcze w
trakcie pisania wzorca; kolejne Alt-Shift-s przenosi kursor o wiersz niżej.
Ustawienie
*search_direction*
odwraca oba, i szukanie idzie w górę, tak jak szuka
**less**
i tak jak było wcześniej. Naciśnięte bez wpisanego wzorca,
oba klawisze biorą poprzedni wzorzec. Backspace kasuje znak, a znak, do
którego nic nie pasuje, nie jest przyjmowany. Enter kończy pisanie i zostawia
widok na znalezionym, wraz z zaznaczeniem; Esc kończy je i przywraca widok
sprzed pisania: kursor tam, gdzie czytano, albo na znak zachęty, jeśli nic
nie czytano, oraz filtr i zaznaczenie, jakie były. Każdy inny klawisz kończy
pisanie i robi to, co robi.

Przy schowanych panelach większość klawiszy funkcyjnych należy do terminala,
a pasek przycisków je nazywa. Podglądu, edycji, kopiowania, przenoszenia i
kasowania z menedżera plików tam nie ma: działają one na pliku pod kursorem
panelu, a tego kursora nie widać. F8 jest celowo pusty, żeby odruch w stronę
kasowania nie zrobił czegoś innego.
F7 tworzy katalog, a Shift-F4 edytuje nowy plik, tak samo jak przy widocznych
panelach: oba działają w katalogu panelu, a w nim stoi powłoka.

```
F2           kopiuje zaznaczenie do schowka
F3           zaznacza całe wyjście albo zdejmuje zaznaczenie
F4           zostawia tylko wiersze pasujące do zaznaczenia
             albo do słowa pod kursorem
F5           zdejmuje ten filtr i zakłada go z powrotem
F6           czyści ekran i całe wyjście
```

Dopóki powłoka czeka przy swojej zachęcie, F1, F7, Shift-F4, F9 i F10 należą
do menedżera plików, a F1 otwiera pomoc o tym rozdziale. Gdy tylko polecenie
działa, ekran i wszystkie klawisze na nim należą do niego, te też. Pięć
powyższych to wyjątek: dopóki polecenie pracuje, zostają przy terminalu.
Aplikacja pełnoekranowa, edytor albo przeglądarka bierze sobie wszystkie
klawisze, także te. Wszystkie są wymienione w sekcji
**[mcterm]**
pliku przypisań klawiszy i tam można je zmienić.

Gdy powłoka za panelami kończy pracę, przez
**exit**
albo przez Ctrl-D, które sama odczytuje, M-Commander kończy pracę razem z
nią. W wierszu poleceń Ctrl-D usuwa znak, tak jak przy widocznych panelach;
polecenie, które czyta swoje wejście, na przykład
**cat**,
dostaje je jako koniec wejścia. W mksh i tcsh, które nie mają wiersza
poleceń M-Commandera, Ctrl-D w pustym wierszu kończy powłokę, a z nią
M-Commander.

Jeśli przy zachęcie powłoki, za schowanymi panelami, wpiszesz
**mcommander**
bez argumentów, działający program pokaże swoje panele z powrotem, zamiast
uruchamiać drugą kopię. Z argumentem, na przykład nazwą katalogu, uruchamia
się zagnieżdżony program, tak jak wcześniej.

Zwykła zachęta, którą pokazuje program, ma postać
"użytkownik@host:ścieżka$ ". Przy powłoce, która to potrafi, takiej jak Bash,
zachęta będzie ta sama, której używasz w powłoce.

(Znany problem z fish: zachęta widoczna jest tylko w trybie pełnoekranowym
(Ctrl-o), a nie przy widocznych panelach.)

Aby użyć powłoki innej niż ta ze zmiennej SHELL albo ta podana w
/etc/passwd, uruchom program tak:
**SHELL=/bin/mojapowloka mcommander**

Rozdział
[OPCJE](#options)
zawiera więcej informacji o sterowaniu powłoką.

# Chmod

Okno Chmod jest używane do zmieniania atrybutów grupy plików lub katalogów.
Może być ono wywołane kombinacją C-x c.

Okno Chmod ma dwie części -
*Uprawnienia (Permissions)*
i
*Plik (File)*

W sekcji Plik wyświetlana jest nazwa pliku lub katalogu i jego uprawnienia
w formie liczbowej jak również właściciel i grupa.

W sekcji Uprawnienia jest kilka przycisków, z których każdy odpowiada
za odpowiednie uprawnienie do pliku. Podczas zmieniania atrybutów, widzisz
jak zmienia się wartość liczbowa w oknie Plik.

Do poruszania pomiędzy okienkami (przyciskami i polami do zaznaczania) używaj
*strzałek*
lub klawisza
*tab.*
Aby zmienić pola lub wcisnąć przycisk używaj klawisza
*spacji.*
Możesz również używać "gorących liter" aby go wybrać
(są one podświetlonymi literami na przyciskach).

Aby uaktywnić wprowadzone zmiany wciśnij Enter.

Kiedy pracujesz z grupą plików, lub katalogów, możesz kliknąć na
bit, który chcesz wybrać lub wyczyścić. Kiedy już wybrałeś bity,
które chcesz zmienić, możesz wcisnąć jeden z przycisków aktywujących
*(Set marked*
lub
*Clear marked).*

I w końcu, aby wprowadzić dokładnie takie zmiany jak wybrałeś, użyj
przycisku
**[Set all]**,
który zadziała na wszystkich wybranych plikach.

**[Marked all]**
włącza tylko zaznaczone atrybuty do wybranych plików.

**[Set marked]**
włącza zaznaczone bity w atrybutach wszystkich wybranych plików.

**[Clean marked]**
czyści zaznaczone bity z atrybutów zaznaczonych plików.

**[Set]**
ustawia atrybuty jednego pliku.

**[Cancel]**
unieważnia komendę chmod.

# Chown

Komenda chown jest używana do zmiany właściela/grupy pliku. Skrótem
klawiszowym jest kombinacja C-x o.

# Zaawansowane chown (Advanced Chown) <a id="advanced-chown"></a>

Zaawansowane chown jest komendą łączącą w sobie komendy chmod i chown.
Możesz za jednym zamachem zmienić atrybuty i właściela/grupę pliku.

# Operacje na plikach (File Operations) <a id="file-operations"></a>

Kiedy kopiujesz, przenosisz lub kasujesz pliki, M-Commander pokazuje
okno opisowe operacji na pliku. Pokazuje nazwę pliku, na którym
aktualnie dokonuje się operacja. Widoczne są co najwyżej trzy linie postępu.
Pierwsza (file) mówi nam jak duża część pliku została już przekopiowana.
Druga (bytes) mówi jak duża część wszystkich zaznaczonych plików została
przekopiowana jak do tej pory. Trzecia (count) mówi jaka ilość plików
została już przekopiowana. Jeśli opcja verbose jest wyłączona, linia
file i bytes nie jest pokazywana.

Są dwa przyciski na dole okna dialogowego. Naciskając przycisk Skip
ominiemy resztę aktualnie "ruszanego" pliku. Naciskając przycisk Abort
zatrzymamy całą operację, pominiemy resztę plików.

Są trzy inne okna dialogowe, które mogą się włączyć podczas operacji
na plikach.

Okno błędów informuje nas o błędach zaistniałych podczas operacji
na pliku. Są w nim trzy możliwości wyboru. Przycisk Skip mówi żeby
pominąć wybrany plik, przycisk Abort żeby przerwać całą operacją,
a Retry aby ponowić próbę (np. kiedy usunąłeś problem korzystając
z innego terminala).

### Nadpisanie pliku <a id="replace"></a>

To okno pokazuje się, kiedy próbujesz przenieść albo skopiować plik na
miejsce już istniejącego. Okno pokazuje daty i rozmiary obu plików i daje
następujące przyciski:

**[Tak]**
: nadpisuje plik.

**[Nie]**
: pomija plik.

**[Dopisz]**
: dopisuje plik źródłowy na koniec docelowego.

**[Kontynuuj]**
: dopisuje do docelowego resztę pliku źródłowego. Ten przycisk widać tylko
wtedy, gdy rozmiar pliku docelowego nie jest zerowy i jest mniejszy od
źródłowego.

**[Wszystkie]**
: nadpisuje wszystkie pliki.

**[Nowsze]**
: nadpisuje, jeśli plik źródłowy jest nowszy od docelowego.

**[Żaden]**
: nie nadpisuje żadnego pliku.

**[Mniejsze]**
: nadpisuje, jeśli plik źródłowy jest mniejszy od docelowego.

**[Inny rozmiar]**
: nadpisuje pliki o różnym rozmiarze.

**[Przerwij]**
: przerywa całą operację.

Przy włączonym polu
**Nie nadpisuj plikiem o zerowej długości**
plik źródłowy o zerowym rozmiarze nie nadpisuje pliku docelowego, który taki
nie jest.

Okno rekurencyjnego kasowania pokazuje się, kiedy próbujesz skasować katalog,
który nie jest pusty. Jego przyciski to:

**[Tak]**
: kasuje katalog wraz z zawartością.

**[Nie]**
: pomija katalog.

**[Wszystkie]**
: kasuje wszystkie katalogi.

**[Żaden]**
: pomija wszystkie niepuste katalogi.

**[Przerwij]**
: przerywa całą operację.

Jeśli masz zaznaczone pliki i wykonujesz na nich operację, odznaczane są
tylko te, na których operacja się udała. Pliki pominięte i te, na których
operacja się nie powiodła, zostają zaznaczone.

# Maski kopiowania/przenoszenia (Mask Copy/Rename) <a id="mask-copyrename"></a>

Operacje przenoszenia i kopiowania pozwalają ci na tłumaczenie nazw
plików w łatwy sposób. Aby to zrobić, musisz wybrać odpowiednią maskę
źródłową i najczęściej w nazwie docelowej użyć gwiazdek.
Wszystkie pliki pasujące do maski źródłowej są kopiowane/przenoszone
w zgodzie z maską docelową. Jeśli są jakieś pliki zaznaczone, tylko one są
brane pod uwagę przy wybieraniu plików.

Są jeszcze inne opcje, które możesz ustawić:

Opcja
*Follow links*
mówi czy dowiązania i dowiązania twarde w katalogu źródłowym powinny być przenoszone
jako dowiązania czy też powinna być przegrywana ich zawartość (plik, na
który wskazują).

Opcja
*Dive into subdirs ...*
mówi co program ma robić, kiedy kopiuje się katalog, a taki już istnieje.
Standardowo kopiuje się pliki do wewnątrz już istniejącego katalogu (dodaje),
po włączeniu tej opcji kopiuje się katalog źródłowy do wnętrza tego katalogu.
Może przykład pomoże:

Chcesz przekopiować zawartość katalogu foo do /bla/foo, które
już istnieje. Normalnie (Dive nie jest włączone), mcommander skopiuje to dokładnie
do /bla/foo. Po włączeniu tej opcji zawartość zostanie skopiowana
do /bla/foo/foo ponieważ ten katalog już istnieje.

Opcja
*Preserve attributes*
mówi czy zachowywać oryginalne atrybuty pliku, czasy i jeśli jesteś
rootem to nawet numery UID i GID. Jeśli ta opcja jest wyłączona
używana jest aktualna wartość zmiennej umask.

**Use shell patterns on**

Jeśli opcja obsługi wzorców powłoki jest włączona, możesz używać znaków '\*' i
'?' w maskach źródłowych. Działają one tak jak w powłoce. W masce docelowej możesz
używać tylko '\*' i '\\\<cyfra>'. Pierwsza maska '\*' w nazwie docelowej
odnosi sie do pierwszej gwiazdki w masce źródłowej, druga do drugiej itd.
Joker '\\1' odnosi się do pierwszego jokera w masce źródłowej, '\\2' odnosi
się do drugiego i tak dalej aż do '\\9'. Joker '\\0' oznacza pełną nazwę
pliku źródłowego.

Dwa przykłady:

Jeśli maska źródłowa jest "\*.tar.gz", a miejscem docelowym jest "/bla/\*.tgz"
i plikiem, który ma zostać przekopiowany jest "foo.tar.gz", to kopią będzie
"foo.tgz" w katalogu "/bla".

Załóżmy, że chcesz zaminieć miejscami nazwę i rozszerzenie pliku, tak, że
plik "plik.c" ma być zmieniony na "c.plik" itp. Maska źródłowa powinna być
następująca: "\*.\*", natomiast docelowa: "\\2.\\1".

**Use shell patterns off**

Kiedy wzorce powłoki są wyłączone, M-Commander nie dokonuje automatycznego grupowania
plików. Musisz użyć wyrażenia'\\(...\\)' w masce źródłowej aby zasygnalizować
istnienie jokerów w masce docelowej. Jest to trochę łatwiejsze, ale też
wymaga aby trochę się napisać. Z drugiej jednak strony, makra są bardzo
podobne tych używanych kiedy wzorce powłoki są włączone.

Dwa przykłady:

Jeśli maską źródłową jest "^\\(.\*\\)\\.tar\\.gz$", docelową jest
"/bla/\*.tgz"
i plikiem do przekopiowania jest "foo.tar.gz", kopią będzie "/bla/foo.tgz".

Załóżmy, że chemy zamienić miejscami nazwę i rozszerzenia, tak, że plik
"plik.c" będzie się nazywał "c.plik" itp. Maską źródłową powinno być
"^\\(.\*\\)\\.\\(.\*\\)$", a docelową "\\2.\\1".

**Konwersje nazwy (Case Conversions)**

Możesz również zmieniać nazwy plików. Jeśli użyjesz '\\u' lub
'\\l' w masce docelowej, następny znak będzie przekonwertowany na
duży lub mały, zależnie od podanej opcji.

Jeśli użyjesz '\\U' lub '\\L' w masce docelowej, następne znaki będą
zmieniane na małe lub duże (zależnie od opcji), aż do napotkania znaku
'\\E' lub następnych '\\U', '\\L' bądź też końca linii.

Konwersje '\\u' i '\\l' mają wyższy priorytet niż '\\U' i '\\L'.

Na przykład, jeśli maską źródłową jest '\*' (shell patterns on) lub '^\\(.\*\\)$'
(shell patterns off) i maską docelową jest '\\L\\u\*', nazwa pliku będzie
miała pierwszą literę dużą, ale pozostałe już małe, niezależnie od obecnej
nazwy.

Możesz również używać '\\' aby "podkreślić" znak. Na przykład, '\\\\' jest
backsleshem, a '\\\*' jest gwiazdką.

# Dokańczanie <a id="completion"></a>

Pozwól M-Commanderowi pisać za ciebie.

Spróbuj użyć dokończenia na tekście przed aktualną pozycją. M-Commander próbuje
dokończyć tekst jako zmienną (jeśli tekst zaczyna się od znaku
**$**),
nazwę użytkownika (jeśli tekst zaczyna się od znaku
**~**),
nazwę hosta (jeśli tekst zaczyna się od znaku
**@**)
lub komendę (jeśli jesteś w linii komend w pozycji gdzie możesz wpisać
jakąś komendę, możliwe dokończenia będą zawierać również zarezerwowane
słowa i wbudowane komendy powłoki). Jeśli żaden z powyższych warunków nie
jest spełniony, próbuje się dokańczać nazwę pliku.

Nazwa pliku, nazwa użytkownika i hosta, pracuje we wszystkich liniach
wejścia, dokańczanie komend pracuje tylko w wybranych. Jeśli dokańczanie
jest rozbudowane (jest więcej różnych możliwości), M-Commander wyda krótki dźwięk, a
następna akcja będzie zależeć od wartości zmiennej
*Complete: show all*
w menu konfiguracja. Jeśli jest ona włączona, zostanie wyświetlona lista
wszystkich możliwych nazw. Właściwą nazwę możesz wybrać za pomocą strzałek
a potem naciskając klawisz
**Enter**
na właściwej pozycji. Możesz także nacisnąć pierwsze litery, którymi różnią
się możliwości aby odrzucić tak dużą część dokończeń jak to tylko możliwe.
Jeśli naciśniesz znowu
**M-Tab**,
pokazane zostaną tylko te pozycje, które zaczynają się od kolejnych
podanych liter. Kiedy nie maja już więcej możliwości, okno znika, ale
możesz je wcześniej schować używając klawiszy anulujących:
**Esc**,
**F10**
oraz strzałek w lewo i prawo. Jeśli
Complete: show all
jest wyłączone, okno z listą włącza się dopiero wtedy, kiedy naciskasz
**M-Tab**
po raz drugi. Za pierwszym razem M-Commander wydaje tylko krótki dźwięk.

# Wirtualny system plików (Virtual File System) <a id="virtual-file-system"></a>

M-Commander zawiera warstwę kodu do dostępu do systemu plików; warstwa ta
nazywa się przełącznikiem wirtualnych systemów plików. Pozwala ona pracować na
plikach, które nie leżą w uniksowym systemie plików.

Oprócz systemu
*local,*
czyli zwykłego systemu plików Uniksa, w program wbudowane są dwa wirtualne:
*extfs,*
który za pomocą własnego skryptu pokazuje plik lub listę systemową jako drzewo
katalogów, oraz
*sfs,*
który przepuszcza pojedynczy plik przez polecenie i pokazuje to, co z niego
wychodzi. Wszystko, co wymaga połączenia z inną maszyną, a także archiwa, to
teraz
[wtyczki paneli](#panel-plugins),
a nie systemy plików przełącznika.

Przełącznik interpretuje wszystkie używane ścieżki i przekazuje je właściwemu
systemowi plików; postać nazwy dla każdego z nich opisuje jego własny dział.

## Wtyczki paneli <a id="panel-plugins"></a>

Panel nie jest związany z systemem plików: wtyczka może go wypełnić wszystkim,
co potrafi wyliczyć. Wraz z programem dostarczane są

```
arcmc        archiwa i ich zawartość
ftp, sftp    pliki na innej maszynie
shell-link   pliki na innej maszynie przez ssh
samba        zasoby serwera SMB
s3           kubełki magazynu S3
git          stan repozytorium
docker       kontenery, obrazy i ich dzienniki
k8s          obiekty klastra
mongo        kolekcje bazy danych
sqlite       tabele bazy danych
systemd      jednostki systemu
panelize     wynik polecenia jako panel
mcpeek       zajrzenie do wnętrza pliku
mcstruct     plik binarny jako drzewo nazwanych pól
skineditor   wygląd programu
```

Każda wtyczka niesie własną pomoc, którą
**F1**
otwiera w jej panelu lub oknie. Pozycja
**Zarządzanie wtyczkami**
menu Opcje wylicza to, co jest wczytane, wyłącza wtyczkę i otwiera jej
ustawienia. Panel wtyczki otwiera się z
[menu lewego i prawego](#left-and-right-menus),
z listy katalogów albo przez wpisanie adresu wtyczki w linii poleceń.

## EXTernal File System

**extfs**
allows to integrate numerous features and file types into
M-Commander in an easy way, by writing scripts.

Extfs filesystems can be divided into two categories:

1\. Stand-alone filesystems, which are not associated with any existing
file.  They represent certain system-wide data as a directory tree.
You can invoke them by typing
*'cd fsname://'*
where fsname is an extfs short name (see below).  Examples of such
filesystems include audio (list audio tracks on the CD) or apt (list of
all Debian packages in the system).

For example, to list CD-Audio tracks on your CD-ROM drive, type

```
  cd audio://
```

2\. 'Archive' filesystems (like rpm, patchfs and more), which represent
contents of a file as a directory tree.  It can consist of 'real' files
compressed in an archive (urar, rpm) or virtual files, like messages
in a mailbox (mailfs) or parts of a patch (patchfs).  To access such
filesystems
*'fsname://'*
should be appended to the archive name.  Note that the archive itself
can be on another vfs.

For example, to list contents of a zip archive documents.zip type

```
  cd documents.zip/uzip://
```

In many aspects, you could treat extfs like any other directory.  For
instance, you can add it to the hotlist or change to it from directory
history.  An important limitation is that you cannot invoke shell
commands inside extfs, just like any other non-local VFS.

Common extfs scripts included with M-Commander are:

**a**
: access 'A:' DOS/Windows diskette
*(cd a://).*

**apt**
: front end to Debian's APT package management system
*(cd apt://).*

**audio**
: audio CD ripping and playing
*(cd audio://*
or
*cd device/audio://).*

**deb**
: package of Debian GNU/Linux distribution
*(cd file.deb/deb://).*

**dpkg**
: Debian GNU/Linux installed packages
*(cd deb://).*

**hp48**
: view and copy files to/from a HP48 calculator
*(cd hp48://).*

**lslR**
: browsing of lslR listings as found on many FTPs
*(cd filename/lslR://).*

**mailfs**
: mbox-style mailbox files support
*(cd mailbox/mailfs://).*

**patchfs**
: extfs to handle unified and context diffs
*(cd filename/patchfs://).*

**rpm**
: RPM package
*(cd filename/rpm://).*

**rpms**
: RPM database management
*(cd rpms://).*

**ulha, urar, uzip, uzoo, uar, uha**
: archivers
*(cd archive/xxxx://*
where xxxx is one of:
*ulha,*
*urar,*
*uzip,*
*uzoo,*
*uar,*
*uha).*

You could bind file type/extension to specified extfs as described in the
[Edit Extension File](#edit-extension-file)
section.  Here is an example entry for Debian packages:

```
  regex/\.deb$
          Open=%cd %p/deb://
```

## System plików jednego pliku <a id="single-file-filesystem"></a>

**sfs**
przepuszcza jeden plik przez polecenie i pokazuje wynik jako osobny plik; w
ten sposób czyta się plik skompresowany bez ręcznego rozpakowywania. Nazwa
systemu plików dopisywana jest do nazwy pliku, tak jak w extfs:

```
  cd dokumenty.gz/ugz://
```

Polecenia wymienia plik
**{{sysconfdir}}/mcommander/sfs.ini**,
po jednym w wierszu: nazwa systemu plików, ukośnik, numer polecenia,
tabulator i samo polecenie, gdzie
*%1*
to plik, na którym stoi panel, a
*%3*
plik, do którego należy pisać. Dostarczony plik zawiera pary pakujące i
rozpakowujące gz, bz2, lz, lz4, lzma, lzo, xz i zst, oraz kilka innych.

# Polskie znaki

M-Commander bardzo dobrze radzi sobie z obsługą znaków
nieamerykańskich (160+) w tym polskich. Ważne jest aby mieć ustawione
polskie znaki na konsoli (tzn. aby powłoka je obsługiwała). Jeśli używasz
basha musisz tylko ustawić w pliku inputrc ( /etc/inputrc lub ~/.inputrc)
następujące wartości:

```
set meta-flag on
set convert-meta off
set output-meta on
```

w pliku /etc/sysconfig/i18n:

```
SYSFONT=lat2u-16
SYSFONTACM=iso02
```

natomiast w pliku /etc/sysconfig keyboard:

```
KEYTABLE=pl
```

Potem użyj poleceń
*/sbin/setsysfont*
i
*loadkeys pl.*
[Zwróć uwagę na to, że te pliki są charakterystyczne dla dystrybucji
RedHat, jeśli masz inną i wiesz jak to ustawić, to napisz do mnie, a ja to
tu dopiszę  [ patrz tłumacz na dole ;)) ]].

I gotowe - polskie literki działają również w podglądzie i wbudowanym
edytorze plików.

# Atrybuty pliku <a id="chattr"></a>

To okno służy do zmiany atrybutów grupy plików i katalogów w linuksowym
systemie plików. Otwiera je C-x e.

Nie każdy system plików zna wszystkie atrybuty. Lista dostępnych atrybutów
pokazana jest jako zestaw pól wyboru odpowiadających znacznikom atrybutów
(szczegóły podaje
**chattr(1)**).
W miarę zmiany pól zmienia się wraz z nimi wartość symboliczna pod nazwą
pliku.

Po elementach okna porusza się
*strzałkami*
albo klawiszem
*Tab*.
Stan pola wyboru zmienia, a przycisk wybiera
**spacja**.

Atrybuty zapisuje Enter.

Przy pracy na grupie plików albo katalogów wystarczy zaznaczyć te atrybuty,
które chcesz włączyć albo skasować, a potem wybrać jeden z przycisków
działania (Ustaw zaznaczone albo Wyczyść zaznaczone).

**[Ustaw wszystkie]**
: ustawia dokładnie podane atrybuty na wszystkich zaznaczonych plikach.

**[Zaznacz wszystkie]**
: ustawia tylko zaznaczone atrybuty na wszystkich wybranych plikach.

**[Ustaw zaznaczone]**
: włącza zaznaczone znaczniki w atrybutach wybranych plików.

**[Wyczyść zaznaczone]**
: wyłącza zaznaczone znaczniki w atrybutach wybranych plików.

**[Ustaw]**
: ustawia atrybuty jednego pliku.

**[Anuluj]**
: wychodzi z polecenia.

# Lista ekranów <a id="screen-selector"></a>

Program potrafi mieć uruchomionych naraz kilka części wewnętrznych (edytor,
podgląd, porównanie) i przechodzić między nimi bez zamykania otwartych
plików. Kilku menedżerów plików naraz na razie nie ma.

Nazwijmy ekranem każdą z tych części. Są trzy sposoby przechodzenia między
ekranami, tymi globalnymi skrótami:

**Alt-}**
: przechodzi do następnego ekranu;

**Alt-{**
: przechodzi do poprzedniego ekranu;

**Alt-\`**
: otwiera okno z listą otwartych ekranów (albo pozycja menu "Lista ekranów").

# Zaznaczanie plików <a id="selectunselect-files"></a>

Klawisze
**+**
i
`\`
proszą o wzorzec i zaznaczają lub odznaczają pliki, które do niego pasują, a
**\***
odwraca zaznaczenie. Okno pamięta ostatni wzorzec i pozwala określić, czy ma
być wzorcem powłoki, czy ma rozróżniać wielkość liter i czy ma dotyczyć także
katalogów.

# Tryby panelu <a id="panel-modes"></a>

Tryb panelu to nazwany układ listy, którego można używać wielokrotnie. Lista
trybów jest wspólna dla obu paneli.

**Alt-t**
(oraz pozycja
**Tryby panelu...**
menu lewego i prawego) otwiera
**przełącznik:**
listę zdefiniowanych trybów. Enter stosuje do panelu tryb, na którym stoi
kursor, Esc zostawia panel bez zmian.

Pozycja
**Tryby panelu plików...**
menu
**Opcje**
otwiera
**menedżera:**
tę samą listę, edytowaną klawiszami.
**Insert**
tworzy nowy tryb,
**F4**
(lub
**Enter**)
edytuje wybrany,
**F5**
go powiela, a
**Delete**
(lub
**F8**)
usuwa. Przycisk
**Domyślne**
zastępuje listę trybami wbudowanymi,
**Ok**
ją zapisuje, a
**Anuluj**
(lub
**Esc**)
odrzuca wszystko, co zrobiono w oknie.

Menedżer edytuje wspólną listę trybów; nie zmienia trybu żadnego panelu.

Edytor trybu ma osobne pola na typy pól kolumn i ich szerokości oraz na
wiersz mini-statusu, z nazwami pól opisanymi w rozdziale
[Układ listy...](#listing-format)
\. Lista typów rozdzielana jest przecinkami, po jednej pozycji na kolumnę;
kolumna może zawierać kilka pól rozdzielonych spacjami (na przykład
**type name**).
Szerokość 0 (albo pusta) zostawia polu jego automatyczną szerokość.
W pole typów można też wkleić pełny łańcuch układu (na przykład
**half name | size:7**):
rozdzielacze
**|**
i przyrostki
**:szerokość**
rozkładają się wtedy na dwie listy.

Zdefiniowane tryby i ten wybrany przez każdy panel są zachowywane między
sesjami.

# Krótki przegląd wyrażeń regularnych <a id="regex-quick-reference"></a>

**Zwykłe elementy**

```
Jeden ze znaków: a, b albo c            [abc]
Znak inny niż a, b albo c               [^abc]
Znak z zakresu a-z                      [a-z]
Znak spoza zakresu a-z                  [^a-z]
Znak z a-z albo A-Z                     [a-zA-Z]
Dowolny znak                            .
Alternatywa: a albo b                   a|b
Dowolny biały znak                      \s
Wszystko poza białym znakiem            \S
Dowolna cyfra                           \d
Wszystko poza cyfrą                     \D
Znak słowa                              \w
Wszystko poza znakiem słowa             \W
Grupa bez przechwytywania               (?:...)
Grupa przechwytująca                    (...)
Zero albo jedno a                       a?
Zero albo więcej a                      a*
Jedno albo więcej a                     a+
Dokładnie 3 a                           a{3}
3 a albo więcej                         a{3,}
Od 3 do 6 a                             a{3,6}
Początek napisu                         ^
Koniec napisu                           $
Granica słowa                           \b
Poza granicą słowa                      \B
```

**Kotwice**

```
Początek trafienia                      \G
Początek napisu                         ^
Koniec napisu                           $
Początek napisu                         \A
Koniec napisu                           \Z
Bezwzględny koniec napisu               \z
Granica słowa                           \b
Poza granicą słowa                      \B
```

**Elementy ogólne**

```
Koniec wiersza                          \n
Powrót karetki                          \r
Tabulacja                               \t
Znak zerowy                             \0
```

**Metaciągi**

```
Dowolny znak                            .
Alternatywa: a albo b                   a|b
Dowolny biały znak                      \s
Wszystko poza białym znakiem            \S
Dowolna cyfra                           \d
Wszystko poza cyfrą                     \D
Znak słowa                              \w
Wszystko poza znakiem słowa             \W
Ciąg Unicode, z końcami wierszy         \X
Końce wierszy Unicode                   \R
Wszystko poza końcem wiersza            \N
Pionowy biały znak                      \v
Zaprzeczenie \v                         \V
Poziomy biały znak                      \h
Zaprzeczenie \h                         \H
Zerowanie trafienia                     \K
Podwzorzec numer #                      \#
Własność Unicode X                      \pX
Własność Unicode albo kategoria pisma   \p{...}
Zaprzeczenie \pX                        \PX
Zaprzeczenie \p{...}                    \P{...}
Cytat: bierz dosłownie                  \Q...\E
Podwzorzec 'nazwa'                      \k{name}
Podwzorzec 'nazwa'                      \k<name>
Podwzorzec 'nazwa'                      \k'name'
n-ty podwzorzec                         \gn
n-ty podwzorzec                         \g{n}
n-ty wcześniejszy podwzorzec wzgl.      \g{-n}
Wyrażenie n-tej grupy                   \g<n>
Wyrażenie n-tej kolejnej grupy          \g<+n>
Wyrażenie n-tej grupy                   \g'n'
Wyrażenie n-tego kolejnego podwzorca    \g'+n'
Nazwana grupa przechwytująca            \g{letter}
Wyrażenie nazwanej grupy                \g<letter>
Wyrażenie nazwanej grupy                \g'letter'
Znak szesnastkowy YY                    \xYY
Znak szesnastkowy YYYY                  \x{YYYY}
Znak ósemkowy ddd                       \ddd
Znak sterujący Y                        \cY
Znak backspace                          [\b]
Czyni każdy znak dosłownym              \
```

**Kwantyfikatory**

```
Zero albo jedno a                       a?
Zero albo więcej a                      a*
Jedno albo więcej a                     a+
Dokładnie 3 a                           a{3}
3 a albo więcej                         a{3,}
Od 3 do 6 a                             a{3,6}
Kwantyfikator zachłanny                 a*
Kwantyfikator leniwy                    a*?
Kwantyfikator zaborczy                  a*+
```

**Klasy znaków**

```
Jeden ze znaków: a, b albo c            [abc]
Znak inny niż a, b albo c               [^abc]
Znak z zakresu a-z                      [a-z]
Znak spoza zakresu a-z                  [^a-z]
Znak z a-z albo A-Z                     [a-zA-Z]
Litery i cyfry                          [[:alnum:]]
Litery                                  [[:alpha:]]
Kody ASCII 0-127                        [[:ascii:]]
Tylko spacja albo tabulacja             [[:blank:]]
Znaki sterujące                         [[:cntrl:]]
Cyfry dziesiętne                        [[:digit:]]
Znaki widoczne (bez spacji)             [[:graph:]]
Małe litery                             [[:lower:]]
Znaki widoczne                          [[:print:]]
Widoczne znaki przestankowe             [[:punct:]]
Białe znaki                             [[:space:]]
Duże litery                             [[:upper:]]
Znaki słowa                             [[:word:]]
Cyfry szesnastkowe                      [[:xdigit:]]
Początek słowa                          [[:<:]]
Koniec słowa                            [[:>:]]
```

**Flagi i modyfikatory**

```
Wielowierszowo                          m
Bez rozróżniania wielkości liter        i
Pomijaj białe znaki / rozwlekle         x
Jeden wiersz                            s
Unicode                                 u
eXtra                                   X
Niezachłannie                           U
Zakotwiczenie                           A
Powtórzone nazwy grup                   J
Grupy bez przechwytywania               n
Pomijaj wszystkie białe / rozwlekle     xx
```

**Konstrukcje grup**

```
Grupa bez przechwytywania               (?:...)
Grupa przechwytująca                    (...)
Grupa atomowa (bez przechwytywania)     (?>...)
Zerowanie numeru podwzorca              (?|...)
Grupa komentarza                        (?#...)
Nazwana grupa przechwytująca            (?'name'...)
Nazwana grupa przechwytująca            (?<name>...)
Nazwana grupa przechwytująca            (?P<name>...)
Modyfikatory w wierszu                  (?imsxUJnxx)
Miejscowe modyfikatory w wierszu        (?imsxUJnxx:...)
Konstrukcja warunkowa                   (?(1)yes|no)
Konstrukcja warunkowa                   (?(R)yes|no)
Rekurencyjna konstrukcja warunkowa      (?(R#)yes|no)
Konstrukcja warunkowa                   (?(R&name)yes|no)
Warunek z wyprzedzeniem                 (?(?=...)yes|no)
Warunek wstecz                          (?(?<=...)yes|no)
Rekurencja całego wzorca                (?R)
Wyrażenie grupy 1                       (?1)
Pierwsza względna grupa                 (?+1)
Wyrażenie nazwanej grupy                (?&name)
Podwzorzec 'nazwa'                      (?P=name)
Wyrażenie grupy '{nazwa}'               (?P>name)
Wzorce zdefiniowane przed użyciem       (?(DEFINE)...)
Dodatnie spojrzenie w przód             (?=...)
Ujemne spojrzenie w przód               (?!...)
Dodatnie spojrzenie wstecz              (?<=...)
Ujemne spojrzenie wstecz                (?<!...)
Literowe asercje spojrzenia             (*pla:...)
Nieatomowa asercja spojrzenia           (*non_atomic_positive_lookahead:...)
Asercja jednolitego pisma               (*script_run:...)
Jednolite pismo (skrót)                 (*sr:...)
Czasownik sterujący                     (*ACCEPT)
Czasownik sterujący                     (*FAIL)
Czasownik sterujący                     (*MARK:NAME)
Czasownik sterujący                     (*COMMIT)
Czasownik sterujący                     (*PRUNE)
Czasownik sterujący                     (*SKIP)
Czasownik sterujący                     (*THEN)
```

# Kolory <a id="colors"></a>

M-Commander próbuje sprawdzić czy twój terminal obsługuje
kolory używając bazy danych terminali. Czasami jest to zmieniane
przez różne flagi startowe, np. możesz wymusić wyświetlanie czarno-białe
lub kolorowe startując z opcją odpowiednio -b i -c.

Jeśli program jest skompilowany z menedżerem ekranu S-Lang zamiast ncurses,
sprawdzi on również wartość zmiennej
**COLORTERM**.
Jeśli jest ustawiona, ma takie samo znaczenie jak opcja -c.

Możesz wybrać terminale, które zawsze żądają wyświetlania w kolorze,
poprzez dodanie ich do pozycji
*color_terminals*
w sekcji pliku startującego. Uchroni to M-Commandera przed próbami
odkrycia typu twojego terminala. Na przykład

```
[Colors]
color_terminals=linux,xterm
```

```
color_terminals=terminal-name1,terminal-name2...
```

Program może być skompilowany zarówno z bibliotekami S-Lang jak i ncurses.
Ncurses nie obsługuje metody wymuszania wyświetlania, zawsze sprawdza w bazie danych
terminali.

# Skórki <a id="skins"></a>

Wygląd programu można zmienić. Trzeba w tym celu podać plik, który zawiera
opis kolorów i linii do rysowania ramek. Nowe ustalenie kolorów jest w pełni
zgodne z przypisaniem opisanym w rozdziale
[Kolory](#colors).

Jeśli skórka zawiera definicje pełnego koloru (true-color), w sekcji [skin]
należy ustawić klucz 'truecolors' na TRUE. Jeśli używa nie pełnego koloru, a
256 kolorów, zamiast tego klucz '256colors'.

Plik skórki szukany jest w następującej kolejności (do pierwszego
znalezionego):

```
1) opcja wiersza poleceń -S <skórka>, --skin=<skórka>
2) zmienna środowiskowa MC_SKIN
3) parametr skin sekcji [Midnight-Commander]
4) plik {{sysconfdir}}/mcommander/skins/default.ini
5) plik {{pkgdatadir}}/skins/default.ini
```

Opcja wiersza poleceń, zmienna środowiskowa i parametr w pliku konfiguracji
mogą zawierać bezwzględną ścieżkę do pliku skórki (z rozszerzeniem .ini albo
bez). Szukanie odbywa się w (do pierwszego znalezionego):

```
1) ~/.local/share/mc6/skins/
2) {{sysconfdir}}/mcommander/skins/
3) {{pkgdatadir}}/skins/
```

Format plików skórek opisuje
**{{pkgdatadir}}/skins/README.txt**.

# Podświetlanie nazw plików <a id="filenames-highlight"></a>

Sekcja [filehighlight] bieżącego pliku skórki zawiera jako klucze nazwy grup
podświetlania, a jako wartości pary kolorów.

Reguły podświetlania nazw stoją w pliku {{pkgdatadir}}/filehighlight.ini
(~/.config/mc6/filehighlight.ini). Nazwa sekcji w tym pliku musi być taka
sama jak nazwa parametru w sekcji [filehighlight] (bieżącego pliku skórki).

Klucze tych grup to:

*type*
: typ pliku. Jeśli jest podany, pozostałe opcje są pomijane.

*regexp*
: wyrażenie regularne. Jeśli jest podane, opcja 'extensions' jest pomijana.

*extensions*
: lista rozszerzeń plików, rozdzielona znakiem ';'.

*extensions_case*
: (ma sens tylko z parametrem 'extensions') sprawia, że reguła 'extensions'
rozróżnia wielkość liter (true) albo nie (false).

Klucz 'type' może mieć wartości:

```
- FILE (wszystkie pliki)
  - FILE_EXE
- DIR (wszystkie katalogi)
  - LINK_DIR
- LINK (wszystkie dowiązania poza zerwanymi)
  - HARDLINK
  - SYMLINK
- STALE_LINK
- DEVICE (wszystkie pliki urządzeń)
  - DEVICE_BLOCK
  - DEVICE_CHAR
- SPECIAL (wszystkie pliki specjalne)
  - SPECIAL_SOCKET
  - SPECIAL_FIFO
  - SPECIAL_DOOR
```

# Parametry zewnętrznego edytora lub podglądu <a id="parameters-for-external-editor-or-viewer"></a>

Program pozwala podać opcje dla zewnętrznych edytorów i podglądów. Sekcja
"[External editor or viewer parameters]" szukana jest najpierw w systemowym
pliku startowym (pliku defaults.ini w katalogu programu), a potem w pliku
~/.config/mc6/ini. Nazwą opcji powinna być nazwa (pełna ścieżka)
zewnętrznego edytora lub podglądu. Wartość może zawierać zmienne:

*%filename*
: nazwa pliku do edycji albo podglądu.

*%lineno*
: wiersz, na którym plik ma się otworzyć.

Na przykład:

```
[External editor or viewer parameters]
    vi=%filename +%lineno
    joe=%filename +%lineno
    more=%filename +%lineno
```

Wiersz początkowy przekazywany jest zewnętrznemu edytorowi albo podglądowi
tylko wtedy, gdy uruchamia się go z okna wyników
[szukania plików](#find-file).

Jeśli zewnętrzny edytor albo podgląd uruchamia się klawiszem F4 lub F3,
program liczy na to, że tamten program (przynajmniej "joe", ale pewnie i
inne) sam otworzy plik tam, gdzie był ostatnio. Program nie przeszkadza
zewnętrznemu edytorowi ani podglądowi w zapisywaniu i odtwarzaniu pozycji w
otwartych plikach.

# Specjalne ustawienia <a id="special-settings"></a>

Większość ustawień można zmienić z menu. Jest jednak niewielka liczba takich,
które zmienia się tylko przez edycję pliku konfiguracji.

Te zmienne można ustawić w pliku ~/.config/mc6/ini:

*clear_before_exec*
: Domyślnie program czyści ekran przed wykonaniem polecenia. Jeśli wolisz
widzieć wyjście polecenia na dole ekranu, zmień w pliku ~/.config/mc6/ini
wartość pola clear_before_exec na 0.

*confirm_view_dir*
: Jeśli naciskasz F3 na katalogu, program zwykle do niego wchodzi. Jeśli ta
wartość to 1, przy zaznaczonych plikach zapyta o potwierdzenie przed zmianą
katalogu.

*vfs_timeout*
: Czas życia pamięci podręcznej wirtualnego systemu plików w sekundach. Po
wyjściu z archiwum albo pliku skompresowanego wczytana lista i rozpakowany
plik tymczasowy zostają przez ten czas, żeby ponowne wejście było
natychmiastowe, a potem są zwalniane. Domyślnie 60; 0 zwalnia je od razu.

*only_leading_plus_minus*
: Traktuje znaki '+', '-' i '\*' w wierszu poleceń osobno (zaznaczanie,
odznaczanie, odwracanie zaznaczenia) tylko wtedy, gdy wiersz poleceń jest
pusty. Dzięki temu nie trzeba ich cytować w środku wiersza, ale przy
niepustym wierszu nie zmienią zaznaczenia.

*alternate_plus_minus*
: Przy włączonej opcji klawisze '+', '-', '\\' i '\*' działają zwyczajnie. Do
zaznaczania i odznaczania służą wtedy 'Alt-+', 'Alt--' i 'Alt-\*'.

*show_output_starts_shell*
: Kiedy klawiszem C-o wracasz na ekran użytkownika, a ta opcja jest włączona,
dostajesz nową powłokę. Inaczej dowolny klawisz przywraca program.

*timeformat_recent*
: Postać daty i czasu dla dat nie starszych niż sześć miesięcy. Opis formatu
podaje strona podręcznika strftime albo date. Bez tej opcji obowiązuje postać
domyślna.

*timeformat_old*
: Postać daty i czasu dla dat starszych niż sześć miesięcy albo przyszłych.
Opis formatu podaje strona podręcznika strftime albo date. Bez tej opcji
obowiązuje postać domyślna.

*use_file_to_guess_type*
: Przy włączonej opcji (domyślnie) program wywołuje polecenie file, aby
rozpoznać typy wymienione w
[pliku extensions.ini](#edit-extension-file).

*xtree_mode*
: Przy włączonej opcji (domyślnie wyłączona), kiedy przeglądasz system plików
w panelu drzewa, drugi panel sam pokazuje zawartość wybranego katalogu.

*shell_directory_timeout*
: Czas życia pozycji pamięci podręcznej katalogów w sekundach. Wartość
domyślna to 900 sekund.

*clipboard_store*
: Ścieżka (wraz z opcjami) do zewnętrznego programu obsługi schowka, takiego
jak 'xclip', który czyta tekst z pliku do zaznaczenia X. Na przykład:

<!-- -->

```
clipboard_store=xclip -i
```

*clipboard_paste*
: Ścieżka (wraz z opcjami) do zewnętrznego programu obsługi schowka, takiego
jak 'xclip', który wypisuje zaznaczenie na standardowe wyjście. Na przykład:

<!-- -->

```
clipboard_paste=xclip -o
```

*autodetect_codeset*
: Ta opcja pozwala użyć polecenia 'enca' do samodzielnego rozpoznania strony
kodowej plików tekstowych we wbudowanym podglądzie i edytorze. Listę
poprawnych wartości daje polecenie 'enca --list languages | cut -d : -f1'.
Opcja musi stać w sekcji [Misc].

Na przykład:

```
autodetect_codeset=russian
```

Ustawienia wbudowanego podglądu plików stoją w sekcji [Viewer] tego samego
pliku. Wszystkie są też w oknie
[Opcje przeglądarki](mview.md#viewer-options);
nazwy tutaj to te, które to okno zapisuje.

*wrap*
: Zawija wiersz szerszy od ekranu do następnego wiersza ekranu. Domyślnie
włączone.

*syntax*
: Koloruje tekst regułami składni edytora. Domyślnie wyłączone.

*mouse_move_pages*
: Przewijanie myszą idzie stronami, a nie wiersz po wierszu. W trybie ASCII
lewy przycisk zaznacza tekst, więc tam to przewijanie robi się prawym albo
środkowym przyciskiem. Domyślnie włączone.

*remember_file_position*
: Otwiera plik w tym miejscu, w którym ostatnio go zostawiono. Domyślnie
wyłączone.

*structured_auto*
: Otwiera obsługiwane pliki (json, yaml, yml, xml, html, htm) od razu w
trybie strukturalnym (drzewo). Jeśli pliku nie da się przeanalizować, bez
słowa używany jest zwykły widok tekstowy. Domyślnie wyłączone.

*eof*
: Tekst wypisywany po ostatnim wierszu pliku. Domyślnie pusty.

*structured_max_size*
: Największy plik, który widok strukturalny analizuje, w bajtach. Większy
jest odrzucany przed czytaniem. Domyślnie 67108864 (64 MB).

*structured_max_nodes*
: Największe drzewo, które widok strukturalny buduje, liczone w węzłach.
Gęsty dokument, taki jak XML z drobnymi znacznikami, dochodzi do tej granicy
wcześniej niż do granicy rozmiaru: zużywa około jednego węzła na dwanaście
bajtów, a każdy węzeł kosztuje pamięć. Domyślnie 10000000, co mieści około
120 MB takiego pliku w mniej więcej 1,5 GB.

*dirt_limit*
: Ile odświeżeń ekranu można najwyżej pominąć, dopóki plik jest czytany. Ta
wartość zwykle nie ma znaczenia, bo program sam dobiera liczbę pominiętych
odświeżeń do tempa nadchodzących klawiszy. Na bardzo wolnych komputerach albo
na terminalach z szybkim powtarzaniem klawiszy duża wartość sprawia, że ekran
skacze. Domyślnie 10, co zachowuje się najlepiej.

Starsze wersje trzymały te ustawienia w głównej sekcji pod dłuższymi nazwami
(wrap_mode, viewer_syntax_highlighting, mouse_move_pages_viewer,
mcview_remember_file_position, mcview_structured_auto, mcview_eof i
max_dirt_limit). Są one stamtąd czytane raz i zapisywane do sekcji [Viewer].

Ustawienia terminala, w którym za panelami pracuje powłoka, stoją w sekcji
[Terminal] tego samego pliku. Nie ma okna, które by je zapisywało.

*search_direction*
: W którą stronę
**Alt-s**
przegląda wyjście powłoki i w którą
**Alt-Shift-s**
przechodzi z jednego wiersza filtru do następnego: "down" idzie od kursora ku
najnowszemu wierszowi, a za nim wraca do najstarszego, "up" idzie ku
najstarszemu wierszowi i wraca do najnowszego, tak jak szuka
**less**
i tak jak było wcześniej. Domyślnie "down".

*clipboard_write*
: Czy program w terminalu może umieścić tekst w schowku sekwencją OSC 52, tak
jak robią to vim, tmux albo sesja ssh. Tekst trafia tam, gdzie kopia z
edytora: do pliku schowka i do polecenia z
*clipboard_store*.
Program nigdy nie może w ten sposób odczytać schowka. Domyślnie "false":
każde wyjście w terminalu, także plik wypisany przez
**cat**,
mogłoby zmienić schowek.

# Baza danych terminali (Terminal databases) <a id="terminal-databases"></a>

M-Commander pozwala ci na naprawienie bazy danych terminali bez
posiadania uprawnień roota. M-Commander szuka w pliku startowym
(defaults.ini położonego w katalogach z bibliotekami M-Commandera) lub w
pliku ~/.config/mc6/ini sekcji "terminal:nazwa-twojego-terminala" i potem sekcji
"terminal:general", każda linia sekcji zawiera symbol klawisza, który
chcesz zdefiniować, zaczynające się do znaku równości i definicji klawisza.
Możesz użyć kombinacji \\E aby reprezentować znak escape i ^x aby
reprezentować znak Control-x.

Możliwymi klawiszami symboli są:

```
f0 do f20     Klawisze funkcyjne f0-f20
bs            backspace
home          klawisz home
end           klawisz end
up            strzałka w górę
down          strzałka w dół
left          strzałka w lewo
right         strzałka w prawo
pgdn          klawisz page down
pgup          klawisz page up
insert        znak insert
delete        znak delete
complete      do dokańczania
```

Na przykład, aby zdefiniować klawisz insert jako Escape + [ + O + p, możesz
ustawić to pliku ini:

```
insert=\E[Op
```

Symbol klawisza
*complete*
reprezentuje sekwencję wyjścia używaną do wywoływania procesu dokańczania,
jest to wywoływane kombinacją M-tab, ale możesz zdefiniować inne klawisze
do wykonywania tych samych funkcji (na tych klawiaturach z toną fajnych i
zupełnie bezużytecznych klawiszy).

<!-- help:break -->

# ZMIENNE ŚRODOWISKOWE <a id="environment"></a>

Poniżej wymieniono zmienne, które M-Commander czyta, oraz te, które ustawia
dla uruchamianych przez siebie programów. Zmienne takie jak **TERM**,
**SHELL**, **HOME** czy **PATH** nie są tu wymienione: program czyta je po to,
by rozpoznać środowisko, a nie po to, by się nimi konfigurować.

## Czytane przy uruchomieniu <a id="read-at-start-up"></a>

**MC_DATADIR**
: Katalog, z którego brane są pliki danych, zamiast wbudowanego. Zobacz
[PLIKI](#files).

**MC_PROFILE_ROOT**
: Korzeń plików użytkownika, jako ścieżka bezwzględna. Zobacz [PLIKI](#files).

**MC_SKIN**
: Skórka, nazwa albo ścieżka. Zobacz [Skórki](#skins).

**MC_KEYMAP**
: Plik przypisań klawiszy. Zobacz [Klawisze](#keys).

**MC_TMPDIR**
: Katalog plików tymczasowych programu.

**MC_NO_LUA**
: Wartość 1 uruchamia program bez środowiska Lua. Żaden pakiet Lua nie jest
wczytywany i nic, co go potrzebuje, nie jest dostępne.

**MC_SIXEL**
: Wartość 0 oznacza, że terminal nie ma grafiki sixel, wartość 1 - że ma. Bez
tej zmiennej pytany jest sam terminal.

**KEYBOARD_KEY_TIMEOUT_US**
: Jak długo czekać na resztę sekwencji sterującej, w mikrosekundach.

**COLORTERM**
: Czytana przy wyborze kolorów. Zobacz [Kolory](#colors).

**CDPATH**
: Katalogi, w których szuka wbudowane polecenie cd.

**EDITOR**, **VIEWER**, **PAGER**
: Programy zewnętrzne używane, gdy wbudowany edytor lub podgląd są wyłączone.
Zobacz
[Parametry zewnętrznego edytora lub podglądu](#parameters-for-external-editor-or-viewer).

## Ustawiane dla uruchamianych programów <a id="set-for-the-programs-m-commander-starts"></a>

Nie są przeznaczone do ustawiania ręcznie. Program zapisuje je po to, by jego
własna kopia uruchomiona z wbudowanego terminala rozpoznała, że działa już
wewnątrz niego.

**MC_SID**
: Sesja, w której działa program. Kopia uruchomiona z tej sesji nie otwiera
własnych paneli.

**MC_PID**
: Identyfikator procesu działającego programu.

**MC_TTY**
: Terminal, na którym program został uruchomiony.

## Dzienniki diagnostyczne <a id="debug-logs"></a>

Dziennik jest zapisywany tylko wtedy, gdy jest włączony, a przełącznik
przyjmuje wartość 1. Zmienne wtyczek odwołują się do ogólnych, więc ustawienie
samej pary ogólnej zapisuje wszystko.

**MC_LOG_ENABLE**, **MC_LOG_FILE**
: Dziennik ogólny. Bez **MC_LOG_FILE** używany jest plik wskazany przez
*logfile* w sekcji *[Logging]* pliku *ini*, a bez tego wpisu *mc.log* obok
pozostałych plików użytkownika.

**MC_FTP_LOG_ENABLE**, **MC_FTP_LOG_FILE**
: Dziennik wtyczki panelu ftp. Plik domyślnie */tmp/mc-ftp.log*.

**MC_SMB_LOG_ENABLE**, **MC_SMB_LOG_FILE**
: Dziennik wtyczki panelu samba. Plik domyślnie */tmp/mc-samba.log*.

**MC_SPELL_LOG**
: Plik, do którego pisze sprawdzanie pisowni. Nie ma własnego przełącznika:
dziennik powstaje, gdy zmienna wskazuje plik.

Aby zachować dziennik nieudanego połączenia ftp:

```
MC_FTP_LOG_ENABLE=1 MC_FTP_LOG_FILE=/tmp/ftp.log mcommander
```

# PLIKI <a id="files"></a>

Program będzie pobierał wszystkie swoje informacje ze zmiennej
**MC_DATADIR**,
jeśli jest ona nie ustawiona to znowu przetwarzany jest katalog /usr.

 {{pkgdatadir}}/help/mcommander.md
: Plik pomocy dla programu.

 {{pkgdatadir}}/extensions.ini
: Standardowy plik rozszerzeń plików.

~/.config/mc6/extensions.ini
: Własny plik użytkownika, konfiguruje podgląd i edycje plików. Ma wyższy
priorytet niż plik systemowy.

 {{pkgdatadir}}/mc.ini
: Standardowy plik setupu do M-Commandera, używany tylko wówczas,
kiedy użytkownik nie ma swojego własnego pliku ~/.config/mc6/ini.

 {{pkgdatadir}}/defaults.ini
: Globalne ustawienia M-Commandera. Ustawienia w tym pliku są
uwzględniane przez wszystkie sesje M-Commandera, użyteczne do
definiowania ogólnosystemowych ustawień terminali.

~/.config/mc6/ini
: Własny setup użytkownika. Jeśli ten plik jest dostępny, jest ładowany
zamiast pliku globalnego.

 {{pkgdatadir}}/hints/hint
: Plik zawierający podpowiedzi (hints) wyświetlane przez program.

 {{pkgdatadir}}/usermenu
: Ten plik zawiera informacje o ogólnosystemowych aplikacjach w menu.

~/.config/mc6/menu
: Własny plik menu użytkownika. Jeśli ten plik jest obecny jest używany
zamiast pliku globalnego.

\~\~/.cache/mc6/tree
: Lista katalogów drzewa katalogów i podglądu drzewa. Jedna linia jest jednym
wejściem. Linie zaczynające się od ukośnika są pełnymi nazwami katalogów.
Linie zaczynające się od numeru mają tyle znaków ile poprzedni katalog.
Jeśli chcesz możesz stworzyć plik używając komendy "find / -type d
-print | sort > ~/.cache/mc6/tree". Normalnie nie ma sensu tego czynić, gdyż
M-Commander robi to sam za ciebie.

\./.usermenu
: Lokalny plik zdefiniowany przez użytkownika. Jeśli ten plik jest dostępny,
jest używany zamiast pliku w katalogu domowym i ogólnosystemowego.

To change default home directory of M-Commander, you can use
**MC_PROFILE_ROOT**
environment variable. The value of MC_PROFILE_ROOT must be an absolute path.
If MC_PROFILE_ROOT is unset or empty, HOME variable is used. If HOME is unset
or empty, M-Commander directories are get from GLib library.

# LICENCJA <!-- help:skip -->

Program jest dystrybuowany na zasadach licencji GNU General Public License
dopóki jako publikowany przez Free Software Foundation. Zobacz wbudowaną
pomoc po więcej szczegółów na temat licencji i braku gwarancji.

# DOSTĘPNOŚĆ <a id="availability"></a>

Najnowsza wersja programu jest do zdobycia na serwerze ftp.nuclecu.unam.mc w
katalogu /linux/local i w Europie na serwerze sunsite.mff.cuni.cz w katalogu
/GNU/mc i na serwerze ftp.teuto.de w katalogu /lmb/mc.

# ZOBACZ TAKŻE <a id="see-also"></a>

ed(1), gpm(1), terminfo(1), view(1), sh(1), bash(1),
tcsh(1), zsh(1).

```
Strona M-Commander w sieci World Wide Web:
	https://github.com/blue-panels/mcommander
```

# AUTORZY <a id="authors"></a>

Miguel de Icaza (miguel@roxanne.nuclecu.unam.mx), Janne Kukonlehto
(jtklehto@paju.oulu.fi), Radek Doulik (rodo@ucw.cz), Fred
Leeflang (fredl@nebula.ow.org), Dugan Porter (dugan@b011.eunet.es),
Jakub Jelinek (jj@sunsite.mff.cuni.cz), Ching Hui
(mr854307@cs.nthu.edu.tw), Andrej Borsenkow (borsenkow.msk@sni.de),
Norbert Warmuth (nwarmuth@privat.circular.de),
Mauricio Plaza (mok@roxanne.nuclecu.unam.mx), Paul Sheer
(psheer@icon.co.za) and Pavel Machek (pavel@ucw.cz) are the developers
of this package;
Alessandro Rubini (rubini@ipvvis.unipv.it) has been especially helpful
debugging and enhancing the program's mouse support, John Davis
(davis@space.mit.edu) also made his S-Lang library available to us
under the GPL and answered my questions about it, and the following
people have contributed code and many bug fixes (in alphabetical
order):

Adam Tla/lka (atlka@sunrise.pg.gda.pl),
alex@bcs.zp.ua (Alex I. Tkachenko), Antonio Palama,
DOS port (palama@posso.dm.unipi.it), Erwin van Eijk
(wabbit@corner.iaf.nl), Gerd Knorr (kraxel@cs.tu-berlin.de),
Jean-Daniel Luiset (luiset@cih.hcuge.ch), Jon Stevens
(root@dolphin.csudh.edu), Juan Francisco Grigera, Win32 port
(j-grigera@usa.net), Juan Jose Ciarlante (jjciarla@raiz.uncu.edu.ar),
Ilya Rybkin (rybkin@rouge.phys.lsu.edu), Marcelo Roccasalva
(mfroccas@raiz.uncu.edu.ar), Massimo Fontanelli (MC8737@mclink.it),
Pavel Roskin (pavel_roskin@geocities.com),
Sergey Ya. Korshunoff (seyko2@gmail.com), Thomas Pundt
(pundtt@math.uni-muenster.de), Timur Bakeyev
(timur@goff.comtat.kazan.su), Tomasz Cholewo
(tjchol01@mecca.spd.louisville.edu), Torben Fjerdingstad
(torben.fjerdingstad@uni-c.dk), Vadim Sinolitis (vvs@nsrd.npi.msu.su)
and Wim Osterholt (wim@djo.wtm.tudelft.nl).

# BŁĘDY <a id="bugs"></a>

Jeśli chcesz zgłosić kłopoty z programem [błędy w nim],
zgłoś go [po angielsku] pod adresem
<https://github.com/blue-panels/mcommander/issues> .

Do zgłoszenia błędu dołącz opis problemu, versję programu, którego używasz
(wyświetla ją mcommander -V), system operacyjny, na którym pracujesz i jeśli program
się wykłada, chcielibyśmy dostać ślad stosu.

# TŁUMACZENIE

Maciej Wojciechowski    wojciech@staszic.waw.pl
