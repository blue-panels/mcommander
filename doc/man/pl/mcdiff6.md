---
date: wrzesień 2026
---

<!-- help:topics "Spis treści:" -->
# NAZWA <!-- help:skip -->

mcdiff6 - Wbudowany podgląd różnic.

# UŻYTKOWANIE <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] plik1 plik2

# OPIS

mcdiff6 to dowiązanie do
**mcommander**,
głównego programu menedżera plików. Uruchomiony pod tą nazwą otwiera
wbudowany podgląd różnic, który porównuje
*plik1*
z
*plik2*,
podanymi w wierszu poleceń.

# Wbudowany podgląd różnic <a id="diff-viewer"></a>

mcdiff6 to narzędzie do poglądowego porównywania. Dwa pliki można porównać i od
razu edytować, a różnica jest liczona na nowo po każdej zmianie. Otwiera go
też wtyczka panelu git, z plikiem takim, jaki widzi go HEAD, po jednej
stronie i plikiem z katalogu roboczego po drugiej.

We wbudowanym podglądzie różnic dostępne są następujące klawisze:

**F1**
: Wywołuje wbudowaną przeglądarkę pomocy.

**F2**
: Zapisuje zmienione pliki.

**F4**
: Edytuje plik z lewego panelu we wbudowanym edytorze.

**F14**
: Edytuje plik z prawego panelu we wbudowanym edytorze.

**F5**
: Przenosi bieżącą różnicę do pliku po prawej. Przenoszona jest tylko bieżąca
różnica, po czym porównanie liczone jest na nowo.

**F15**
: Przenosi bieżącą różnicę w drugą stronę, do pliku po lewej.

**F7**
: Zaczyna szukanie.

**F17**
: Szuka dalej.

**F9**
: Otwiera
[opcje porównania](#diff-options).

**Alt-e**
: Wybiera zestaw znaków, w którym czytane są oba pliki.

**F10, Esc, q, Q**
: Wychodzi z podglądu różnic.

**Alt-s, s**
: Przełącza pokazywanie stanu różnic.

**Alt-n, l**
: Przełącza pokazywanie numerów wierszy.

**Ctrl-s**
: Przełącza podświetlanie składni. Tekst każdego wiersza jest kolorowany
regułami składni, tak samo jak we wbudowanym edytorze, a stan wiersza zostaje
przy tle i kolumnie znaczników. Tam, gdzie skórka odróżnia zmienione słowo od
reszty wiersza samym kolorem tekstu, słowo jest zamiast tego podkreślane.
Ustawienie jest zapamiętywane osobno od tego samego ustawienia edytora.

**f**
: Powiększa lewy panel na maksimum.

**=**
: Wyrównuje szerokość paneli.

**>**
: Zwęża prawy panel.

**<**
: Zwęża lewy panel.

**2, 3, 4, 8**
: Ustawia szerokość tabulacji.

**C-u**
: Zamienia zawartość paneli.

**C-r**
: Czyta oba pliki na nowo i liczy różnicę od początku.

**C-o**
: Pokazuje ekran poleceń.

**Enter, spacja, n**
: Idzie do następnej różnicy.

**Backspace, p**
: Idzie do poprzedniej różnicy.

**g, G**
: Idzie do wskazanego wiersza.

**Down**
: Przewija o wiersz w przód.

**Up**
: Przewija o wiersz wstecz.

**PageUp**
: Przewija o stronę wstecz.

**PageDown**
: Przewija o stronę w przód.

**Left, Right**
: Przesuwają tekst o kolumnę w bok.

**C-Left, C-Right**
: Przesuwają tekst o osiem kolumn w bok.

**Home**
: Wraca do pierwszej kolumny.

**C-Home**
: Idzie na początek pliku.

**C-End**
: Idzie na koniec pliku.

# Opcje porównania <a id="diff-options"></a>

Ustawienia
[podglądu różnic](#diff-viewer),
które otwiera tam
**F9**,
a w menedżerze plików pozycja
**Opcje podglądu różnic**
menu Opcje. Podgląd bierze je przy starcie, więc porównanie już otwarte
zostaje przy tych, z którymi je otwarto.

*Algorytm porównania.*
Zwykły porównuje pliki takimi, jakie są. Najszybszy zakłada duże pliki i
zadowala się zgrubnym wynikiem. Najmniejszy poświęca więcej czasu, aby
znaleźć mniejszy zbiór zmian.

*Pomijaj wielkość liter.*
Duże i małe litery liczą się jako ten sam znak.

*Pomijaj rozwijanie tabulacji.*
Wiersze różniące się tylko tym, czy to samo wcięcie zapisano tabulacjami czy
spacjami, liczą się jako równe.

*Pomijaj zmiany odstępów.*
Ciąg białych znaków jest równoważny każdemu innemu ciągowi białych znaków.

*Pomijaj wszystkie białe znaki.*
Białe znaki nie biorą udziału w porównaniu.

*Usuwaj znak powrotu karetki.*
Zdejmuje znak powrotu karetki z końca wiersza, dzięki czemu plik z końcami
wierszy DOS porównuje się z plikiem z końcami uniksowymi.

# LICENCJA <!-- help:skip -->

Ten program jest rozpowszechniany na warunkach Powszechnej Licencji
Publicznej GNU opublikowanej przez Free Software Foundation. Szczegóły o
licencji i o braku gwarancji podaje wbudowana pomoc.

# DOSTĘPNOŚĆ

Najnowsza wersja tego programu znajduje się pod adresem
<https://github.com/blue-panels/mcommander/releases> .

# ZOBACZ TAKŻE

mcommander(1), mview(1), mcedit6(1), diff(1).

# BŁĘDY

Błędy należy zgłaszać pod adresem
<https://github.com/blue-panels/mcommander/issues> .
