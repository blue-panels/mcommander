---
date: 2026. szeptember
---

<!-- help:topics "Tartalomjegyzék" -->
# NÉV <!-- help:skip -->

mcommander - kétpaneles, szöveges módú fájlkezelő

# ALKALMAZÁSA <!-- help:skip -->

**mcommander**
[-abcCdfhPstuUVx] [-l log] [dir1 [dir2]] [-v file]

# LEÍRÁS <a id="description"></a>

Az M-Commander kétpaneles, szöveges módú fájlkezelő, amely a GNU Midnight
Commanderre épül. Architektúrájának alapja egy tömör mag és a dinamikusan
betöltődő panelbővítmények. A bővítmények egységes panelfelületet adnak
archívumokhoz, távoli fájlrendszerekhez, verziókezelt tárolókhoz és egyéb
adatforrásokhoz. A parancsok beépített terminálban futnak. Az M-Commander
tartalmaz továbbá szintaxiskiemelő szövegszerkesztőt és szöveges, valamint
bináris formátumokat kezelő megjelenítőt.


# OPCIÓK <a id="options"></a>

*-a*
A kereteket és vonalakat egyszerűsített karakterekkel rajzolja ki

*-b*
: Fekete-fehér megjelenítés kérése

*-c*
: Engedélyezi a színes megjelenítést; nézd meg a
[Színek](#colors)
részt további információkért.

*-d*
: Nem engedélyezi az egér használatát.

*-f*
: Megjeleníti a M-Commander fájlainak elérési útvonalát, ahogy azt
a fordításnál beállítottuk.

*-k*
: Törli azon gyorsbillentyűket, amelyek alapértelmezésben a termcap/terminfo
adatbázisból töltődnek be. Csak HP terminálokon érdemes használni,
ahol a funkció billentyűk nem működnek.

*-l fájl*
: Fájlba menti a szerverrel lebonyolított ftpfs dialógus adatait.

*-P*
: A program befejezésekor a M-Commander kiírja az utolsó
munkakönyvtárat, ez nem használható közvetlenül, csak olyan különleges
shell funkcióval, amely lehetővé teszi az aktuális shell könyvtár
helyett a M-Commander által utoljára meglátogatott könyvtárra való
átváltást (köszönet a funkcióért és a funkcióhoz szükséges kódért Torben
Fjerdingstad-nek és Sergey-nek közreműködésükért). Kérlek, ne csinálj
szó szerinti másolatot a funkció beállításairól. A fájlok forrása a
*{{pkglibexecdir}}/mc6.sh*
(bash és zsh felhasználóknak),
*{{pkglibexecdir}}/mc6.csh*
(tcsh felhasználóknak) illetőleg a
*{{pkglibexecdir}}/mc6.fish*
(fish felhasználóknak) fájl. Ilyenkor, amikor a funkció beállításokat
változtatod, a profil értékeket nem szükséges megváltoztatnod, csak
arról gondoskodj, hogy az M-Commander-t ne fordítsd eltérő beállításokkal.

A bash és zsh funkciók lehetnének rövidebbek is, de a bash környezete
nem fogadja el a program C-z háttérbe helyezését. A temp fájlok a $TMPDIR
(alapértelmezés szerint /tmp) alatt egy csak általad írható mcommander-XXXXXX
könyvtárba kerülnek, mert ez biztonságosabb, mint a közös írható /tmp
könyvtár.

*-s*
: Bekapcsolja a lassú terminál módot, ebben a módban a program nem használja
a sok energiát felemésztő vonal karaktereket és az un. bővített módot
kikapcsolja.

*-t*
: Ezt csak akkor használd, ha S-Lang-gel és terminfo-val fordítottad a
programot: a
**TERMCAP**
váltózó értékét használja, és nem a rendszer szintű terminál adatbázist.

*-u*
: Nem engedélyezi a konkurrens shell-ek használatát (csak akkor használható,
ha a M-Commandert a "concurrent shell" támogatással fordították).

*-U*
: Engedélyezi a konkurens shell támogatást (csak akkor használható ha
a M-Commander fordításakor beállították a subshell támogatást,
mint választható lehetőséget).

*-v fájl*
: Belép a belső fájlnézőbe a kiválasztott fájl megtekintéséhez.

*-V*
: Megmutatja a program verziószámát.

*-x*
: Belép xterm módba. (Két képernyős módban használható, és az egér escape
szekvenciái is használhatóak).

*-X, --no-x11*
: Do not use X11 to get the state of modifiers Alt, Ctrl, Shift

*-g, --oldmouse*
: Force a "normal tracking" mouse mode. Used when running on
xterm-capable terminals (tmux/screen).

Ha megadtad, akkor az első útvonal tartalma jelenik meg az aktuális
panelen; a második könyvtár útvonal pedig a másik panelen jelenik meg.

# Áttekintés <a id="overview"></a>

A M-Commander képernyőjének négy része van.  Csaknem az egész
képernyőt a két könyvtár panelre tölti ki.  Alapértelmezésben a képernyőn
alulról a második sor a parancssor, a legalsó sor pedig a funkció gombok
elnevezéseit jeleníti meg. A legfelső sor a
[Menüsor](#menu-bar)
A menüsor esetleg nem látható, de könnyen megjeleníthető úgy, hogy a
felső sorra kattintasz az egérrel, vagy lenyomod az F9-et.

A M-Commander lehetővé teszi, hogy egyszerre két panelt
láthassunk. Az egyik a panelek közül az aktív "current" panel
(a kiválasztó sáv az aktív panelen található). Majdnem minden
művelet a jelenlegi panelben történik. Néhány fájlművelet, úgy, mint
átnevezés-áthelyezés és másolás alapértelmezésben a kiválasztatlan panelt
használja rendeltetési helyként (ne aggódj, végrehajtás előtt erre mindíg
rákérdez a megerősítés műveletnél). További információkért nézd meg a
[Könyvtár panelek](#directory-panels),
a
[Bal és Jobb oldali menük](#left-and-right-menus)
és a
[Fájl menü](#file-menu)
részt.

Futtathatsz rendszer parancsot is a M-Commander-ből, annak egyszerű
begépelésével. A megjelenő shell parancssorba mindíg begépelheted a
parancsot és az Enter lenyomásakor a M-Commander lefuttatja azt;
olvasd el a
[Shell parancssor](#shell-command-line)
és a
[Beviteli gombok](#input-line-keys)
részt, hogy többet is megtudhass a parancssorról.

# Egér kezelés <a id="mouse-support"></a>

A M-Commander eredendően tartalmazza az egér támogatást. Ez
aktiválódik, ha
**xterm(1)**
terminálon futtatod (akkor is működik, amikor telnet, vagy rlogin
kapcsolatban vagy egy másik géppel az xterm-ből), vagy, ha Linux konzolon
használod, és a
**gpm**
egér szerver fut.

Amikor bal gombbal kattintasz a fájlra, a könyvtár panalben a fájl
kiválasztódik; ha a jobb gombbal kattintasz, a fájlt ezzel megjelölöd
(vagy megszünteted azt, az azt megelőző állapotnak megfelelően).

A fájlra történő dupla kattintásra az M-Commander megpróbálja futtani
azt, ha futtatható fájlról van szó; ha a
[fájl kiterjesztését](#edit-extension-file)
egy adott programhoz már hozzá rendelted, a fájl kiterjesztéséhez
hozzárendelt program lefut.

Továbbá rájuk kattintva láthatóvá teszi a parancs futtatásához megadott
funkció billentyű elnevezéseket is.

Ha az egérrel a könyvtár panel legfelső sorára kattintunk, az egy
oldalnyit lapozik visszafelé. Ennek megfelelően az alsó sorra kattintva
egy oldalnyit ugrasz előre. Ez az eszköze használható a
[Súgó néző](#contents)
és a
[Könyvtárfa](#directory-tree)
esetén is.

Az egérgomb automatikus ismétlésének határértéke alapesetben 400
ezredmásodpercnyi. Ez megváltoztatható az
[~/.config/mc6/ini](#save-setup)
fájlban a
*mouse_repeat_rate*
paraméter értékének megváltoztatásával.

Ha a Commander-t egér támogatással indítottad az eredeti egér
tulajdonságok (szöveg kivágás és beillesztés) a Shift gomb lenyomásával
érhetők el.

# Billentyűzet <a id="keys"></a>

Néhány M-Commander parancshoz szükséges a
*Control (~vezérlő)*
(ezeket CTRL-lal vagy CTL-lel jelöljük) és a
*Meta (~Váltó)*
(ezeket ALT-tal vagy néha Compose-zal jelöljük) gombok használata. Ebben
a leírásban a következő rövidítéseket használjuk:

C-\<kar> ilyenkor lenyomva kell tartanod a Control billentyűt addíg, amíg
a megadott karaktert \<kar> le nem ütöd. Így például a C-f esetén: tartsd
lenyomva a Control billentyűt, amíg az f-et begépeled.

M-\<kar> ilyenkor lenyomva kell tartanod a Meta, vagy az Alt billentyűt
addíg, amíg a megfelelő karaktert \<kar> be nem gépeled. Ha ez nem a
Meta, vagy az Alt billentyű, akkor használd az ESC-et, a megfelelő
karakter \<kar> begépelésekor. A Meta funkció Linux alatt úgy érhető el,
hogy megnyomjuk, majd elengedjük az ESC billentyűt. A Meta funkció az
ezután megnyomott billentyűre vonatkozik!

beviteli eszköze a GNU Emacs szerkesztő billentyűzet-kombinációihoz
hasonlóan működik.

Több részben is beszélünk majd ezekről a gombokról. Az itt következők a
legfontosabbak ezek közül.

A
[Fájl menü](#file-menu)
rész tartalmazza a Fájl menü parancsainak billentyűzet gyorskapcsolóit.
Ez a rész tartalmazza még a funkció billentyűket is. Ezen parancsok
jobbára valamilyen műveletet végeznek el, általában a kiválasztott
fájlon, vagy a kijelölt fájlokon.

A
[Könyvtár panelek](#directory-panels)
rész tartamazza azokat a billentyűket, amelyek a későbbi műveletekhez
kiválasztják, vagy kijelölik a fájlokat (a művelet általában a Fájl
menüben megtalálható).

A
[Shell Parancssor](#shell-command-line)
felsorolja azokat a gombokat, amelyeket használhatsz a begépeléshez és a
parancssor szerkesztéshez. Ezek átmásolják a fájlnevet a könyvtár
panelből a parancssorba (a túlságosan sok gépelést elkerülendő), vagy
hozzáférést enged a parancssor előzményeihez.

[Beviteli gombok](#input-line-keys)
a beviteli sorok szerkesztésére szolgálnak. Ezen eszközök a
parancssorban és lekérdező dialógus (query dialog) beviteli soraihoz
szükségesek.

## A billentyűk átállítása <a id="keys_redefine"></a>

Ugyanez magában a programban is elvégezhető, a
**Beállítások**
menüből. A
[Billentyűtársítások](#key-bindings)
párbeszédablak felsorolja az összes műveletet a hozzájuk tartozó
billentyűkkel, átállítja őket, és az eredményt a
**~/.config/mc6/keymap.ini**
fájlba írja, vagyis abba, amelyet a beállítás keres. A
[Billentyűzet tanítás](#learn-keys)
a kérdés másik felével foglalkozik: megtanítja a programnak azokat a
sorozatokat, amelyeket a terminál a rosszul felismert billentyűkre küld. A
[Billentyűfigyelő](#key-sniffer)
megmutatja, mi érkezik egy billentyű leütésekor, és azt a műveletet, amelyhez
az adott billentyű az aktuális kiosztásban tartozik; ezt érdemes megnézni, ha
egy társítás nem látszik működni.

A billentyűtársítások külső fájlból is beolvashatók. A program először a
forráskódban megadott kiosztásból építi fel őket. Ezután mindig betöltődik a
**{{pkgdatadir}}/keymap.ini**
és a
**{{sysconfdir}}/mcommander/keymap.ini**
fájl, ebben a sorrendben felülírva a korábbi társításokat.
A csomag a saját kiosztásait a
**{{sysconfdir}}/mcommander**
könyvtárba teszi:
**keymap.default.ini**,
**keymap.emacs.ini**
és
**keymap.vim.ini**,
ahol a
**keymap.ini**
az alapértelmezettre mutató link.
A
**--nokeymap**
kapcsoló egyik fájlt sem olvassa be, és a forráskódbeli társításokat hagyja
érvényben.

A felhasználó saját kiosztásfájlját a program a következő sorrendben keresi
(az elsőig, amelyet megtalál):

```
1) parancssori kapcsoló -K <kiosztás>, --keymap=<kiosztás>
2) MC_KEYMAP környezeti változó
3) a [Midnight-Commander] szakasz keymap paramétere
4) a ~/.config/mc6/keymap.ini fájl
```

Az első három név vagy abszolút útvonal lehet. Ahhoz a névhez, amely nem
**.keymap**
végű, a program hozzáteszi ezt a kiterjesztést, és a fájlt itt keresi (az
elsőig, amelyet megtalál):

```
1) ~/.config/mc6/
2) {{pkgdatadir}}/
```

Emiatt a kiterjesztés miatt a csomag kiosztásait, amelyek neve
**.ini**
végű, így nem lehet kiválasztani. Ha valamelyiket használni akarod, másold
vagy linkeld a
**~/.config/mc6/keymap.ini**
fájlba, amely utoljára olvasódik be, és nem kell hozzá kapcsoló:

```
ln -s {{sysconfdir}}/mcommander/keymap.vim.ini ~/.config/mc6/keymap.ini
```

## Különleges gombok <a id="miscellaneous-keys"></a>

Itt azon billentyűket találhatod meg, amelyek nem tartoznak bele
egyetlen más kategóriába sem:

**Enter.**
Ha található valamilyen szöveg a parancssorban (az egyik sor a panelek
aljánál), akkor azt lefuttatja, mint parancsot. Ha nem található szöveg
a parancssorban, és a kiválasztás egy könyvtár felett van a
M-Commander-ben, akkor végrehajtja a
**chdir(2)**
(könyvtárváltás) parancsot a kiválasztott könyvtárra és újraolvassa a
panel információit; ha a kiválasztás egy futtatható fájlon van, akkor
lefuttatja azt. Végül, ha a kiválasztott fájl kiterjesztése szerepel a
[társításoknál](#edit-extension-file),
akkor a kijelölt parancs fut le.

**C-l**
Frissít minden információt a M-Commander.

**C-x c**
Futtatja a
[Chmod](#chmod)
parancsot a fájlon, vagy a kijelölt fájlokon.

**C-x o**
Futtatja a
[Chown](#chown)
parancsot a fájlon, vagy a kijelölt fájlokon.

**C-x l**
Futtatja a link parancsot.

**C-x s**
Futtatja a szimbolikus link parancsot.

**C-x i**
Beállítja a másik panel információ megjelenítési
módját.

**C-x q**
Beállítja a másik panelt a quick view-ra (villámnézetre).

**C-x !**
Futtatja a
[Parancskimenet panel](#external-panelize)
parancsot.

**C-x h**
Futtatja a
[könyvtár hozzáadása a Könyvjelzőkhöz](#hotlist)
parancsot.

**M-!**
Futtatja a Szűrés (Filtered view) parancsot, a
[Belső fájlnézőnek](mview.md#internal-file-viewer)
megfelelően.

**M-?**
Futtatja a
[Fájl keresés](#find-file)
parancsot.

**M-c**
Beugrik a
[Gyors cd](#quick-cd)
dialógboxba.

**C-o**
A parancs futtatásakor xterm-en Linux, vagy FreeBSD konzolon, megmutatja
az előzö parancs kimeneteit. Linux konzolon történő futtatáskor a
M-Commander egy beépített programot használ (cons.saver) a
képernyő-információk elmentésére és visszaállítására. Tehát az M-Commander
képernyőjét bármikor kikapcsolhatjuk, és visszakapcsolhatjuk.

Ha a subshell támogatást is befordították, bármikor begépelheted a C-o
gombokat ahhoz, hogy visszatérhess a M-Commander saját
képernyőjéhez, majd a C-o gombok használatával visszatérhetsz a
parancsodhoz. Ha az alkalmazásod felfüggesztett állapotba kerül, ennek a
trükknek a használatakor, nem leszel képes futtatni más parancsot a
M-Commander-ből addíg, amíg a felfüggesztett alkalmazást meg nem
szakítod.

## Könyvtár panelek <a id="directory-panels"></a>

Ez a rész azon billentyűket sorolja fel, amelyek a könyvtár panelekben
használhatóak. Ha tudni akarod azt, hogy hogyan tudod megváltoztatni a
panelek külső megjelenését, akkor nézd meg a
[Bal és jobboldali menük](#left-and-right-menus)
részt.

**Tab, C-i**
Váltja az aktuális panelt. Az előzőleg inaktív panel lesz a jelenlegi
panel és az előzőleg aktív panel lesz az inaktív panel. A kiválasztó sáv
az előzőleg aktívról átugrik az újonnan aktív panelre.

**Insert, C-t**
DEPRECATED! A fájlok kijelölésére az Insert gombot használhatod (a kich1 terminfo
kombináció), vagy a C-t (Control-t) kombinációt. A kijelölés
megszüntetéséhez csak újra ki kell jelölni a kijelölt fájlt.

**Insert**
: to tag files you may use the Insert key (the kich1 terminfo sequence).
To untag files, just retag a tagged file.

**M-e**
: to change charset of panel you may use M-e (Alt-e).
Recoding is made from selected codepage into system codepage. To
cancel the recoding you may select "directory up" (..) in active panel.
To cancel the charsets in all directories, select "No translation " in
the dialog of encodings.

**M-g, M-r, M-j**
A panel legfelső, középső és alsó fájljának kiválasztásához használd
sorban a megfelelő billentyű-kombinációt. Linuxban M-h a "history"
bekapcsolására szolgál.

**M-t**
Vált a jelenlegi lista megjelenítési módról a következő megjelenítési
módra. Ezzel gyorsan át tudsz váltani a hosszú listáról a rendezett
listára és a felhasználó által definiált listázási módra.

**C-\\ (control-backslash)**
Megjeleníti a
[Könyvjelzőket](#hotlist)
és átvált a kiválasztott könyvtárra.

**+  (plusz)**
Ez használható a fájlok csoportjainak kiválasztásához (kijelöléséhez). A
M-Commander megjelenít egy ablakot a jelölendő csoport pontos
kiterjesztésének megadásához. Ha a
*Shell kifejezések*
opció engedélyezve van, csak a pontos kiterjesztések használhatók a
shell-ben kiterjesztésként (\* jelent egy, vagy több karaktert, a ?
egyetlen karaktert). Ha a
*Shell kifejezések-et*
kikapcsolva tartjuk, a fájlok kijelölésére a normál kifejezések
használhatóak (lásd
*ed (1)).*

**\\ (backslash).**
Használd a "\\" gombot a fájlcsoportok kiválasztásának megszüntetéséhez.
Ez a Plusz gomb ellentettje.

**crsr up, C-p**
Az előző panel-bejegyzésre mozgatja a kiválasztó sávot.

**crsr down, C-n**
A következő bejegyzésre lépteti a kiválasztó sávot a panelben.

**home, a1, M-<**
A kiválasztó sávot a panel első bejegyzésére mozgatja.

**end, c1, M->**
A kiválasztó sávot a panel utolsó bejegyzésére mozgatja.

**Page Down, C-v**
A kiválasztó sávot egy oldallal lejjebb viszi.

**Page Up, M-v**
A kiválasztó sávot egy oldallal feljebb viszi.

**M-o**
Ha a másik panel a lista panel és te a könyvtárodon vagy az aktív
panelen, akkor a másik panel tartalma állítódik be a jelenleg aktív
könyvtárban (hasonlóan az Emacs C-o gombjához), egyébként a másik panel
tartalma állítódik be a jelenlegi könyvtár eredeti könyvtárába. Ha a
kurzor könyvtáron áll, akkor az inaktív panelen megnyitja.

**C-PageUp, C-PageDown**
Csak Linux konzolon történő futtatáskor: könyvtárat vált felfelé (..) a
jelenleg kiválasztott könyvtárnak megfelelően.

**M-y**
Az előzőleg látogatott könyvtárba lép vissza, ami azonos a panel tetején
látható '<' jelre egérrel történő kattintással.

**M-u**
A következő látogatott könyvtárba lép át, azonos a '>'
egérrel történő lenyomásával.

**M-S-h, M-H**
Megjeleníti a könyvtár előzményeket, azonos a 'v' egérrel történő
lenyomásával.

## Gyorskeresés és gyorsszűrő <a id="quick-search"></a>

A gyorskeresés a fájlpanelen való gyors kereséshez való. A
**C-s**
vagy az
**Alt-s**
indítja el a fájlnév keresését a könyvtárlistában. Az
**Alt-Shift-s**
a gyorsszűrőt indítja el, amely ugyanazt a mintát használja, de elrejti
azokat a tételeket, amelyek nem tartalmazzák. A szülőkönyvtár tétele mindig
látszik.

Amíg a kettő valamelyike be van kapcsolva, a leütött billentyűk a közös
mintához adódnak hozzá, nem a parancssorhoz. Ha a
*Mini fájlinfó*
beállítás be van kapcsolva, a minta a mini-állapotsorban látszik. Gépelés
közben a kijelölősáv arra a fájlra lép, amelynek a neve a beírt betűkkel
kezdődik; szűrő módban a lista ezenkívül az illeszkedő tételekre szűkül. A
**Backspace**
és a
**DEL**
billentyű a gépelési hibák javítására való.

A
**C-s**
vagy az
**Alt-s**
bekapcsolt gyorsszűrő mellett a gyorskeresésre vált, és megmutat minden
tételt anélkül, hogy a mintát vagy az aktuális fájlt elveszítené. Az
**Alt-Shift-s**
bekapcsolt gyorskeresés mellett visszakapcsolja a szűrőt. A bekapcsolt mód
billentyűjének ismételt leütése a következő találatra lép.

A mozgató billentyűk, a kurzorbillentyűk, a
**Home**,
az
**End**,
a
**PageUp**
és a
**PageDown**
a szűrt listán belül mozognak, és nem zárják be a szűrőt.

A fájlokat a bekapcsolt szűrő mellett is ki lehet jelölni, és a kijelölést meg
lehet szüntetni. A kijelölések megmaradnak a módváltáskor és a szűrő
bezárásakor is.

Ha bármelyik módot a billentyűjének kétszeri leütésével indítod el, az előző
minta kerül elő.

A fájlnév karakterein kívül a '\*' és a '?' helyettesítő karakter is
használható.

## Shell parancssor <a id="shell-command-line"></a>

Ez a rész tartalamazza azokat a billentyű-kombinációkat,
amiket a túlságosan sok gépelés elkerülésére használhatunk a
shell parancsok begépelésénél.

**M-Enter**
A jelenleg kiválasztott parancs nevét átmásolja a parancssorba.

**C-Enter**
Azonos az M-Enter-rel, de ez csak Linux konzolon működik.

**M-Tab**
Fájlnév, parancs, változó, felhasználónév és hostnév
[Kiegészítés](#completion)
készítés. A hiányosan bebillenyűzött filenevet kiegészíti.

**C-x t, C-x C-t**
A parancssorba másolja az aktív panel kijelölt fájlait (ha nincsennek
kijelölt fájlok, a kiválasztott fájlt) (C-xt), vagy a másik paneléit
(C-x C-t).

**C-x p, C-x C-p**
Az első billentyű-sorozat az aktív panel elérési útját átmásolja a
parancssorba, a második billentyű-sorozat pedig az inaktív panel
könyvtárának elérési útját másolja át a parancssorba.

**C-q**
A quote (idézet) parancsot olyan karakterek beillesztésére használhatod,
amelyeket egyébként a M-Commander használ (ilyen pl. a '+'
szimbólum). Például a C-+ elindítja a fájlkijelőlést ahelyett, hogy
beíródna a parancssorba. A
**C-q**
segítségével viszont be lehet írni.

**M-p, M-n**
Ezeket a gombokat az előzőleg kiadott parancsok (a history) közötti
böngészésre használhatod. Az M-p átléptet az előző bejegyzésre, az M-n
átléptet a következő bejegyzésre.

**M-h**
Megjeleníti a jelenlegi beviteli sor előzményeit (history).

## Általános mozgási lehetőségek billentyűzettel <a id="general-movement-keys"></a>

A Súgó néző, a Fájl néző és a Könyvtárfa azonos kódokat használ a
mozgáshoz. Emiatt ezek pontosan ugyanazokat a billentyűket fogadják el.
Ezeken túl néhány olyan van, amely csak az adott eszköz számára
fogadható el.

A M-Commander többi része is használ néhány billentyűt a
mozgáshoz, ezért ebben a részben ezek is használhatók a mozgáshoz.

**crsr Up, C-p**
Egy sort ugrik vissza.

**crsr Down, C-n**
Egy sort ugrik előre.

**Prev Page, Page Up, M-v**
Egy teljes oldalnyit ugrik vissza.

**Next Page, Page Down, C-v**
Egy teljes oldalnyit ugrik előre.

**Home, A1**
A fájl elejére ugrik.

**End, C1**
A fájl végére ugrik.

A Súgó néző és a Fájl néző az itt látható további billentyű-kombinációk
használatát teszi lehetővé:

**b, C-b, C-h, Backspace, Delete**
Egy teljes oldalnyit ugrik hátra.

**Space bar**
Egy teljes oldalnyit ugrik előre.

**u, d**
Egy fél oldalnyit ugrik vissza, vagy előre.

**g, G**
Az elejére, vagy a végére ugrik.

## Beviteli gombok <a id="input-line-keys"></a>

A beviteli sorok (ezek azok, amelyeket a
[Shell parancssor](#shell-command-line)
és a programok lekérdező dialógusablakai használnak) a következő
billentyűket fogadják el:

**C-a**
: a kurzor a sor elejére ugrik.

**C-e**
: a kurzor a sor végére ugrik.

**C-b, Balra**
: a kurzort egy pozícióval balra mozgatja.

**C-f, Jobbra**
: a kurzort egy pozícióval jobbra mozgatja.

**M-f**
: egy szónyit ugrik előre.

**M-b**
: egy szónyit ugrik vissza.

**C-h, Backspace**
: törli az előző (balra eső) karaktert.

**C-d, Delete**
: törli az adott pontban levő karaktert (a kurzor alól).

**C-@**
: beállítja a kijelölés helyét.

**C-w**
: kimásolja a kurzor és a kijelölés közötti szöveget a kill bufferbe, és
törli azt a beviteli sorból.

**M-w**
: kimásolja a kurzor és a kijelölés közötti szöveget a kill bufferbe.

**C-y**
: visszateszi a kill buffer tartalmát.

**C-k**
: törli a szöveget a kurzortól a sor végéig.

**Ctrl-Insert**
: a kijelölt szöveget a csereállományba és a rendszer vágólapjára másolja.
Ha nincs kijelölés: a képen levő panel kijelölt fájljait, soronként egyet;
egyébként az egész sort; egyébként a panel kurzora alatti fájlt.

**Shift-Delete**
: a kijelölt szöveget a csereállományba és a rendszer vágólapjára vágja.

**Shift-Insert**
: a csereállományt egyetlen sorként illeszti be: a sortörések és a többi
vezérlőkarakter szóközzé válik. A parancssorban a panelek megjelenített és
félretett állapotában is működik. 2 KB-nál több szöveg csak megerősítés után
kerül be.

**M-p, M-n**
: ezen billentyűk segítségével közvetlenül böngészhetünk az előzőleg kiadott
parancsok közt. Az M-p visszaléptet az előző bejegyzésre, az M-n pedig
átléptet a következőre.

**M-C-h, M-Backspace**
: egy szót töröl visszafelé.

**M-Tab**
: fájlnév, parancs, változó, felhasználónév és gépnév
[kiegészítését](#completion)
végzi.

<!-- help:break -->

# Menüsor <a id="menu-bar"></a>

A menüsor akkor jelenik meg, ha az F9-es gombot lenyomod, vagy ha a
képernyő legfelső sorára kattintasz. A menüsor hat menüt tartalmaz:
"Bal", "Fájl", "Attribútumok", "Parancsok", "Beállítások" és "Jobb".

A
[Bal és jobboldali menük](#left-and-right-menus)
lehetővé teszik a bal és jobb oldali könyvtár panelek külső
megjelenítésének módosítását.

A
[Fájl menü](#file-menu)
felsorolja a kiválasztott fájlon, vagy a kijelölt fájlokon végrehajtható
parancsokat.

Az
[Attribútumok menü](#attributes-menu)
azokat a parancsokat sorolja fel, amelyek ugyanezeken a fájlokon a
jogosultságokat, a tulajdonost és a fájlrendszer jelzőit módosítják.

A
[Parancsok menü](#command-menu)
felsorolja az általános és a jelenleg kiválasztott fájltól, kijelölt
fájloktól függetlenül végrehajtható parancsokat.

## Bal és jobboldali menük <a id="left-and-right-menus"></a>

A könyvtárpanelek megjelenése változtatható a
**Bal**
és
**Jobb**
menükben.

### Fájllista... <a id="listing-format"></a>

A fájllista módozatok a fájlok megjelenítésének beállítására szolgálnak,
négy különböző listázási mód használható:
**Hosszúlista**,
**Rövidlista**,
**Részleteslista**
és a
**Felhasználói**.
A hosszú könyvtár nézet megmutatja a fájlneveket, a méretüket és a
módosításuk idejét.

A rövid lista nézet csak a fájl nevét és ezt két oszlopban (ekkor
kétszer, vagy többször annyi fájlt láthatsz mint a többi nézetekben). A
részletes lista tisztán az
**ls -l**
parancs kimenetét jeleníti meg. A részletes lista helyenként képernyő
széles is lehet.

Ha a "Felhasználói" megjelenítési formátumot választod, akkor te tudod
meghatározni azt, hogy mi is jelenjen meg a panelekben.

A felhasználói megjelenítésnek a panel méretét megadó bejegyzéssel kell
kezdődnie. Ez lehet "half" (fél), vagy "full" (teljes), ezek határozzák
meg azt, hogy a panelek fél, illetve teljes képernyő szélesen
jelenjenek-e meg.

A panel méretének magadása után, meghatározhatod azt, hogy a panel két
oszlopot tartalmazzon, egy "2"-es hozzáadásával a felhasználói
formátumot megadó szöveghez.

Ezután az opcionális fájl jellemzők neveit kell megadnod. Az itt
megjelenített értékek használhatóak:

**name**
: a fájl nevét jeleníti meg.

**size**
: a fájl méretét jeleníti meg.

**bsize**
: ez a
**size**
formátum egyik formája. Megjeleníti a fájlok és könyvtárak méretét, ha
az utóbbi tartalmaz SUB-DIR-t vagy UP--DIR-t.

**type**
: megjelenít egy egykarakteres érték típust. Ez a karakter állítja be azt,
hogy mit jelenítsen meg az
**ls -F**
flaggel. A csillag-jel a futtatható fájlokhoz, a "slash" jel (törtvonal)
a könyvtárakhoz, a "at-sign" a linkekhez, az "equal" (egyenlőség) jel a
socket-ekhez, a "hyphen" a karakteres eszközökhöz, a pluszjel a blokk
eszközökhöz, a "pipe" a fifo-hoz, a "tilde" a könyvtárak szimbolikus
linkjeihez és a felkiáltójel a stalled szimlinkekhez (linkek, amik
sehova sem mutatnak) használhatók.

**mark**
: a kijelölt fájl megjelölése, csillagozása, space, ha a fájl nem
kijelölt.

**mtime**
: a fájl utolsó módosításának (modify) ideje.

**atime**
: a fájl utolsó hozzáférésének (access) ideje.

**ctime**
: a fájl készítésének (create) ideje.

**perm**
: a megjelenített szöveg a fájl jelenlegi hozzáférési jogainak
(permission) bitjeit mutatja.

**mode**
: a fájl jelenlegi nyolcas számrendszerbeli hozzáférését mutató bit
értéke.

**nlink**
: a fájlra mutató linkek száma.

**ngid**
: a GID (a csoport azonosító kódja; szám).

**nuid**
: a UID  (felhasználó azonosító kódja; szám).

**owner**
: a fájl tulajdonosa.

**group**
: a fájl csoportja.

**inode**
: a fájl inódja (helyfoglalása a harddiszken).

Ezeken kívül még a következő érték megnevezések adhatók meg az értékek
megjelenítéskori rendezéséhez:

**space**
: helykitöltő a megjelenítési formátumban.

**|**
: ez a karakter használható arra, hogy függőleges vonalat jelenítsünk meg.

Egy érték fix méretének megadásához (mezőszélesség megadás), csak egy
':'-ra van szükséged és azt követően a megjelenített érték karaktereinek
számára, ha a szám egy '+' jelet követ, akkor a méret meghatározás a
minimum érték szélességet adja meg, ha a program több helyet talál a
képernyőn, mint ami az alap megjelenítéshez szükséges, ki tudja
használni a maradékot is, az értékek helyének kinyújtásával.

Például a
**Hosszú lista**
megjelenítés ehhez a formátumhoz hasonló:

half type name | size | mtime

A
**Részletes lista**
megjelenítés ennek a formátumnak megfelelő:

full perm space nlink space owner space group space size space mtime
space name

Érdemes például ezt kipróbálni:

half name | size:7 | type mode:3

A Paneleket még a következő módokba lehet állítani:

**Infó**
: Az infó nézet a jelenleg kiválasztott fájlra vonatkozó adatokat mutatja,
és, ha látható információ a jelenlegi fájlrendszerről, akkor azt is.

**Könyvtárfa**
: A könyvtárfa nézet azonos a
[Könyvtárfa](#directory-tree)
eszközzel. Lásd az erről szóló részt további információkért.

**Gyorsnézőke**
: Ebben a módban a panel átvált
[Belső fájlnézőre](mview.md#internal-file-viewer),
amely megjeleníti a jelenleg kiválasztott fájl tartalmát, ha a panelt
választod ki (a tab billentyűvel, vagy az egérrel), elérhetővé válnak a
fájlnéző parancsai.

### Rendezés... <a id="sort-order"></a>

Nyolc rendezési sorrend található itt: Név szerinti, Kiterjesztés
szerinti, Módosítás ideje szerinti, Elérés ideje szerinti, az inode
információk módosítása szerinti, Méret szerinti, az Inode szerinti és a
Rendezetlen elrendezés. A Rendezés dialógus ablakban választhatsz a
rendezési szabályok közül és megadhatod azt is, hogy a megjelenítés a
kijelölt rendezési sorrenddel ellentétes legyen a megfelelő box
kijelölésével.

Alapértelmezésben a könyvtárak a fájlok előtt találhatók, de ez
megváltoztatható a
[Beállításokban](#options-menu)
(**Minden fájl vegyesen**
opciójával).

### Szűrés <a id="filter"></a>

A szűrés parancs engedélyezi számodra azt, hogy meghatározhasd a shell
mintát (például
**\*.tar.gz**),
amelyre a megjelenítendő fájloknak és könyvtáraknak illeszkedniük kell. A
[beviteli sor](#input-line-keys)
veszi a panelen megjelenítendő nevek mintáját.

Ha a
*Csak fájlok*
jelölőnégyzet be van kapcsolva, a szűrő csak a fájlokra vonatkozik, és minden
könyvtár látszik. Egyébként a fájlok és a könyvtárak egyaránt szűrődnek. Ha a
*Shell minták*
jelölőnégyzet be van kapcsolva, a minta úgy működik, mint a shell
fájlnévmintája (a \* nulla vagy több karaktert, a ? egyet jelent). Egyébként
az illesztés szokásos reguláris kifejezéssel történik (lásd ed(1)). Ha a
*Kis/nagybetű számít*
jelölőnégyzet be van kapcsolva, a szűrő megkülönbözteti a kis- és a
nagybetűket, egyébként nem.

### Frissít <a id="reread"></a>

A frissítés parancs újraolvassa a könyvtár fájl listáját. Ez más
processzekben is használható, amikor készítünk egy új fájlt, vagy törlünk
fájlokat. Ha a panelbe mentett fájlneveket használod, a panel újra fogja
olvastatni a könyvtár bejegyzéseket és törli ezen információkat (Lásd a
[Parancskimenet panel](#external-panelize)
részt további információkért).

## Fájl menü <a id="file-menu"></a>

A M-Commander az F1 - F10 gombokat, mint gyorsbillentyűket
használja a Fájl menü parancsainak végrehajtásához. Az F-es gombok
(funkciógombok) a TERMINFO kf1 ... kf10 escape szekvenciáit használják. Ha
a terminálon nincs funkciógomb támogatás, neked kell néhány funkciót
végrehajtanod az ESC (META) gomb és az 1-től 9-ig terjedő és a 0 számok
használatával ( F1-től F9-ig és F10 egyenként megfelelően).

A Fájl menü a következő parancsokat tartalmazza (a gyorsbillentyűk
megjegyzésként megtalálhatóak):

**Súgó (F1)**

Segítségül hívja a beépített hypertext Súgó nézőt. A
[Súgó nézőn](#contents),
belül a Tab gombot használhatod a következő link kiválasztására és az
Enter gombot a link követésére. A Space és a Backspace gombok az előre-
és hátralépésre használhatóak a súgó oldalon belül. Az F1 újbóli
lenyomására egy teljes listát kapsz az elérhető gombokról.

**Menü (F2)**

Ez segítségül hívja a
[felhasználói menüt](#edit-menu-file).
A felhasználói menü könnyű használatot biztosít az új menükkel és az
extra eszközökkel a M-Commander-hez.

**Megnéz (F3, Shift-F3)**

Megmutatja a jelenlegi fájlt. Alapértelmezésben ehhez a
[Belső fájlnézőt](mview.md#internal-file-viewer)
használja, de ha a "Belső Nézegető" opció ki van kapcsolva, a
**PAGER**
környezeti változóban megadott külső fájlnézőt fogja használni. Ha a
**PAGER**
értéke sincs megadva, a "view" parancsot fogja használni. Ha a
Shift-F3-at használod, a fájlnéző minden formázás, vagy átszerkesztés
nélkül nyitja meg a fájlt.

**Szűrés... (M-!)**

Ez egy parancssort jelenít meg a kiadandó parancshoz és a hozzá tartozó
kiegészítés magadásához (a kiegészítés alapértelmezésben a jelenleg
kiválasztott fájl neve), a parancs kimeneteit a belső fájl nézővel
nézhetjük meg.

**Szerkesztés (F4)**

Alapértelmezésben a
**vi**
editort használja, vagy az
**EDITOR**
környezeti változóban megadott szerkesztőt, vagy a
[Belső fájl szerkesztőt](mcedit6.md#internal-file-editor),
ha a belső szerkesztő be van kapcsolva.

**Másol (F5)**

Egy beviteli ablakot jelenít meg, amely alapértelmezésben a nem
kiválasztott panel könyvtárát adja meg rendeltetési helyként, majd
átmásolja a kiválasztott fájlt (vagy kijelölt fájlokat, ha egynél több
fájlról van szó) a beviteli ablakban megadott könyvtárba. Space for
destination file may be preallocated relative to preallocate_space
configure option. A folyamat
futását a C-c, vagy ESC lenyomásával szakíthatod meg. A forrás maszk
beállításairól (ami általában a \*, vagy a ^\\(.\*\\)$ közül valamelyik.
Ezekről a "Shell kifejezések" beállításnál, illetve a
[Kijelölt fájlok másolása vagy áthelyezése](#mask-copyrename)
rendeltetésénél olvashatsz.

Néhány rendszeren a láthatóság beállítható a háttérben történő
másoláshoz a background gomb kijelölésével (vagy a M-b lenyomásával a
dialógboxban). A
[Háttérmunkák](#background-jobs)
a háttér processzek beállítására használható.

**Link (C-x l)**

Hard linket csinál a fájlhoz.

**SymLink (C-x s)**

Szimbolikus linket készít a jelenlegi fájlhoz. Azoknak, amik nem tudják
mire jók ezek a linkek: kapcsolatot hoz létre a fájlhoz a fájl egy
kicsiny másolatával, ám a forrás fájlnév és a célfájl fájlneve ugyanazt
a fájlt jeleníti meg.  Például, ha szerkeszted ezeket a fájlokat, minden
változtatás, amit elvégzel, mindkét fájlban végrehajtódik.  Néhányan a
linkeket alias-nak (~álnév), vagy gyorsbillyentyűnek hívják.

A hard link valós fájlként látszik. Elkészítése után nem lehet megmondani
azt, hogy melyik az eredeti és melyik a link. Ha ezek közül az egyiket
törlöd, a másik sértetlen marad. Ez nagyon eltér attól, hogy egy fájl
önmaga másolataként jelenjen meg. Akkor használj hard linket, amikor
nem igazán tudod mit akarsz csinálni.

A szimbolikus link az eredeti fájl nevére vonatkozik. Ha az eredeti fájlt
töröljük, a szimbolikus link használhatatlan lesz. Ezt elég egyszerű
úgy megjegyezni, hogy ez a fájlok megjelenítése más néven. A
M-Commander "@"-jelet jelenít meg a fájlnév előtt, ha az szimbolikus linkkel
mutat valahova (a könyvtárakat kivéve, ahol tilde (~) jelet mutat). Az
eredeti fájl, ahova mutat a link, láthatóvá válik a mini-fájlinfó sorban,
ha a
*Mini fájlinfó*
opciót engedélyezted. Használj szimbolikus linket, ha el akarod kerülni
az összevisszaságot, amit a hard link okozhat.

**Átnevezés, vagy mozgatás (F6)**

Egy beviteli ablakot jelenít meg, amely alapértelmezésben a nem
kiválasztott panel könyvtárát adja meg rendeltetési helyként, és
átmásolja a kiválasztott fájlt (vagy kijelölt fájlokat, ha egynél
több fájlról van szó) a beviteli ablakban megadott könyvtárba úgy,
hogy az eredeti helyéről letörli. A folyamat futását a C-c, vagy az
ESC lenyomásával megszakíthatod. További részletekért lásd a Másolás
műveletet az elöbbiekben, mivel több dolog azonos.

Néhány rendszeren a láthatóság beállítható a háttérben történő másolás a
**Háttérben gomb**
kijelölésével (vagy a M-b lenyomásával a dialógboxban). A
[Háttérmunkák](#background-jobs)
használható a háttér processzek beállítására is.

**Új könyvtár (F7)**

Megnyit egy beviteli dialógus ablakot, amelyben megadhatod a készítendő
könyvtár jellemzőit.

**Törlés (F8)**

Törli a kiválasztott fájlt, vagy kijelölt fájlokat, vagy könyvtárakat
az aktuális panelben. A folyamatot a C-c, vagy az ESC lenyomásával
megszakíthatod.

**Gyors cd (M-c)**
Használd a
[Gyors cd](#quick-cd)
parancsot, ha teljes parancssort akarsz alkalmazni a könyvtárváltáshoz.

**Csoport kiválasztás (+)**

Ez a fájlok csoportjainak kiválasztására (kijelölésére) használható. A
M-Commander promptot (dialógus ablakot) jelenít meg a csoport
meghatározásának leírására. Ha a
*Shell kifejezések*
et engedélyezted, a pontos beírásnak megfelelő fájlnevek választódnak
ki a shell-ben (\*-ot helyezve az üres-, vagy a több karakterhez és ?-et
helyezve egy adott karakter helyére). Ha a
*Shell kifejezések-et*
kikapcsoltad, akkor a fájlok kijelölése a szabványos kifejezésekkel
(regular expression) történik (lásd
*ed (1)).*

**Csoport kiválasztás megszüntetése (**

A fájlcsoportok kiválasztottságának megszüntetésére szolgál.  Ez a
*Csoport kiválasztás*
parancs ellentéte.

**Kilépés (F10, Shift-F10)**

Leállítja a M-Commander-t. A Shift-F10 akkor használható a
kilépéshez, ha rejtett shellt használsz. A Shift-F10 nem a
M-Commander-rel utoljára meglátogatott könyvtárat őrzi meg, hanem a
M-Commander induláskori könyvtárát.

### Gyors cd <a id="quick-cd"></a>

Ez a parancs akkor használható, amikor a teljes parancssort akarod a
[cd](#the-cd-internal-command)
parancshoz használni, parancssor nélkül. Ez a parancs egy kis dialógus
ablakot jelenít meg, amelybe bármit begépelhetsz, amit a parancssorban a
**cd**
parancs után begépeltél volna, és ezután használd az entert.Ez az eszkőz
mindenben ugyanaz, mint a
[belső cd parancs](#the-cd-internal-command).

## Attribútumok menü <a id="attributes-menu"></a>

Ennek a menünek a parancsai nem a fájl tartalmát változtatják, hanem azt, amit
a fájlrendszer tud róla: a hozzáférési jogosultságokat, a tulajdonost és a
csoportot, valamint a fájlrendszer jelzőit. Mindegyik a kiválasztott fájlon
dolgozik, vagy a kijelölt fájlokon, ha vannak.

**Chmod... (C-x c)**
: A hozzáférési jogosultságok beállítása a
[Chmod](#chmod)
ablakban.

**Chown... (C-x o)**
: A tulajdonos és a csoport beállítása a
[Chown](#chown)
ablakban.

**Haladó chown...**
: A jogosultságok, a tulajdonos és a csoport beállítása egy ablakban, lásd
[Haladó (bővített) Chown](#advanced-chown).

**Chattr jelzők... (C-x e)**
: Az ext2, ext3 vagy ext4 fájlrendszer jelzőinek beállítása a
[Fájlattribútumok](#chattr)
ablakban. A menüpont csak akkor van meg, ha a program ezek támogatásával
készült.

## Parancsok menü <a id="command-menu"></a>

A
[Könyvtárfa](#directory-tree)
parancs lehetővé teszi számodra azt, hogy fa szerkezetben jelenítsd meg
a könyvtárakat.

A
[Fájl keresés](#find-file)
parancs lehetővé teszi számodra a speciális fájlok megkeresését. A
"Panelek felcserélése" parancs felcseréli a két könyvtár panel
tartalmát.

A "Panelok ki-be" parancs megmutatja az utolsó shell parancs kimenetét.
Ez csak xterm-en, Linux-on és FreeBSD konzolon működik.

A Könyvtár összehasonlítás (C-x d) parancs összehasonlítja a könyvtár
paneleket egymással. Ilyenkor használható a Másol (F5) parancs a panelek
azonossá tételére. Ennek három formája van. A gyors változat csak a fájlok
méretét, és dátumát vizsgálja meg. Az alapos változat teljesen, byte-ról
byte-ra végzi el a vizsgálatot. A 'Csak fájlhossz'
szerinti változat csak a fájlméretet hasonlítja össze és nem ellenőrzi
le a dátumukat.

A Parancssor előzmények parancs megmutatja a begépelt parancsok
listáját. Az itt kiválasztott parancs átmásolódik a parancssorba. A
Parancssor előzmények a M-p, vagy a M-n begépelésével is elérhető.

A
[Könyvjelzők](#hotlist) (C-\\)
parancs felveszi a jelenlegi könyvtárat a gyakran használt könyvtárak
közé.

A
[Parancskimenet panel](#external-panelize)
lehetővé teszi számodra külső parancsok futtatását, majd a program
tartalmát a jellegi panelbe teszi.

### Könyvtárfa <a id="directory-tree"></a>

A Könyvtárfa parancs fa formában mutatja meg a könyvtárakat. Ebből a
listából kiválaszthatsz egy könyvtárat és a M-Commander abba a
könyvtárba lép át.

Két lehetőség van a fa megjelenítésére. Az igazi könyvtárfa parancs
elérhető a Parancsok menüből. A másik mód a Bal, vagy a Jobb menüben a
fa nézet kiválasztása.

A M-Commander a fa nézet készítéséhez csak minden könyvtár belső
beállításait szkenneli le, így magszabadít téged a hosszú várakozástól. Ha
megtalálod a megtekinteni kívánt könyvtárat, menj rá a szülökönyvtárára
és nyomd le a C-r-t (vagy az F2-t).

A következő gombokat használhatod:

[Általános mozgási lehetőségek billentyűzettel](#general-movement-keys).

**Enter.**
A Könyvtárfánál kilép a Könyvtárfából és a jelenlegi panelben átváltja
a könyvtárat. Fa nézetben átvált erre a könyvtárra a másik panelben és
a jelenlegi panelben marad a fa nézet.

**C-r, F2 (Újraolvasás).**
Újraolvassa ezt a könyvtárat. Ezt akkor használd, ha a fa nézet
aktualitását vesztette: ez megkeresi a belső könyvtárakat és megmutat
néhány belső könyvtárat, amely eddig nem létezett.

**F3 (Elfelejt).**
Törli ezt a könyvtárat a fa nézetből. Ezt az összevisszaság eltüntetésére
használhatod a fa nézetben.  Ha vissza akarsz tenni egy könyvtárat a fa
nézetbe, nyomd le az F2-t a szülő könyvtáron állva.

**F4 (Statikus-Dinamikus).**
Vált a dinamikus (alapértelmezett) és a statikus böngésző mód között.

A statikus böngésző módban a Fel és Le gombokat használhatod a könyvtár
kiválasztására. Minden ismert könyvtár látható.

A dinamikus böngésző módban a Fel és Le gombokat a testvér könyvtárak
kiválasztására, a Bal gombot a szülő könyvtárra való lépéshez és a Jobb
gombot az alárendelt könyvtárra lépéshez. Csak a szülő, a testvér és
az alárendelt könyvtár látható, a többi nem. A fa nézet a dinamikus
váltáshoz használhatod.

**F5 (Másolás).**
A könyvtárat másolja.

**F6 (Átnevezés vagy mozgatás).**
Áthelyezi a könyvtárat.

**F7 (Létrehoz Könyvtárat).**
Új könyvtárat készít a könyvtár
alá.

**F8 (Töröl).**
Törli a könyvtárat a fájlrendszerből.

**C-s, M-s**
Megkeresi a következő könyvtárat, amely megfelel a keresett szövegnek.
Ha nincs ilyen könyvtár, akkor egy sorral lejjebb lép.

**C-h, Backspace**
Törli az utolsó karaktert a keresési
szövegben.

**Bármely más karakter.**
Karaktert tesz hozzá a keresési szöveghez és átlép a következő olyan
könyvtárra, amely ezekkel a karakterekkel kezdődik. A fa nézetben
először a C-s-sel tudod aktiválni a keresést. A keresett szöveg a mini
fájlinfó sorban jelenik meg.

A további műveletek csak a könyvtárfában érhetőek el. Ezeket a fa nézet
nem támogatja.

**F1 (Súgó)**
Belép a Súgó nézőbe és megjeleníti ezt a részt.

**Esc, F10**
Kilép a Könyvtárfából. Nem vált könyvtárat.

Az egér használható. A dupla kattintás egy Enter-nek felel meg. További
információkat az
[Egér kezelés](#mouse-support)
részben találhatsz.

### Fájl keresés <a id="find-file"></a>

A Fájl keresés eszköz először a keresés induló könyvtárát, majd a keresett
fájlnevet kérdezi meg. A Könyvtárfa gomb lenyomásával kiválaszthatod az
induló könyvtárat a
[Könyvtárfa](#directory-tree)
nézetből.

A "Fájlnév" mező a keresett név mintáját tartalmazza. A program shell
mintaként vagy reguláris kifejezésként értelmezi, aszerint hogy a "Shell
minták" jelölőnégyzet be van-e kapcsolva. Az üres érték is érvényes, az
minden névre illeszkedik.

A "Tartalom" mező azt a szöveget tartalmazza, amelyet a fájlokon belül kell
keresni. Üresen hagyva a program nem keres a tartalomban.

A "Teljes szavak" beállítással a keresés azokra a fájlokra szorítkozik,
amelyekben a találat teljes szót alkot, mint a grep -w esetében.

A keresést az Oké gombbal indíthatod el. Közben a Megállít gombbal
felfüggesztheted, a Folytatás gombbal pedig folytathatod.

A lista minden megtalált fájlnál a módosítás idejét, a méretet és a
jogosultságokat is mutatja a név mellett. Tartalomban való kereséskor egy
fájl egyszer szerepel: az egyetlen találat a fájlnév mellett
"fajl.c:12" alakban látszik, az egynél több találatot tartalmazó fájl pedig
kiírja a találatok számát, és "[+]" jelet kap. Az ilyen fájl találatait a
Balra billentyű vagy a jelre való kattintás nyitja ki: a sor száma és maga a
sor. Ott az Enter a fájlhoz visz, az F3 megmutatja, az F4 pedig szerkeszti a
választott találatnál.

A listában a kurzorbillentyűkkel lehet böngészni. Az Ugrás gomb a kijelölt
fájl könyvtárába lép. Az Újra gomb egy új keresés paramétereit kérdezi meg. A
Kilép gomb bezárja a keresést. A Panelba gomb a megtalált fájlokat az
aktuális panelbe teszi, így további műveletek végezhetők velük (megtekintés,
másolás, mozgatás, törlés és a többi). A szokásos fájllistához a ".."
könyvtárba lépve lehet visszatérni; a panelbe tett eredményt a bal vagy a
jobb oldali menü Panelba pontja hozza vissza.

Az "Ignorált könyvtárak" jelölőnégyzet és az alatta levő mező azoknak a
könyvtáraknak a listáját adja meg, amelyeket a keresés kihagy (például egy
CD-ROM-ot vagy egy lassú kapcsolaton csatolt NFS könyvtárat). A lista elemeit
kettősponttal kell elválasztani:

```
/cdrom:/nfs/wuarchive:/afs
```

Relatív útvonal is megadható. A következő példa a verziókezelők könyvtárait
is kihagyja:

```
/cdrom:/nfs/wuarchive:/afs:.svn:.git:CVS
```

Figyelem: a mező tartalmazhat pontot (.), ami az aktuális abszolút útvonalat
jelenti.

Néhány művelethez érdemes a
[Parancskimenet panel](#external-panelize)
parancsot használni. A Fájl keresés egyszerű lekérdezésekre való, a
Parancskimenet panellel viszont tetszőlegesen összetett keresés végezhető.

### Parancskimenet panel <a id="external-panelize"></a>

A Parancskimenet panel lehetvé teszi számodra külső program futtatását,
és a parancs kimenetének megjelenítését a jelenlegi panelben.

Például, ha egyszerre szeretnéd módosítani a jelenlegi könyvtár összes
szimbolikus linkjét a jelenlegi panelben, a következő parancsot is
használhatod a parancskimenet panelben:

```
find . -type l -print
```

A parancs befejeztével a panelban lévő könyvtár-bejegyzések száma nem
nagyobb mint a jelenlegi könyvtáré, de minden szimbolikus link fájlt
tartalmaz.

Ha minden olyan fájlt meg akarsz jeleníteni a panelben, amelyet ftp
szerverről töltöttél le, használhatod az awk parancsot az átmásolt
fájlok neveit tartalmazó log fájl tartalmának megjelenítésére:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

A gyakran használt parancsokat elmentheted egy számodra egyértelmű
néven, így azokat gyorsan újra előhívhatod a későbbiekben is. Úgy tudsz
ilyen parancsokat létrehozni, hogy begépeled a parancsot a beviteli
mezőbe, és lenyomod az Új gombot. Ekkor begépelheted azt a nevet,
amilyen néven el szeretnéd menteni a parancsot. Következő alkalommal
csak ki kell választanod a parancsot a listából ahhoz, hogy ne kelljen
mégegyszer begépelned azt.

### Könyvjelzők <a id="hotlist"></a>

A Könyvjelzők parancs megmutatja a listában szereplő helyek neveit, és a
program a kiválasztott névhez tartozó helyre lép. A hely lehet könyvtár, egy
virtuális fájlrendszeren belüli útvonal, vagy egy panelbővítmény címe, például
*sftp:gép/könyvtár.*
A párbeszédablakból ki lehet venni a meglevő név-hely párokat, és újakat lehet
hozzáadni. A jelenlegi helyet, akár könyvtár, akár bővítmény panelje, a
Hozzáadás a könyvjelzőkhöz parancs (C-x h) adja hozzá a leggyorsabban, amely
csak a nevet kérdezi meg. Azt a helyet, amely már szerepel a listában, nem
veszi fel másodszor: a párbeszédablak a meglevő tételt mutatja meg.

A párbeszédablak billentyűi:

```
Enter        a kiválasztott helyre lépés
Alt-o        a kiválasztott hely megnyitása a másik panelen
Ctrl-Enter   "cd hely" beírása a parancssorba
Alt-Enter    ugyanez, Ctrl-Enter nélküli terminálokon
Insert       a jelenlegi hely hozzáadása
Shift-F4     új tétel: nevet és helyet kérdez
F7           új csoport
F4           a tétel nevének és helyének szerkesztése
Delete       a tétel törlése
Ctrl-Up      a tétel egy sorral feljebb
Ctrl-Down    a tétel egy sorral lejjebb
F6           a tétel másik csoportba tétele: az ablak felsorolja
             a csoportokat, az Enter megnyit egyet, az Áthelyez
             vagy az újabb F6 a mutatott csoport végére teszi a
             tételt, a kiindulóéba is
F9           az aktuális csoport rendezése név szerint, elöl a
             csoportokkal
Ctrl-s       keresés a listában gépelés közben, Ctrl-s tovább
Right, Left  belépés a csoportba és kilépés belőle
```

Ezzel a gyakran használt könyvtárakhoz ugorhatunk. A CDPATH változó
használatát megtekintheted a
[A cd belső parancs](#the-cd-internal-command)
leírásánál.

### Háttérmunkák <a id="background-jobs"></a>

Ezzel szabályozhatod néhány Commander háttérfolyamat állapotát (csak a
másolás és a mozgatás fájlműveletek tehetők háttérbe). Ezeket a
háttérmunkákat állíthatod le, indíthatod újra, lőheted ki itt. A
linuxban futó background processzekre hatástalan.

### Menü szerkesztés <a id="edit-menu-file"></a>

A felhasználói menü hasznos műveletek menüje, amelyet a felhasználó maga
állít össze. Kétféle alakban létezik: a menü, amely saját magát szerkeszti és
egy kulcsfájlban áll, valamint a régebbi, kézzel írt menüfájl. Ahol
kulcsfájl van, az F2 azt nyitja meg; ahol nincs, ott a régi fájl olvasódik
be, mint eddig.

**A menü, amely saját magát szerkeszti**

A tételek az aktuális könyvtár .mc6menu fájljában és a
~/.config/mc6/menu.ini fájlban állnak, és együtt látszanak. A .mc6menu fájlt
a program csak akkor olvassa be, ha a felhasználóé vagy a rooté, és rajta
kívül senki nem írhatja, mert a tételei parancsokat futtatnak. Semmi mást nem
olvas be: a menüben az van, amit a gazdája beletett, és a program egyetlen
tételt sem hoz magával, így az új felhasználó menüje üres, és az elsőt kéri.
A menün belül:

```
Enter        a tétel futtatása
Ins          tétel hozzáadása
F4           a tétel szerkesztése
F5           tételek behozatala kézzel írt menüfájlból
Shift-F4     a tételt tartalmazó fájl megnyitása
Del          a tétel törlése
Ctrl-Up      a tétel feljebb vitele
Ctrl-Down    a tétel lejjebb vitele
Alt-A        az itt elrejtett tételek is látszanak
```

A tétel egy gyorsbillentyűből, egy feliratból, a parancsokból és azokból a
feltételekből áll, amelyek megmondják, hol látszik. A parancsokban ugyanazok
a helyettesítések működnek, mint a régi menüben, a %f, a %s, a %{prompt} és a
többi, amelyeket a
[makróhelyettesítés](#macro-substitution)
ír le. Két jelölőnégyzet mondja meg, mi legyen a kimenettel: a fájlnézőbe
kerüljön-e, és a parancs a panel shellje nélkül fusson-e.

A párbeszédablak "Show when" sora a tétel feltételeit mutatja, a Conditions
gomb pedig egy párbeszédablakot nyit hozzájuk: az útvonal maszkjai, a
fájltípusok, amelyekre a tétel vonatkozik (ha egy négyzet sincs
bejelölve, bármelyik típus), csak futtatható fájlok, csak ha vannak kijelölt
fájlok, a programok, amelyekre a parancsoknak szükségük van, a panel, amelyet
a feltételek néznek, és hogy a menü ezen a tételen nyíljon-e meg: soha,
mindig, vagy ha teljesülnek azok a feltételek, amelyeket a "When" melletti
gomb nyit meg. Az a tétel, amelynek a feltételei nem teljesülnek, nincs a
listában. A párbeszédablak egyszerre egy panelt mutat; a mindkét panelt
néző feltételeket a fájlban lehet módosítani.

Ha a "Regular expression" jelölőnégyzet be van jelölve, az útvonal mezője
maszkok helyett reguláris kifejezést fogad; a fájlban ez a path~= kulcs.

A menü címe megmondja, hány tétel van elrejtve az adott helyen. Az Alt-A
ezeket is megmutatja, halványabb színnel: így szerkeszthetők,
mozgathatók és törölhetők, de nem futtathatók.

A felirat a helyettesítésekkel együtt látszik, így a "print %f" felirat a
listában a kurzor alatti fájl nevével áll. Maga a fájl ettől nem változik, a
%{...} pedig úgy marad, ahogy le van írva: a lista nem alkalmas kérdezésre.

A tétel parancs helyett almenü is lehet: az Ins megkérdezi, melyiket adja
hozzá. Az almenü a neve után perjellel látszik, mint a könyvtár; az Enter
megnyitja, a címsor pedig megnevezi azokat az almenüket, amelyeken belül
vagy. Az Esc egy szinttel feljebb visz, a legfelsőn pedig kilép a menüből. Az
almenü törlése a benne levőket is törli. A fájlban az almenü egy csoport,
amelyben submenu=true áll és nincs parancs, a benne levő tétel pedig a
parent= kulcsban nevezi meg az almenüt.

A parancsok több sorból álló mezőt alkotnak: az Enter új sort nyit, a
kurzorbillentyűk, a Home és az End pedig a szövegben járnak. A Shift és egy
mozgás kijelöli azt, amin a mozgás áthalad, az egér húzással jelöl ki, a
Ctrl-Insert, a Shift-Insert és a Shift-Delete pedig a csereállományon
keresztül másol, illeszt be és vág ki, ugyanúgy, mint a beviteli sorban. A
Szerkesztő gomb kilép a párbeszédablakból, és megnyitja a tételt tartalmazó
fájlt, arra, amit ott könnyebb megírni.

A tétel abba a fájlba íródik vissza, amelyikből jött, a lista sorrendje pedig
a fájl sorrendje. Csak az íródik ki, ami megváltozott: a megjegyzések, az
üres sorok és a menü által nem ismert kulcsok úgy maradnak, ahogy vannak, a
tétel fölötti megjegyzés pedig együtt mozog a tétellel. A Shift-F4 megnyitja
a fájlt.

**A menü fájlja**

A fájlt kézzel is lehet olvasni és módosítani. Minden tétel egy csoport:
a felirata szögletes zárójelben, utána soronként egy kulcs=érték. Az egysoros
érték úgy áll az "=" után, ahogy van, idézőjelek és escape-ek nélkül. A több
sorból álló parancs két, három backtickből álló sor között áll, és a köztük
levő szöveg pontosan úgy kerül be, ahogy le van írva:

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

Ha maguk a parancsok is tartalmaznak három backtickből álló sort, a
blokkot néggyel kell nyitni és zárni. A '#' jellel kezdődő sorok
megjegyzések. Az első sor mindig "# mc menu format 2", és a menü csak az
ilyen fájlokat olvassa be. Az ilyen sor nélküli fájlt, amelyet egy régebbi
változat vagy valaki kézzel írt, a menü egyszer felajánlja átalakítani, a
fájl korábbi alakját pedig menu.ini.old néven megtartja.

Ezek a kulcsok mondják meg, hol látszik a tétel. Ha több kulcs van,
mindegyiknek teljesülnie kell.

*path=*
: Annak az útvonalára vonatkozó maszkok, amin a kurzor áll, úgy olvasva,
ahogy a git .gitignore fájlja olvassa a mintáit; lásd lent.

*path~=*
: Reguláris kifejezés maszkok helyett, az útvonal bármely részén keresve:
path~=^/home/me/dev/. Ha nincs benne '/', csak az útvonal utolsó részét
nézi: path~=^ttyS. Az elé írt '!' megfordítja. Az érték egyetlen
kifejezés, így a ';' és a '|' is a része.

*on=*
: Amin a kurzor áll: file, dir, link, broken, char, block, fifo, socket,
';' jellel elválasztva; ezek közül bármelyik elég. A könyvtárra mutató
link könyvtárnak számít, a fájlra mutató link fájlnak, a link pedig
bármelyik linkre illeszkedik. A '!' megfordítja a típust: az on=!dir
bármi, ami nem könyvtár.

*exec=true*
: A kurzor alatti fájl nem könyvtár, és futtatható.

*marked=true*
: A panelen vannak kijelölt fájlok; marked=false esetén nincsenek.

*needs=*
: Programok, amelyeknek meg kell lenniük a PATH-ban, vagy teljes elérési
utak; mindegyiknek.

*other.path=, other.path~=, other.on=, other.exec=, other.marked=*
: Ugyanez a másik panelre.

*default=true*
: A menü ezen a tételen nyílik meg, bárhol látszik is.

*default.path=, default.on=, default.other.path=, ...*
: A fenti kulcsok bármelyike "default." előtaggal: a menü ott nyílik meg
ezen a tételen, ahol ezek a kulcsok teljesülnek, a tétel pedig továbbra is
ott látszik, ahol a saját kulcsai mondják. A default.path=ttyS\* akkor
nyitja meg a menüt ezen a tételen, ha a kurzor egy soros porton áll. Ahol
több tétel is szóba jöhet, az első nyer.

A path= maszkját a program annak az útvonalával veti össze, amin a kurzor
áll, úgy, ahogy a .gitignore veti össze a mintáit:

```
*.c               .c fájl bárhol
~/dev/mc/*.c      .c fájl közvetlenül a ~/dev/mc alatt
~/dev/mc/**/*.c   .c fájl ott vagy bármely alkönyvtárban
~/dev/mc/**       bármi ebben a fában
**/src/*          bármi közvetlenül egy src könyvtárban
build/            build nevű könyvtár
*.c;!test_*.c     a C források, a tesztek nélkül
```

A '/' nélküli maszk csak az útvonal utolsó részével van összevetve,
bármely szinten: a \*.1 teljesül egy rrr.1 könyvtáron, de a benne levő
uu.2 fájlon nem. Az elején '/' jellel kezdődő maszk a teljes útvonallal
van összevetve; a közepén '/' jelet tartalmazó a .mc6menu könyvtárától
indul, a menu.ini fájlban pedig bármely szinten teljesül. A '\*' és a '?'
nem lép át '/' jelen, a '\*\*' akárhány könyvtáron átlép, az [abc] és az
[a-z] karakterhalmazok, a végén álló '/' könyvtárat kér, az elején álló
'~' pedig a saját könyvtár. A .gitignore fájltól eltérően a könyvtáron
teljesülő maszk semmit sem mond a benne levő fájlokról.

A maszkok balról jobbra olvasódnak, és az utolsó illeszkedő dönt; az elé
írt '!' jellel kezdődő maszk nemet mond. Ahol egy maszk sem illeszkedik,
a tétel nem látszik: a !\*.o egymagában semmit sem mutat, a "bármi, csak
tárgykódfájl nem" pedig \*;!\*.o. A ".." bejegyzésen az útvonal a panel
könyvtára, utána "/..", így az adott könyvtár maszkja teljesül rajta, egy
név maszkja pedig nem.

Az olyan fájl, amelyet a menü nem tud beolvasni, nem üres menüként látszik:
egy üzenet megnevezi a hibás sort, és felajánlja a fájl megnyitását, a menü
pedig addig nem ír a fájlba, amíg a hibát ki nem javítják.

**A kézzel írt menüfájl**

A telepítés már nem hoz magával ilyet; ami itt következik, ott olvasódik be,
ahol valaki a régebbi alakban tartja a saját menüjét: az aktuális könyvtár
\.usermenu fájlja, ha az létezik, de csak akkor, ha a felhasználóé vagy a
rooté, és nem írhatja bárki. Ha nincs ilyen fájl, ugyanígy a
~/.config/mc6/menu következik.

Ha a saját magát szerkesztő menünek még nincs fájlja, és a program másik
menüt talál (a sajátodat kézzel írva, egy telepített mcommander usermenu
fájlját vagy egy korábbi változat menu.ini fájlját), munkamenetenként egyszer
felajánlja a behozatalát; a menüben az F5, az üres menüben pedig a Behozatal
gomb bármikor kéri, arra a fájlra vagy egy kézzel megnevezettre. Ezután
megmutatja, mi van a fájlban: a szóköz kijelöl egy tételt, az Ins kijelöli és
lelép, a '\*' megfordítja az összes kijelölést, az Enter pedig a
kijelölteket a ~/.config/mc6/menu.ini fájlba viszi, ahol már a
párbeszédablakkal szerkeszthetők. A forrásfájl a helyén marad.

A tétel fölötti feltételek a tétel kulcsai lesznek, ahol a kulcsok ki
tudják fejezni őket: a "+ f \\.c$ | f \\.h$ & t r" feltételből
path=\*.c;\*.h és on=file lesz, a reguláris kifejezésből pedig maszkok,
ahol ugyanazokra a nevekre illeszkednek. Az "=" sorból, amely azt a tételt
választotta ki, amelyen a menü megnyílik, ugyanígy default.\* kulcsok
lesznek. Az a feltétel, amelyet a kulcsok nem tudnak kifejezni, például
egy "&" utáni alternatíva, megjegyzésként marad a tétel fölött, és a
behozatal megmondja, hány ilyen volt.

Az a reguláris kifejezés, amelyet egyetlen maszk sem tud kifejezni,
változatlanul a path~= kulcsba kerül.

A menüfájl formátuma nagyon egyszerű. A sorok, amelyek bármivel
kezdődhetnek, de a space, vagy a tab megkülönböztetett menübejegyzések
(gyorsbillentyűként definiálható az első karakter). Minden olyan sor ami
szóközzel, tabulátorral kezdődik, parancs, amit lefuttat az mcommander, ha
kiválasztottad a bejegyzést.

Ha az opciót kiválasztod, a parancssor bemásolódik egy ideiglenes fájlba
a temp könyvtárba (ez vagy az /usr/tmp, vagy a /tmp), és ilyenkor a fájl
lefut. Ez lehetővé teszi a felhasználónak normál shell parancslista
(script) készítését a menüben. Továbbá egyszerű Makrók helyezhetők el
benne, amelyek a menü kód futtatása előtt futnak le. További
információkért lásd a
[Macro Helyettesítő](#macro-substitution)
részt.

Egy példa az usermenu fájlra:

```
A       A kiválasztott fájlok listázása oktális formában
        od -c %f

B       A hiba leírás szerkesztése és elküldése a root-nak
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
        vi $I
        mail -s "M-Commander bug" root < $I
	rm -f $I

M       Levél olvasás
        emacs -f rmail

N       A Usenet hírek elolvasása
        emacs -f gnus

H       Az info hypertext böngésző elindítása
        info

J       A jelenlegi könyvtár rekurzív átmásolása a másikba
        tar cf - . | (cd %D && tar xvpf -)

K       Az aktuális könyvtárról archiválása
        echo -n "Name of distribution file: "
        read tar
        ln -s %d `dirname %d`/$tar
        cd ..
        tar cvhf ${tar}.tar $tar

= f *.tar.gz | f *.tgz & t n
X       A kijelölt tömörített tar fájl kicsomagolása
        tar xzvf %f
```

**Alapértelmezett Feltételek**

Néhány menü bejegyzés irányadó feltételként szerepelhet. A feltétel eslő
oszlopában az '=' karakternek kell lennie. Ha a feltétel igaz, a
menüpont alapértelmezett bejegyzéssé fog válni.

```
Feltétel szintaktika: 	= <belső-felt.>
   vagy:                = <belső-felt.> | <belső-felt.> ...
   vagy:                = <belső-felt.> & <belső-felt.> ...

A belső feltétel az alábbiak közül valamelyik:

  y <minta>             a jelenlegi fájlminta szintaktikusan
                        illeszkedik?
                        csak menüszerkesztéshez
  f <minta>             jelenlegi fájlminta egyezik?
  F <minta>             egyéb fájlminta egyezik?
  d <minta>             jelenlegi könyvtár minta egyezik?
  D <minta>             más könyvtár minta egyezik?
  t <type>              jelenlegi fájltípus?
  T <type>              más fájltípus?
  x <fájlnév>           ez futtatható fájlnév?
  ! <belső-felt.>       a belső feltételek ellentéte
```

A minta lehet a shell által értelmezett, vagy lehet szabványos
kifejezés. Felülírhatod a rendszerszintű értékeket a Shell kifejezések
opcióval a "shell_patterns=x" beírásával a menü fájl első sorában (ahol az
"x" a 0 és 1 közül valamelyik lehet).

A következő karakterek közül egyet, vagy többet is begépelhetsz:

```
  n	nem könyvtár
  r	szabályos fájl
  d	könyvtár
  l	link
  c	speciális karakter
  b	speciális blokk
  f	fifo
  s	socket
  x	futtatható fájl
  t	fájl kijelölve
```

Például az 'rlf' bejegyzés esetén lehet fájl, link, vagy fifo. A 't'
típus egy kicsit különleges, mert nem fájlon, hanem panelen dolgozik. A
'=t t' feltétel igaz akkor, ha a jelenlegi panelben vannak kijelölt
fájlok és hamis, ha nincsennek.

Ha a feltétel '=?'-lel kezdődik '=' helyett, a hibakereső (debug)
kimenete jelenik meg, mialatt akkor a feltétel eredményét a program
kiértékeli.

A feltételek kiértékelése balról-jobbra történik. Ennek megfelelően:

```
	= f *.tar.gz | f *.tgz & t n
```

kibontva:

```
 	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

Egy példa a feltételek használatára:

```
= f *.tar.gz | f *.tgz & t n
L	Listázza az aktuális tar archívumot
	gzip -cd %f | tar xvf -
```

**Járulékos feltételek**

Ha a feltétel '+'-szal (vagy '+?'-lel) kezdődik az '=' (vagy '=?')
helyett, ez járulékos feltétel. Ha a feltétel igaz, a menü bejegyzés
megjelenik a menüben. Ha a feltétel hamis, a menü bejegyzés nem jelenik
meg.

Kombinálhatod is az alapértelmezett és a járulékos feltételeket a
feltétel sorának '+='-lel, vagy '=+'-szal (vagy '+=?'-lel és '=+?-lel',
ha hiba követőt is szeretnél) kezdésével. Ha két eltérő feltételt
szeretnél használni, egyet járulékosként és egyet alapértelmezettként,
két feltételsort kell készítened; egyet '+'-szal kezdődően és egy
másikat '='-lel kezdődően.

A magyarázat sorát '#'-kal kell kezdened. A kiegészítő magyarázat sorait
'#'-kal, space-szel, vagy tab-bal kell kezdened.

## Beállítások <a id="options-menu"></a>

A programnak számos beállítása van, amelyeket az ebből a menüből elérhető
párbeszédablakokban lehet ki- és bekapcsolni. A bekapcsolt beállítás előtt
csillag vagy "x" áll. A menü sorrendben a következőket tartalmazza:

Az
[M-Commander konfigurálása](#configuration)
a beállítások többségét tartalmazó ablakot nyitja meg.

A
[Megjelenés](#layout)
azt az ablakot nyitja meg, amelyben a képernyő felosztása állítható.

A
[Panel beállítások](#panel-options)
a fájlkezelő paneljeinek beállításait nyitja meg.

A
[Fájlpanel módok](#panel-modes)
a megnevezett listaformák listáját nyitja meg, ahol új hozható létre, a
meglevő szerkeszthető és törölhető.

A
[Megerősítés](#confirmation)
azt az ablakot nyitja meg, amelyben megadható, mely műveleteket kell
megerősíteni.

A
[Megjelenés (skin)](#appearance)
a skin kiválasztására való.

A
[Billentyűzet tanítás](#learn-keys)
megtanítja a programnak azokat a billentyűket, amelyeket egyes terminálok nem
küldenek helyesen.

A
[Billentyűtársítások](#key-bindings)
a műveletek listáját nyitja meg a hozzájuk tartozó billentyűkkel, ahol egy
billentyű átállítható, az eredmény pedig a kiosztásfájlba kerül.

A
[Billentyűfigyelő](#key-sniffer)
megmutatja, mit küld a terminál a leütött billentyűre, és azt a műveletet,
amelyhez az a billentyű tartozik.

Az
**Összehasonlító beállításai**,
a
[Megjelenítő beállításai](mview.md#viewer-options)
és a
**Szerkesztő beállításai**
annak a három programnak az ablakait nyitja meg, amelyek fájlt mutatnak: az
összehasonlítóét, a fájlnézőét és a szerkesztőét. Ugyanezek az ablakok
megtalálhatók mindegyikük saját Beállítások menüjében is; itt fájl megnyitása
nélkül is elérhetők. Az összehasonlító az indulásakor veszi át a
beállításait, így a már megnyitott összevetés azokkal dolgozik tovább,
amelyekkel elindult.

A
[Bővítmények kezelése](#panel-plugins)
felsorolja a betöltött bővítményeket, ki tud kapcsolni egyet, és megnyitja a
beállításait.

A
[Társítások](#edit-extension-file)
lehetővé teszik számodra a futtatandó programok meghatározását, a
kiválasztott fájl kiterjesztésének (fájlnév vége) megfelelően akkor, ha
futtatod, megtekinted a tartalmát, átszerkeszted vagy más egyéb dolgot
szeretnél vele csinálni.

A
**Fájltípus színkiemelés szerkesztése**
parancs azt a fájlt nyitja meg, amely megadja, hogy a panel mely neveket és
mely fájltípusokat milyen színnel mutatja, lásd
[Fájlnevek kiemelése](#filenames-highlight).

A
[Beállítások mentése](#save-setup)
elmenti a Bal, a Jobb és a Beállítások menü jelenlegi értékeit. Néhány más
beállítás is mentődik.

A
**Névjegy**
megmutatja a program verzióját és azt, hogy kik írták.

### Az M-Commander konfigurálása <a id="configuration"></a>

A dialógus ablalban lévő opciók három csoportra bonthatók: Panel
Beállítások, Futtatás után vár... és Egyéb.

**Panel Beállítások**

*Backup fájlt mutat.*
Alapértelmezésben a M-Commander nem mutatja a '~'-re végzödő
fájlokat (a GNU' -B opciójának megfelelően).

*Rejtett fájlt mutat.*
Alapértelmezésben a M-Commander láthatóvá teszi a ponttal kezdődő
fájlokat (az ls -a -hoz hasonlóan).

*Kijelölés után lefele lép*
Alapértelmezésben, amikor kijelölsz egy fájlt (a Insert
gomb közül valamelyikkel,) a kiválasztó sáv lefelé mozdul el.

*Legördülő menük.*
Amikor ezt az opciót engedélyezed, az
**F9**
gomb lenyomásakor a menü le fog ereszkedni, egyébként te csak a menü
címét tudod megjeleníteni és ezek után tudod kiválasztani a menü
bejegyzést a nyíl gombokkal, vagy annak megjelölt betűjével, és csak
ekkortól tudsz menüpontot kiválasztani.

*Minden fájl vegyesen.*
Ha ezt az opciót engedélyezted, a fájlok és könyvtárak vegyesen jelennek
meg. Ha az opció ki van kapcsolva, a felsorolás a könyvtárakkal (és a
könyvtár linkekkel) fog kezdődni, és ezeket az egyéb fájlok követik.

*Gyors könyvtárlista.*
Ez az opció alapértelmezésben ki van kapcsolva. Ha bekapcsolod a gyors
könyvtárlista funkciót, a M-Commander egy trükköt fog használni
akkor, ha a könyvtár tartalma megváltozik. A trükk az, hogy csak
akkor olvassa újra a könyvtárat, ha a könyvtár inode-ja megváltozott;
ez azt jelenti, hogy csak fájl létrehozásakor, és törlésekor kerül
újraolvasásra. Ha valami a könyvtárban lévő fájl inode-jában történik
(fájlméret-változás, módok, és tulajdonosok változnak, stb.) a
megjelenítés nem kerül frisítésre. Ebben az esetben, ha az opció be van
kapcsolva, kézzel tudod újraolvastatni a könyvtár tartalmát (a
*C-r-rel).*

**Futtatás után vár**

Az általad kiadott parancs lefutása után a M-Commander várhat
amiatt, hogy meg tudd vizsgálni a parancs kimenetét. Három beállítás
adható meg ennek a változónak:
*Soha*
Abban az esetben, ha te nem kívánod látni azt, hogy mit írt ki a
parancs. Ha Linux, vagy FreeBSD konzolt, vagy xterm-et használsz, a
parancs kimenete a
*C-o*
begépelésével megjeleníthető.
*Buta terminálokon*
várakozási üzenetet fogsz kapni azon a terminálon, amely nem képes
megmutatni az utolsóként kiadott parancs kimenetét (bármilyen terminálon,
amely nem xterm, vagy nem Linux konzol).
*Mindig*
A program mindig vár, miután a parancsod lefutott.

**Egyéb beállítások**

*Részletes műveletinfó.*
Ez van bejelölve akkor, ha a fájl
Másolás, Átnevezés és Törlés műveletek részletesek (pl., egy
dialógus ablakot jelenít meg néhány művelethez). Ha lassú
terminálod van, beállíthatod azt, hogy ne legyenek részletes
műveletek. Automatikusan kikapcsolódik ez a beállítás, ha a
terminálod sebessége kissebb mint 9600 bps.

*Byteok számítása*
Ha ez az opció engedélyezve van, a M-Commander számítja a teljes
byte méretet és a teljes fájlszámot a Másolás, Átnevezés és a Törlés
műveleteknél. Ez a funkció ellát téged több pontos folyamat sávval,
kiegészítve azt néhány sebességgel. Ez az opció nem látható, ha a
*Részletes műveletinfót*
nem engedélyezted.

*Shell mintázatok*
Alapesetben a Kiválasztás, Kiválasztás megszüntetése és a Szűrés
parancsok a shell-nek megfelelő pontos kiterjesztéseket használják. A
következő konverzió átalakítások vannak jelenleg: a '\*' kicserélődik
a '.\*'-gal (zeró, vagy több karakter); a '?' kicserélődik a '.'-tal
(pontosan egy karakter) és a '.' a szó szerinti ponttal. Ha az opció nem
engedélyezett, akkor a szokásos kiterjesztések azonosak a ed-ben lévőkkel:
*man ed.*

*Beállítások automatikus mentése*
Ha ez az opciót bekapcsoltad, amikor kilépsz a M-Commander-ből,
az M-Commander opcióinak beállításait az ~/.config/mc6/ini fájlba menti.

*Auto menük.*
Ha ez az opció engedélyezett, a felhasználói menü megjelenik az mcommander
indításkor. Különösen azok számára ajánlott, akik nem szoktak hozzá a
UNIX-os környezethez.

*Belső szövegszerkesztő*
Ha ez az opció engedélyezve van, a beépített fájlszerkesztőt használja a
fájlok szerkesztésére. Ha az opciót nem engedélyezzük, az mcommander az
**EDITOR**
környezeti változóban megadottat használja. Ha ez sincs megadva, a
**vi**-t
fogja használni. Lásd a
[Belső fájlszerkesztő](mcedit6.md#internal-file-editor).
részben.

*Belső nézegető.*
Ha ezt az opciót engedélyeztük, a beépített fájlnézőt fogja a fájlok
tartalmának megtekintéséhez használni a program. Ha nem engedélyeztük, a
**PAGER**
környezeti változóban megadott pager értéket használja. Ha nincs megadva
a pager értéke, a
**Megnéz**
parancsot használja. Lásd a
[Belső fájlnéző](mview.md#internal-file-viewer)
részben.

*Kiegészítés: minden mutat*
Alapértelmezésben a M-Commander megjelenít minden elem
[Kiegészítést](#completion).
Ha a kiegészítésben bizonytalan vagy, nyomd le az
**M-Tab**-ot
és a második alkalommal kiegészíti, első alkalommal csak annyit jelenít
meg, mint amennyit lát, és ebben az esetben a kétértelműség miatt egy
beep hangot is kapsz. Ha látni szeretnéd az összes kiegészítést, az első
**M-Tab**
lenyomása után, engedélyezd ezt az opciót.

*Forgó törtjel*
Ha ezt az opciót engedélyezted, a M-Commander forgó törtjelet
jelenít meg a jobb felső sarokban, mutatva ezzel azt, hogy munka van
folyamatban.

*Mozgás, mint lynx-ben*
Ha ezt az opciót engedélyezted, a nyíl gombokat, mint automatikus
könyvtárváltókat használhatod ha az aktuális kiválasztás egy belső
könyvtár és a shell parancssor elérhető. Alapértelmezésben ez a
beállítás ki van kapcsolva.

*Cd követi a linket*
Ez az opció, ha be van állítva, akkor a M-Commander követi a
könyvtárak logikai kapcsolatait ha könyvtárat váltasz valamelyik
panelben, vagy a cd parancsot használod. Ez alapértelmezésben a bash
jellemzője. Amikor ez nincs beállítva, a M-Commander a valós
könyvtárszerkezetet követi, úgy, mint amikor a cd..-t gépeled be, a
könyvtáron keresztül átlépsz a "szülő" könyvtárba, és nem abba a
könyvtárba, amelyre a link mutat.

*Biztonságos törlés*
Ha ezt az opciót engedélyezted, a fájlok közvetlen szándék nélkül
törlését megnehezíted.  Alapesetben egy "Megerősítés" dialógus ablaknan
választhatunk az "Igen" és "Nem" gombok között törléskor. Alapesetben ez
az opció nem engedélyezett.

### Megjelenés <a id="layout"></a>

Ebben a párbeszédablakban a képernyő általános felosztása állítható. A
beállítások három csoportba vannak osztva: "Panelfelosztás",
"Konzolkimenet" és "Egyéb beállítások".

**Panelfelosztás**

A képernyő többi részét a két fájlpanel foglalja el. Megadható, hogy a
felosztás
*Függőleges*
vagy
*Vízszintes*
legyen. A felosztás az Alt-, (Alt-vessző) billentyűvel is váltható.

*Egyenlő felosztás.*
Alapértelmezés szerint a két panel egyforma méretű. Ezzel a beállítással
eltérő felosztás is megadható.

**Konzolkimenet**

Linux vagy FreeBSD konzolon megadható, hány sor látszik a kimeneti ablakban.
Ez a beállítás csak natív konzolon futó programban érhető el.

**Egyéb beállítások**

*Menüsor látszik.*
Bekapcsolva a főmenü mindig látszik a képernyő felső sorában, a panelek
fölött. Alapértelmezés szerint be van kapcsolva.

*Parancssor.*
Bekapcsolva a parancssor használható. Alapértelmezés szerint be van
kapcsolva.

*Gombsor látszik.*
Bekapcsolva az F1-F10 billentyűkhöz tartozó tíz felirat a képernyő alsó
sorában látszik. Alapértelmezés szerint be van kapcsolva.

*Tippsor látszik.*
Bekapcsolva az egysoros tippek a panelek alatt látszanak. Alapértelmezés
szerint be van kapcsolva.

*XTerm ablakcím.*
X11 alatti terminálemulátorban futva a program a terminálablak címét az
aktuális könyvtárra állítja, és szükség szerint frissíti. Ha a
terminálemulátorod hibás, és induláskor vagy könyvtárváltáskor zavaros
kimenetet látsz, kapcsold ki ezt a beállítást. Alapértelmezés szerint be van
kapcsolva.

*Szabad hely mutatása.*
Bekapcsolva az aktuális fájlrendszer szabad és teljes területe látszik a
panel alsó keretén. Alapértelmezés szerint be van kapcsolva.

### Megerősítés <a id="confirmation"></a>

Ebben a menüben tudod beállítani enter lenyomására a törlés,
felülírás, futtatás, és programból történő kilépés
Megerősítésének opcióit.

### Billentyűzet tanítás <a id="learn-keys"></a>

Ez a párbeszédablak megtanítja a programnak azokat a vezérlősorozatokat,
amelyeket a terminálod a funkcióbillentyűkre, a kurzorbillentyűkre és a
mozgató billentyűkre küld.

Válaszd ki a jelölőnégyzetekkel a módosítók együttesét (Ctrl, Alt, Shift),
majd nyomd meg a keresett billentyű gombját. Üsd le magát a billentyűt, és
várd meg, amíg a felvételt jelző üzenet eltűnik. A megtanult sorozat a gomb
mellett jelenik meg.

**Del**
\- a megtanult billentyű elfelejtése.

**Mentés**
\- a megtanult billentyűk kiírása a ~/.config/mc6/term/\<TERM> fájlba.

**Terminálfájl szerkesztése**
\- a terminál billentyűdefinícióit tartalmazó fájl megnyitása a
szerkesztőben.

A ~/.config/mc6/ini fájl [terminal:TERM] szakaszában levő régi definíciók az
első indításkor maguktól átkerülnek.

### Bővítmények kezelése <a id="manage-plugins"></a>

A program által betöltött bővítmények táblázatban: a fajtája, a neve és az,
amit a bővítmény magáról mond. A sor jelölőnégyzete ki- és bekapcsolja; ami ki
van kapcsolva, legközelebb sem töltődik be.

**Enter, F4**
: Megnyitja annak a bővítménynek a beállításait, amelyen a kurzor áll. Amelyiknek
nincs beállítása, azt megmondja.

Itt szerepelnek a
[panel bővítmények](#panel-plugins)
a szerkesztő bővítményeivel és a Lua szkriptcsomagokkal együtt; egy csomag
szkriptjeit a beállításaiból nyíló
[Lua szkriptek](#lua-scripts)
ablak mutatja.

### Lua szkriptek <a id="lua-scripts"></a>

Egy Lua csomag szkriptjei táblázatban: a név, az azonosító, hol van a szkript,
mit nyújt és mit csinál. A sor jelölőnégyzete ki- és bekapcsolja.

**Beállítások**
: Lefuttatja a csomag beállításait tartalmazó szkriptet, ha van ilyen.

### A fájl létezik <a id="plugin-file-exists"></a>

Egy bővítmény paneljébe másolás ott ilyen nevű fájlt talált. Az ablak mutatja
annak az útvonalát, méretét és idejét, amit másolunk, és annak is, ami már ott
van, majd megkérdezi, mi legyen: írja felül, hagyja ki, folytassa a másolást
onnan, ahol abbamaradt, ha a bővítmény tudja folytatni, vagy szakítsa meg az
egész műveletet.

### Kódlap választása <a id="codepages-translation"></a>

Azoknak a kódlapoknak a listája, amelyeket a program ismer, a
**{{pkgdatadir}}/charsets**
fájlból. A választás megmondja a programnak, milyen kódlapon vannak a nevek
vagy a szöveg, a
**\<Nincs átalakítás>**
pont pedig bájtként hagyja őket. A listát az
**Alt-e**
nyitja meg a panelben, a megjelenítőben és a szerkesztőben, valamint a
menüjük megfelelő pontja.

### A beviteli sor előzményei <a id="history-query"></a>

Annak a listája, amit korábban a beviteli sorba írtak, a legutóbbival kezdve;
az
**Alt-h**
nyitja meg annak a sornak, amelyben a kurzor áll. Az Enter a kurzor alatti
bejegyzést a sorba teszi, az Esc úgy hagyja a sort, ahogy volt, az
**F8, Del**
pedig törli a bejegyzést az előzményekből.

### Panel beállítások <a id="panel-options"></a>

**Fő panelbeállítások**

*Mini-állapotsor.*
Bekapcsolva a panelek alján egy sornyi tájékoztatás látszik a kurzor alatti
tételről. Alapértelmezés szerint be van kapcsolva.

*Tizedes mértékegységek.*
Bekapcsolva a program SI előtagokat (tízes alap) használ a méretek
kiírásakor. Kikapcsolva (ez az alapértelmezés) IEC előtagokat (kettes alap).

*Vegyes fájllista.*
Bekapcsolva a fájlok és a könyvtárak összekeverve látszanak. Kikapcsolva (ez
az alapértelmezés) a könyvtárak (és a rájuk mutató linkek) a lista elején
állnak, a többi fájl alattuk.

*Biztonsági másolatok mutatása.*
Bekapcsolva a hullámvonalra végződő fájlok is látszanak, egyébként nem (mint
a GNU ls -B kapcsolója). Alapértelmezés szerint be van kapcsolva.

*Rejtett fájlok mutatása.*
Bekapcsolva a ponttal kezdődő fájlok is látszanak (mint az ls -a esetében).
Alapértelmezés szerint ki van kapcsolva.

*Gyors könyvtárfrissítés.*
Bekapcsolva a program trükkel állapítja meg, változott-e a könyvtár tartalma:
csak akkor olvassa újra, ha a könyvtár i-node-ja változott, vagyis ha fájl
jött létre vagy szűnt meg. Ha egy fájl i-node-ja változik (mérete, módja,
tulajdonosa), a kép nem frissül; ilyenkor kézzel kell újraolvasni (C-r).
Alapértelmezés szerint ki van kapcsolva.

*Kijelöléskor lefelé lép.*
Bekapcsolva a kijelölősáv lefelé lép, amikor egy fájlt kijelölsz (az Insert
billentyűvel). Alapértelmezés szerint be van kapcsolva.

*Csak fájlok megfordítása.*
Bekapcsolva a Fájl menü "Kijelölés megfordítása" pontja csak a fájlokra
vonatkozik, nem a könyvtárakra is. Alapértelmezés szerint be van kapcsolva.

*Egyszerű csere.*
Ha mindkét panel fájllistát mutat, az egyszerű csere azt jelenti, hogy a
panelek helyet cserélnek a képernyőn: a bal oldaliból jobb oldali lesz, és
fordítva. Kikapcsolva a két panel a tartalmát cseréli, a listaformát és a
rendezést megtartva. Alapértelmezés szerint ki van kapcsolva.

*Panelbeállítások automatikus mentése.*
Bekapcsolva a program kilépéskor a panelek jelenlegi beállításait a
~/.config/mc6/panels.ini fájlba menti. Alapértelmezés szerint ki van
kapcsolva.

*Könyvtárak figyelése.*
Bekapcsolva a program megkéri a rendszermagot, hogy szóljon a panelekben
látszó könyvtárak változásairól, és újraolvassa a panelt, ha benne egy fájl
létrejön, megszűnik vagy megváltozik valami mástól: egy másik terminálból,
egy fordításból vagy a terminálablak shelljéből. A képen kívüli panel akkor
olvasódik újra, amikor visszatér. Egy figyelés egy egész könyvtárra
vonatkozik, így a költsége nem függ a benne levő fájlok számától, és sok
gyors változás egyetlen újraolvasást eredményez. A virtuális fájlrendszerek
könyvtárai és azok, amelyeket a rendszermag nem tud figyelni, például az NFS,
a korábbi módon működnek: ott a C-r végzi el a munkát. Amíg ez a beállítás be
van kapcsolva, a Gyors könyvtárfrissítésnek nincs mit megspórolnia, ezért
kikapcsolva látszik. Alapértelmezés szerint be van kapcsolva.

**Mozgás**

*Lynx-szerű mozgás.*
Bekapcsolva a kurzorbillentyűkkel lehet könyvtárat váltani, ha a kurzor alatt
alkönyvtár áll és a parancssor üres. Alapértelmezés szerint ki van kapcsolva.

*Lapozó görgetés.*
Bekapcsolva (ez az alapértelmezés) a panel fél képernyőnyit gördül, amikor a
kurzor a panel aljára vagy tetejére ér, egyébként fájlonként gördül.

*Középre görgetés.*
Bekapcsolva a panel akkor gördül, amikor a kurzor a panel közepére ér, és
csak az első, illetve az utolsó fájlnál áll meg a panel tetején vagy alján.
Ez fájlonkénti gördítéskor érvényes, a lapozó billentyűkre nem vonatkozik.

*Lapozás egérrel.*
Azt szabályozza, hogy az egérgörgő a paneleken lapokat vagy sorokat gördít-e.

**Fájlok kiemelése**

Megadható, hogy a
*jogosultságok*
és a
*fájltípusok*
külön
[színekkel](#colors)
legyenek-e kiemelve. Ha a jogosultságok kiemelése be van kapcsolva, a
*perm*
és a
*mode*
[megjelenítési mező](#listing-format)
azon része, amely a programot futtató felhasználóra vonatkozik, a
*marked*
kulcsszóval megadott színt kapja. Ha a
*jogosultságszínek*
be vannak kapcsolva, a
*perm*
mező minden karaktere a jelentése szerinti színt kapja: a skin
*permread ,*
*permwrite ,*
*permexec ,*
*permspecial*
és
*permnone*
színét az r, w, x, s/t és - karakterekhez. A kettő egyszerre is
bekapcsolható; a felhasználóra vonatkozó hármas ilyenkor megtartja a
*marked*
színt. Ha a fájltípusok kiemelése be van kapcsolva, a fájlnevek a
{{sysconfdir}}/mcommander/filehighlight.ini fájlban leírt szabályok szerint
színeződnek. Bővebben lásd a
[Fájlnevek kiemelése](#filenames-highlight)
részt.

**Gyorskeresés és gyorsszűrő**

Megadható, hogyan működjön a
[gyorskeresés](#quick-search)
és a gyorsszűrő: a kis- és nagybetűt ne különböztesse meg, különböztesse meg,
vagy igazodjon a panel rendezéséhez, amely szintén megkülönböztetheti vagy
sem.

### Megjelenés (skin) <a id="appearance"></a>

A program kinézetét adó skin választása. A lista azokat a skineket mutatja,
amelyek a
**{{pkgdatadir}}/skins**
és a
**~/.local/share/mc6/skins**
könyvtárban vannak; a kiválasztott azonnal életbe lép. A skinek felépítését az angol kézikönyv
[Skins](mcommander.md#skins)
szakasza írja le.

### Társítások <a id="edit-extension-file"></a>

Ez az
*~/.config/mc6/extensions.ini*
szerkesztéséhez segítségül fogja hívni a szövegszerkesztődet.
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.

### Beállítások mentése <a id="save-setup"></a>

A M-Commander indításkor megpróbálja az indítási információkat
beolvasni az
*~/.config/mc6/ini*
fájlból. Ha ez a fájl nem létezik, ezeket az információkat a
rendszerszintű konfigurációs fájlból fogja beolvasni, amelyek a
*{{pkgdatadir}}/mc.ini*
fájlban találhatóak meg. Ha ez a rendszerszintű konfigurációs fájl sem
létezik, a M-Commander az alapértelmezett beállításokat használja.

A
*Beállítások mentése*
parancs elmenti a
[Bal és Jobb oldali menü](#left-and-right-menus)
és a
[Beállítások](#options-menu)
menü beállításait az ~/.config/mc6/ini fájlba.

Ha aktiválod az
*Auto Beállításmentés*
opciót a M-Commander mindíg elmenti a beállításait kilépéskor.

Még vannak beállítások, amelyek nem állíthatóak be a menükből. Ezek
beállításához használd a kedvenc fájlszerkesztődet. Lásd a
[Speciális Beállítások](#special-settings)
részt a további információkért.

<!-- help:break -->

# Az operációs rendszer parancsainak futtatása <a id="executing-operating-system-commands"></a>

Közvetlenül futtathatod a parancsokat azok begépelésével a
M-Commander beviteli sorába, vagy a futtatandó program kiválaszátásval
valamely panelben a kiválasztó sáv segítségével, és az Enter
használatával.

Ha az Enter-t az adott fájl felett lenyomod, nem indul el azonnal, hanem
a M-Commander leellenőrzi a kiválasztott fájl kiterjesztését a
[Társításokban](#edit-extension-file)
találhatónak megfelelően. Ha talál egyezést, akkor a kódnak megfelelő
bejegyzést futtatja. Egy nagyon egyszerű
[Macro Helyettesítő](#macro-substitution)
végzi ezt el a parancs futtatása előtt.

## A cd belső parancs <a id="the-cd-internal-command"></a>

A
*cd*
parancs végrehajtását a M-Commander nem adja át a shellnek. Tehát
a shellben értelmezett makrók és helyettesítések helyett a saját
beállításai szerint dolgozik:

*Tilde helyettesítés*
A (~) karakter helyettesíti a home könyvtár nevét, ha hozzáfűzöl bármely
felhasználói nevet, akkor az M-Commander a megadott felhasználó saját HOME
könyvtárára ugrik.

Például a ~guest a guest felhasználó könyvtárára mutat, amíg a ~/guest a
guest könyvtárra a te home könyvtáradban.

*Előző könyvtár*
Vissza tudsz ugrani abba a könyvtárba, ahol előzőleg voltál a '-'
speciális könyvtárnévvel így:
**cd -**

*CDPATH könyvtárak*
Ha a könyvtármeghatározás a
**cd**
parancs és nem a jelenlegi könyvtár, akkor a M-Commander a
**CDPATH**
környezeti változót keresi a könyvtárnevek között.

Például te beállítod a
**CDPATH**
változót az ~/src:/usr/src-re, lehetővé teszi számodra azt, hogy bármely
könyvtárról a fájlrendszeren belül a relatív név használatával bárhonnan
átléphess az ~/src-be, vagy az /usr/src-be bárhonnan (például a
*cd linux*
az /usr/src/linux könyvtárba léptet át).

## Makro helyettesítő <a id="macro-substitution"></a>

Amikor belépsz a
[felhasználói menübe](#edit-menu-file),
vagy a
[társítások parancsot](#edit-extension-file),
futtatod, illetve a parancsot a parancssorból futtatod, a Makró
Helyettesítőt használod.

A makrók:

*%i*
: A space-szel jelölt rész, amely azonos a kurzor oszlop pozíciójával.
Csak menü szerkesztéshez.

*%y*
: A jelenlegi fájl szintaktikájának típusa. Csak menü szerkesztéshez.

*%b*
: A blokk fájl neve.

*%e*
: A hiba fájl neve.

*%m*
: A jelenlegi menu neve.

*%f*
és
*%p*

> A jelenlegi fájl neve.

*%n*
: Csak a jelenlegi fájlnév kiterjesztés nélkül.

*%x*
: A jelenlegi fájl kiterjesztése.

*%d*
: A jelenlegi könyvtár neve.

*%F*
: A jelenlegi fájl a nem kijelölt panelben.

*%D*
: A könyvtár neve a nem kiválasztott panelben.

*%t*
: A jelenleg kijelölt fájlok.

*%T*
: A kijelölt fájlok a nem aktív panelben.

*%u*
és
*%U*

> Azonos a %t és a %T makrókkal, de hozzáadáskor a fájlok nem kerülnek
> kijelölésre. Ezt a makrót csak egyszer használhatod egy menü fájlon
> belül, vagy fájl kiterjesztésben bekezdés esetén, mivel a következő
> alkalommal ezek nem lesznek kijelölt fájlok.

*%s*
és
*%S*

> A kiválasztott, kijelölt fájlok, ha vannak ilyenek. Egyébként a jelenlegi
> fájlok.

*%cd*
: Ez a speciális makro, ami arra használható, hogy a jelenlegi könyvtárat
lecserélhessük az előtte levő könyvtárra. Ezt elsősorban a
[Csatolt fájlrendszernél](#virtual-file-system)
használhatjuk.

*%view*
: Ez a makro használható a belső fájlnéző meghívására. Ez a makro
használható egyedül, vagy kiegészítésekkel is. Ha ezen makro bármely
kiegészítését használod, akkor annak zárójelen belül kell lennie. A
kiegészítések a következők:
*ascii*
a fájlnéző ascii módú használatához;
*hex*
a hex mód használatához; a
*nroff*
mondja meg a fájlnézőnek az nroff a félkövér és az aláhúzás
szekvenciáját; az
*unformated*
mondja meg a fájlnézőnek azt, hogy az nroff paranccsal készített
vastagítása és aláhúzása nem használható.

*%%*
: A % karakter

*%{valamilyen szöveg}*
: Súgó a kiegészítéshez. Beviteli ablak jelenik meg, és a szöveg
magyarázatként jelenik meg. A makró a felhasználó által begépelendő
szöveget helyettesíti. Ezt az ESC, vagy az F10 lenyomásával tudja törölni
a felhasználó.  Ez a makró jelenleg még nem működik a parancssorban.

## A terminál <a id="the-terminal"></a>

A program a shelledet egy ál-terminálban tartja a panelek mögött. A
következő shellekkel működik: bash, ash (BusyBox és Debian), (o/m)ksh, tcsh,
zsh és fish.

A shell az, amelyik a
**SHELL**
változóban meg van adva, és ha az nincs megadva, akkor az, amelyik az
/etc/passwd fájlban szerepel. Ahelyett, hogy minden parancshoz új shellt
indítana, a program a parancsot ennek a shellnek adja át, mintha te gépelted
volna be. Így környezeti változókat lehet állítani, shell-függvényeket
használni és aliasokat megadni, amelyek a programból való kilépésig
érvényesek.

**bash**
: indítóparancsok a ~/.local/share/mc6/bashrc fájlban (egyébként ~/.bashrc),
saját billentyűkiosztás a ~/.local/share/mc6/inputrc fájlban (egyébként
~/.inputrc).

**ash/dash**
: (BusyBox vagy Debian) indítóparancsok a ~/.local/share/mc6/ashrc fájlban
(egyébként ~/.profile).

**ksh/oksh**
: indítóparancsok a ~/.local/share/mc6/kshrc fájlban (egyébként
*ENV*
vagy ~/.profile).

**mksh**
: (MirBSD ksh) indítóparancsok a ~/.local/share/mc6/mkshrc fájlban (egyébként
*ENV*
vagy ~/.mkshrc).

**zsh**
: indítóparancsok a ~/.local/share/mc6/.zshrc fájlban (egyébként ~/.zshrc).

**tcsh, fish**
: egyelőre nincs saját indítófájljuk ehhez a programhoz, csak magának a
shellnek a fájljai érvényesek.

A futó alkalmazást bármikor félre lehet tenni a
**C-o**
billentyűvel, és vissza lehet térni a programhoz. Ha egy parancsot így
szakítottál félbe, addig nem tudsz másik külső parancsot indítani, amíg a
félbeszakított alkalmazás be nem fejeződik.

A panelek mögött a terminál megőrzi mindazt, amit a shell kiírt, és amíg a
panelek félre vannak téve, ez olvasható, kijelölhető és törölhető. A
kurzorbillentyűk a kimenetben járnak, Shifttel pedig kijelölik, mindkettő
addig, amíg maga a terminál kapja a billentyűket; azok a billentyűk,
amelyek csak a képet mozgatják, attól függetlenül működnek, hogy ki gépel.
Minden billentyű, amely nincs alább felsorolva, a shellhez jut.

```
Ctrl-Insert    a kijelölés másolása a vágólapra
Ctrl-Shift-u   a kijelölés megszüntetése
Alt-s          keresés a kimenetben a most begépeltre
Alt-Shift-s    csak az illeszkedő sorok mutatása
Ctrl-l         a képernyő törlése, a kimenet megtartásával
Ctrl-Shift-l   a képernyő és az egész kimenet törlése
               (Ctrl-Alt-l is)
```

Az Alt-s és az Alt-Shift-s ugyanúgy veszi a mintát, mint a panelekben: a
képernyő felső sorában gépelődik, a kimenet pedig követi, ahogy nő. A kis- és
nagybetű nem számít. A keresés a kurzortól lefelé megy, és a legközelebbi
találatot jelöli ki; az újabb Alt-s az alatta levőt, a legújabb sor után
pedig a keresés a legrégebbire fordul. Ahol a parancsértelmező gépel, onnan
még semmit sem olvastak, így a keresés a kimenet legrégebbi sorától indul. A
szűrő csak az illeszkedő sorokat mutatja, és a kurzorbillentyűk már gépelés
közben közöttük járnak; az újabb Alt-Shift-s az alatta levő sorra viszi a
kurzort. A
*search_direction*
beállítás mindkettőt megfordítja, és a keresés felfelé megy, ahogy a
**less**
keres, és ahogy korábban ment. Begépelt minta nélkül
megnyomva mindkettő az előző mintát veszi elő. A Backspace egy karaktert
töröl, az a karakter pedig, amelyre semmi nem illeszkedik, nem kerül be. Az
Enter befejezi a gépelést, és a képet a találaton hagyja, a kijelöléssel
együtt; az Esc befejezi, és visszaadja a gépelés előtti képet: a kurzort oda,
ahol olvastak, vagy a parancssorba, ha semmit sem olvastak, és az addigi
szűrőt meg kijelölést. Minden más billentyű befejezi a gépelést, és azt
teszi, amit egyébként tenne.

Félretett panelek mellett a funkcióbillentyűk többsége a terminálé, és a
gombsor nevezi meg őket. A fájlkezelő megnézés, szerkesztés, másolás,
átnevezés és törlés funkciói ott nincsenek felkínálva: azok a panel kurzora
alatti fájllal dolgoznak, és ez a kurzor nem látszik. Az F8 szándékosan
marad üresen, hogy a törlés felé induló mozdulat inkább ne csináljon semmit,
mint valami mást.
Az F7 könyvtárat hoz létre, a Shift-F4 pedig új fájlt szerkeszt, ugyanúgy,
mint kint a panelekkel: mindkettő a panel könyvtárában dolgozik, és a shell
is abban áll.

```
F2           a kijelölés másolása a vágólapra
F3           az egész kimenet kijelölése, vagy a kijelölés
             megszüntetése
F4           csak a kijelölésre vagy a kurzor alatti szóra
             illeszkedő sorok meghagyása
F5           ennek a szűrőnek a levétele és visszatétele
F6           a képernyő és az egész kimenet törlése
```

Amíg a shell a promptjánál vár, az F1, az F7, a Shift-F4, az F9 és az F10 a
fájlkezelőé marad, és az F1 ezt a szakaszt nyitja meg a súgóban. Amint egy
parancs fut, a képernyő és rajta minden billentyű azé, ezek is. A fenti öt a
kivétel: amíg a parancs dolgozik, azok a terminálé maradnak. A teljes
képernyős alkalmazás, a szerkesztő vagy a lapozó minden billentyűt magának
vesz, ezeket is. Mindegyikük a billentyűkiosztás fájl
**[mcterm]**
szakaszában szerepel, és ott át is állítható.

Ha a shell promptjánál, a félretett panelek mögött, argumentumok nélkül azt
gépeled, hogy
**mcommander**,
a futó program újra megmutatja a paneleit, ahelyett hogy egy második példányt
indítana. Argumentummal, például egy könyvtárnévvel, a korábbi módon egy
beágyazott példány indul.

Az alapértelmezett prompt, amelyet a program mutat,
"felhasználó@gép:útvonal$ " alakú. Olyan shellel, amely erre képes, például a
Bash-sel, ugyanaz a prompt látszik, amelyet egyébként is használsz.

(Ismert hiba a fish esetében: a prompt csak teljes képernyős módban (Ctrl-o)
látszik, a panelek mellett nem.)

Ha a SHELL változótól vagy az /etc/passwd fájlban megadott bejelentkezési
shelltől eltérő shellt akarsz, így indítsd a programot:
**SHELL=/bin/mishell mcommander**

Az
[OPCIÓK](#options)
rész további tájékoztatást ad arról, hogyan lehet a shellt vezérelni.

# Chmod (hozzáférési jogosultság) <a id="chmod"></a>

A Chmod ablak a fájlok, könyvtárak attribútum bitjeinek beállítására
szolgál. A
*C-x c*
billenytűkombinációval is indítható ez a funkció.

A Chmod ablak két részből áll -
*Jogosultság*
és
*Állomány*
(Az Állomány szó itt fájlt, vagy könyvtárnevet jelent).

Az Állomány részben megjelenik a fájlok, illetve könyvtárak neve, és a
hozzáférési jogok nyolcas számrendszerbeli formátumban, úgyanúgy mint a
tulajdonos és a csoport neve.

A Jogosultság részben az Állomány attribútumbitjének megfelelő sorban a
check gombot kell kijelölni. Változtatáskor a nyolcas számrendszerbeli
atribútumbitek megváltozott értékét az Állomány részben láthatod.

A widgetek közötti mozgáshoz (gombok, és check gombok) a
*kurzor billentyűket,*
vagy a
*Tab*
gombot használhatod. A check gombok kijelöléséhez, vagy a gombok
kijelöléséhez használd a
*Space*
gombot. Ezeken kívül még használhatsz gyorsbillentyűket is, a gombok
gyorsabb kiválasztásához (a megjelölt betük a gombokon).

Az attribútum bitek beállítására használd az Enter gombot.

Amikor könyvtárak, vagy fájlok csoportjával dolgozol, csak rá kell
kattintanod a megfelelő bitre annak kijelöléséhez, vagy a kijelölés
törléséhez. Amikor kiválasztottad azokat a biteket, amiket meg akarsz
változtatni, válasz ki egy gombot a művelet gombok közül (Bekapcsol,
vagy Töröl).

Végül, az itt megadott beállításoknak megfelelően állítsuk be
az attribútumokat a
**[Mind]**
gombbal, ami az összes kijelölt fájlon végrehajtja a beállítást.

A
**[Beállít]**
csak a kijelölt attribútumokat állítja be a fájlokhoz.

A
**[Bekapcsol]**
a megjelölt biteket állítja be az összes fájlhoz.

A
**[Töröl]**
a megjelölt attribútum biteket törli a kiválasztott fájloknál.

Az
**[Ok]**
egyetlen fájlhoz állítja be az attributumot.

A
**[Mégsem]**
kilép Chmod parancs módból.

# Chown (Tulajdonos változtatása) <a id="chown"></a>

A Chown parancs a fájl tulajdonos, vagy csoport azonosítójának
beállítására szolgál. A parancs gyorsbillentyűje a C-x o.

# Haladó (bővített) Chown <a id="advanced-chown"></a>

A Haladó Chown parancs a
[Chmod](#chmod)
és a
[Chown](#chown)
parancsok kombinációja egyetlen ablakban. Egyszerre tudod megváltoztatni
a fájlok jogosultságait, és tulajdonos, vagy csoport azonosítóját.

# Fájl műveletek <a id="file-operations"></a>

Amikor fájlokat másolsz, mozgatsz, vagy törölsz, a M-Commander a
Fájl műveletek dialógus ablakot jeleníti meg. Majd megjeleníti az
elkezdett művelet fájljait, ahol jobbára három folyamatsávot jelenít
meg. A fájl sáv azt mutatja meg, hogy a kijelölt fájlok közül a jelenleg
másolt fájlnak mekkora részén hajtotta végre a művelet. A Darab sáv azt
mutatja meg, hogy a kijelölt fájlok közül hányat dolgozott fel eddig. A
bájt sáv azt mutatja meg, hogy a kijelölt fájlok teljes méretének
mekkora része került már átmásolásra. Ha a Részletes műveletinfó ki van
kapcsolva, a fájl és a bájtok sáv nem jelenik meg. Két gomb található a
dialógus ablak alján. A Következő gomb lenyomásával át tudod lépni a
jelenlegi fájlt. A Megszakít gomb megszakítja a műveletet, a fájlok
visszamaradó részével nem történik semmi.

Van három másik dialógus ablak is, amelyekkel a fájl műveleteknél
találkozhatsz.

A hiba dialógus ablak a hiba körülményeiröl értesít bennünket, és három
választási lehetőséget tartalmaz. Normálisan a Következő gomb, amellyel
átlépheted a jelenlegi fájlt, vagy a Megszakít gomb, amellyel
megszakíthatod a további műveleteket, között választhatsz. Választhatod
még az Újra gombot is, ha egy másik virtuális terminálról ki tudtad
javítani a hibát.

### A fájl felülírása <a id="replace"></a>

Ez a párbeszédablak akkor jelenik meg, ha másolással vagy áthelyezéssel egy
már létező fájlt írnál felül. Az ablak megmutatja mindkét fájl dátumát és
méretét, és a következő gombokat kínálja:

**[Igen]**
: felülírja a fájlt.

**[Nem]**
: kihagyja a fájlt.

**[Hozzáfűz]**
: a forrásfájlt a célfájl végéhez fűzi.

**[Folytat]**
: a forrásfájl hátralevő részét fűzi a célfájlhoz. Ez a gomb csak akkor
látszik, ha a célfájl mérete nem nulla, és kisebb a forrásénál.

**[Mind]**
: minden fájlt felülír.

**[Frissít]**
: akkor ír felül, ha a forrásfájl újabb a célfájlnál.

**[Egyik sem]**
: egyetlen fájlt sem ír felül.

**[Kisebb]**
: akkor ír felül, ha a forrásfájl kisebb a célfájlnál.

**[Eltérő méretű]**
: az eltérő méretű fájlokat írja felül.

**[Megszakít]**
: az egész műveletet megszakítja.

Ha a
**Ne írja felül nulla hosszúságú fájllal**
jelölőnégyzet be van kapcsolva, a nulla méretű forrásfájl nem írja felül a
nem nulla méretű célfájlt.

A rekurzív törlés párbeszédablaka akkor jelenik meg, ha nem üres könyvtárat
akarsz törölni. A gombjai:

**[Igen]**
: a könyvtárat a tartalmával együtt törli.

**[Nem]**
: kihagyja a könyvtárat.

**[Mind]**
: minden könyvtárat töröl.

**[Egyik sem]**
: minden nem üres könyvtárat kihagy.

**[Megszakít]**
: az egész műveletet megszakítja.

Ha vannak kijelölt fájlok, csak azoknak a kijelöltsége szűnik meg, amelyeken
a művelet sikerült. A kihagyott és a sikertelen fájlok kijelöltek maradnak.

# Kijelölt fájlok másolása vagy áthelyezése <a id="mask-copyrename"></a>

A másolás, vagy mozgatás művelet a fájlok átnevezésének legegyszerűbb
módja. Ennek elvégzéséhez meg kell határoznod a megfelelő forrás
maszkot, és általában a cél részben a rendeltetés szerinti maszknak
megfelelően. Minden forrás maszkkal azonos fájl átmásolásra vagy
átnevezésre kerül a cél maszknak megfelelően. Ha vannak kijelölt fájlok,
csak a kijelölt fájlokra vonatkozik a átnevezett forrás maszk.

Ezen kívül vannak egyéb opciók is, amiket még beállíthatsz:

A link követés megadja vajon symlink, vagy hardlink készült-e a forrás
könyvtárban (rekurzívan belső könyvtáraknál), és új linket kell-e
csinálni a cél könyvtárba, vagy csak át kell másolnod a bejegyzést.

"Létező könyvtárba belép" megmondja azt, hogy történjék, ha azonos nevű
célkönyvtár létezik, mint amelyet elkezdtünk másolni. Az alapértelmezett
művelet ilyenkor az, hogy ebbe a könyvtárba történő átmásoláskor,
lehetővé teszi számodra azt, hogy a forrás könyvtárat átmásold. Talán
egy példa segíteni fog:

Neked a foo könyvtár tartalmát át kell másolnod a /bla/foo könyvtárba,
amely már létezik. Normálisan (amikor az Ugrás (Dive) nincs beállítva),
az mcommander be fogja másolni ezt a /bla/foo könyvtárba. Az opció
engedélyezésekor a bejegyzéseket a /bla/foo/foo könyvtárba fogod
másolni, mivel a könyvtár már létezik.

Az "Attributumok megőrzése" megmondja azt, vajon az eredeti fájlok
jogosultságait, időadatait, és (ha root vagy) az eredeti fájlok UID és
GID értéekit. Ha ez az opció nincs beállítva, az umask jelenlegi értékét
fogja használni a funkció.

**Shell kifejezések be**

Amikor a "Shell mintát használ" kifejezések opció be van kapcsolva, a
forrás maszkhoz használhatsz használhatod a '\*' és a '?' maszkokat. Ezek
használhatóak a shellben is. A cél maszkhoz csak '\*' és '\\\<szám>' maszk
használható. A célmaszk első '\*' maszkja megfelel a forrás maszk első
maszk csoportjának, a második '\*' megfelel a második csoportnak, és így
tovább. A '\\1' maszk megfelel a forrás maszk első maszk csoportjának, a
'\\2' maszk megfelel a második csoportnak, és ez így megy '\\9'-ig. A
'\\0' maszk jelentése: a forrás fájl teljes neve.

Két példa:

Ha a forrás maszk, "\*.tar.gz" a rendeltetésé a "/bla/\*.tgz", a másolandó
fájl a "foo.tar.gz", - a másolat a "/bla" könyvtárban található
"foo.tgz" lesz.

Tételezzük fel azt, hogy fel akarod cserélni a fájlnevet a
kiterjesztéssel, például a "file.c"-t a "c.file"-lal, és a többit. Ennek
a forrásmaszkja a "\*.\*" lesz, a rendeltetésé pedig a "\\2.\\1".

**Shell kifejezések ki**

Amikor a shell kifejezések opció ki van kapcsolva az M-Commander a továbbiakban
nem csoportosít automatikusan. Az '\\(...\\)' kiegészítést kell
használnod a forrásmaszkban, a célmaszk specifikációnak meagadásához. Ez
jóval gördülékenyebb módszernel tűnik, de több gépelést igényel.
Egyébként a cél maszk használata egyszerűbb, ha a Shell kifejezések
opció be van kapcsolva.

**Kisbetű-nagybetű csere"**

Magváltoztathatod a fájlnév betűnagyságát. Ha a '\\u'-t, vagy a '\\l'-t
használod a cél maszkban, a következő karaktert naggyá, vagy kicsivé
konvertálja.

Ha a '\\U'-t, vagy '\\L'-t használod a cél maszkban a következő
karakterek naggyá, illetve kicsivé fognak változni egészen addig, amíg
'\\E', vagy '\\U', '\\L', vagy a fájlnév vége következik.

Az '\\u' és '\\l' erősebb az '\\U'-nál és az '\\L'-nél.

Például, ha a forrás maszk '\*' (Shell kifejezés be van kapcsolva), vagy
'^\\(.\*\\)$' (Shell kifejezés ki van kapcsolva) és a célmaszk '\\L\\u\*'
a fájl nevek eleje nagybetűsre konvertálódik, a többi betű pedig
kicsire.

Ezeken kívül még használhatod a '\\' karaktert, mint hivatkozó
karaktert. Például a  '\\\\'-t a backslash-hez és a '\\\*'-et a
csillaghoz.

# Kiegészítés <a id="completion"></a>

A M-Commander begépeli neked a kívánt szöveget.

Megkísérli kiegészíteni a szöveget a jelenlegi pozíciótól. Az M-Commander
kiegészíti a szöveget (ha a szöveg
**$**-ral
kezdődik), felhasználónevet (ha a szöveg
**~**-vel
kezdődik), hostnevet (ha a szöveg
**@**-lel
kezdődik), vagy parancsot (ha a parancssor azon részén állsz, ahova
a parancsot kell begépelni, megjeleníti a kiegészítést, ha a shell
tartalmazza a szót, és az a shell beépített parancsa). Ha ezek közül
egyik sem egyezik, akkor fájlnév kiegészítéssel próbálkozik.

A fájlnév, felhasználónév, változó és hostnév kiegészítése működik az
összes beviteli sorban, a parancskiegészítés csak a parancssorban. Ha
a kiegészítés kétértelmű (több érték megjelenítése lehetséges), a M-Commander
hangjelzést ad és a
[Beállítások](#configuration)
dialogbox
*kiegészítés: összes*
opciójának megfelelően hajtja végre a további műveleteket. Ha ez az
opció be van állítva, az összes megjeleníthető elem egy listában jelenik
meg a jelenlegi pozíciótól kezdődően, a fel-le nyilak segítségével, és
az
**Enter**-rel
tudod kiegészíteni a bejegyzésed. Ezen kívűl, még begépelhetsz az első
helyre akkor, amikor a listában megjelenített összes kiegészítés eltér
az általad kívánttól. Ha újra lenyomod a
**M-Tab**-ot,
egy listarészlet jelenik meg a listában, egyébként pedig csak az első
egyező elem, amely az összes kijelölt karakterrel egyezik. Hamarosan a
kétértelműség meg fog szűnni, a dialógus ablak eltűnik, amit az
**Esc**
**F10**
és a bal, illetve a jobb nyíl billentyűkkel is megtehetsz. Ha a
[kiegészítés: összes](#configuration)
nincs beállítva, a dialógus ablak csak a
**M-Tab**
második lenyomására jelenik meg, az első lenyomáskor, az M-Commander csak
hangjelzést ad.

# Csatolt (látszólagos) fájlrendszer <a id="virtual-file-system"></a>

A M-Commander egy kódréteggel éri el a fájlrendszert; ezt a réteget a csatolt
fájlrendszerek váltójának nevezzük. Ezzel a program olyan fájlokkal is
dolgozhat, amelyek nem a Unix fájlrendszerén vannak.

A
*local*
fájlrendszeren, vagyis a szokásos Unix fájlrendszeren kívül két csatolt
fájlrendszer van beépítve:
*extfs,*
amely egy fájlt vagy a rendszer egy listáját saját szkripttel könyvtárfaként
mutatja meg, és
*sfs,*
amely egyetlen fájlt átenged egy parancson, és azt mutatja, ami kijön. Minden,
ami másik géphez való kapcsolatot kíván, és az archívumok is, már
[panel bővítmények](#panel-plugins),
nem a váltó fájlrendszerei.

A váltó minden útvonalat értelmez, és a megfelelő fájlrendszernek adja át; az
egyes fájlrendszerek névalakját a saját szakaszuk írja le.

## Panel bővítmények <a id="panel-plugins"></a>

A panel nincs fájlrendszerhez kötve: egy bővítmény bármivel megtöltheti, amit
fel tud sorolni. A programmal a következők érkeznek:

```
arcmc        archívumok és a tartalmuk
ftp, sftp    fájlok másik gépen
shell-link   fájlok másik gépen ssh fölött
samba        egy SMB kiszolgáló megosztásai
s3           egy S3 tároló vödrei
git          egy verziókezelt könyvtár állapota
docker       konténerek, képek és a naplóik
k8s          egy fürt objektumai
mongo        egy adatbázis gyűjteményei
sqlite       egy adatbázis táblái
systemd      a rendszer egységei
panelize     egy parancs eredménye panelként
mcpeek       bepillantás egy fájlba
mcstruct     bináris fájl megnevezett mezők fájaként
skineditor   a program kinézete
```

Minden bővítmény hozza a saját súgóját, amelyet az
**F1**
nyit meg a paneljében vagy a párbeszédablakában. A Beállítások menü
**Bővítmények kezelése**
pontja felsorolja, mi van betöltve, kikapcsol egy bővítményt és megnyitja a
beállításait. A bővítmény panelje a
[bal és jobb oldali menüből](#left-and-right-menus),
a gyorslistából vagy a bővítmény címének a parancssorba írásával nyílik meg.

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

## Egyetlen fájl fájlrendszere <a id="single-file-filesystem"></a>

Az
**sfs**
egyetlen fájlt átenged egy parancson, és az eredményt önálló fájlként mutatja
meg; így olvasható egy tömörített fájl anélkül, hogy kézzel kicsomagolnánk. A
fájlrendszer neve a fájl neve után kerül, akárcsak az extfs esetében:

```
  cd documents.gz/ugz://
```

A parancsok a
**{{sysconfdir}}/mcommander/sfs.ini**
fájlban vannak, soronként egy: a fájlrendszer neve, egy perjel, a parancs
száma, egy tabulátor, majd maga a parancs, ahol a
*%1*
az a fájl, amelyen a panel áll, a
*%3*
pedig az, amelybe írni kell. A programmal érkező fájl a gz, bz2, lz, lz4,
lzma, lzo, xz és zst tömörítő és kicsomagoló párjait tartalmazza, és még
néhányat.

# Fájlattribútumok <a id="chattr"></a>

Ez az ablak egy fájl- és könyvtárcsoport attribútumainak megváltoztatására
való Linux fájlrendszeren. A C-x e billentyűvel nyitható meg.

Nem minden fájlrendszer ismer minden attribútumot. Az elérhető attribútumok
jelölőnégyzetek halmazaként jelennek meg (a részleteket lásd a
**chattr(1)**
lapon). Ahogy a jelölőnégyzetek változnak, a fájlnév alatti jelöléssorozat is
velük változik.

Az ablak elemei között a
*kurzorbillentyűkkel*
vagy a
*Tab*
billentyűvel lehet mozogni. A jelölőnégyzet állapotát és a gombok
kiválasztását a
**szóköz**
végzi.

Az attribútumok beállítása az Enterrel történik.

Fájl- vagy könyvtárcsoportnál elég bejelölni azokat az attribútumokat,
amelyeket be akarsz kapcsolni vagy törölni akarsz, majd a műveleti gombok
közül választani (Jelöltek beállítása vagy Jelöltek törlése).

**[Mind beállít]**
: pontosan a megadott attribútumokat állítja be az összes kijelölt fájlon.

**[Mind jelölt]**
: csak a bejelölt attribútumokat állítja be az összes kijelölt fájlon.

**[Jelöltek beállítása]**
: bekapcsolja a bejelölt attribútumokat a kijelölt fájlokon.

**[Jelöltek törlése]**
: kikapcsolja a bejelölt attribútumokat a kijelölt fájlokon.

**[Beállít]**
: egyetlen fájl attribútumait állítja be.

**[Mégsem]**
: kilép a parancsból.

# Képernyőválasztó <a id="screen-selector"></a>

A program több belső részt (szerkesztő, fájlnéző, összehasonlító) is tud
egyszerre futtatni, és a megnyitott fájlok bezárása nélkül lehet köztük
váltani. Több fájlkezelő egyidejű használata egyelőre nem támogatott.

Nevezzük képernyőnek mindegyik ilyen részt. Háromféleképpen lehet köztük
váltani, ezekkel a mindenhol érvényes billentyűkkel:

**Alt-}**
: a következő képernyőre vált;

**Alt-{**
: az előző képernyőre vált;

**Alt-\`**
: megnyitja a megnyitott képernyők listáját (vagy a menü "Képernyők listája"
pontjával).

# Fájlok kijelölése <a id="selectunselect-files"></a>

A
**+**
és a
`\`
billentyű mintát kér, és kijelöli vagy leveszi a kijelölést azokról a
fájlokról, amelyekre a minta illik; az
**\***
megfordítja a kijelölést. A párbeszédablak megjegyzi, mit kértek utoljára,
és megkérdezhető, hogy a minta shell minta legyen-e, számít-e a kis- és
nagybetű, és vonatkozzon-e a könyvtárakra is.

# Panel módok <a id="panel-modes"></a>

A panel módja egy megnevezett, újra felhasználható listaforma. A módok listája
közös a két panel között.

Az
**Alt-t**
(és a bal és jobb oldali menü
**Panel módok...**
pontja) a
**váltót**
nyitja meg: a megadott módok listáját. Az Enter a kurzor alatti módot
alkalmazza a panelre, az Esc érintetlenül hagyja.

A
**Beállítások**
menü
**Fájlpanel módok...**
pontja a
**kezelőt**
nyitja meg: ugyanazt a listát, billentyűkkel szerkesztve. Az
**Insert**
új módot hoz létre, az
**F4**
(vagy az
**Enter**)
szerkeszti a kijelöltet, az
**F5**
lemásolja, a
**Delete**
(vagy az
**F8**)
pedig törli. Az
**Alapértelmezés**
gomb a beépített módokra cseréli a listát, az
**OK**
menti, a
**Mégsem**
(vagy az
**Esc**)
eldobja az ablakban végzett összes változtatást.

A kezelő a módok közös listáját szerkeszti; egyik panel módját sem váltja át.

A módszerkesztőben külön mező tartozik az oszlopok mezőtípusaihoz és
szélességeihez, és külön a mini-állapotsorhoz, a
[Fájllista...](#listing-format)
résznél leírt mezőnevekkel
\. A típuslista vesszővel tagolt, oszloponként egy tétel; egy oszlop több,
szóközzel elválasztott mezőt is tartalmazhat (például
**type name**).
A 0 (vagy üres) szélesség a mező automatikus szélességét hagyja meg.
Teljes formátumszöveget is be lehet illeszteni a típusmezőbe (például
**half name | size:7**):
a
**|**
elválasztók és a
**:szélesség**
utótagok ilyenkor szétosztódnak a két lista között.

A megadott módok és az, amelyiket az egyes panelek használják, megmaradnak a
munkamenetek között.

# Reguláris kifejezések gyors áttekintése <a id="regex-quick-reference"></a>

**Gyakori elemek**

```
Egy karakter ezek közül: a, b, c        [abc]
Egy karakter, de nem a, b vagy c        [^abc]
Egy karakter az a-z tartományból        [a-z]
Egy karakter az a-z tartományon kívül   [^a-z]
Egy karakter a-z vagy A-Z közül         [a-zA-Z]
Bármelyik karakter                      .
Vagylagos: a vagy b                     a|b
Bármely üres karakter                   \s
Bármi, ami nem üres karakter            \S
Bármely számjegy                        \d
Bármi, ami nem számjegy                 \D
Bármely szókarakter                     \w
Bármi, ami nem szókarakter              \W
Nem rögzítő csoport                     (?:...)
Rögzítő csoport                         (...)
Nulla vagy egy a                        a?
Nulla vagy több a                       a*
Egy vagy több a                         a+
Pontosan 3 a                            a{3}
3 vagy több a                           a{3,}
3 és 6 közötti számú a                  a{3,6}
A szöveg eleje                          ^
A szöveg vége                           $
Szóhatár                                \b
Nem szóhatár                            \B
```

**Horgonyok**

```
A találat eleje                         \G
A szöveg eleje                          ^
A szöveg vége                           $
A szöveg eleje                          \A
A szöveg vége                           \Z
A szöveg abszolút vége                  \z
Szóhatár                                \b
Nem szóhatár                            \B
```

**Általános elemek**

```
Soremelés                               \n
Kocsivissza                             \r
Tabulátor                               \t
Nulla karakter                          \0
```

**Metasorozatok**

```
Bármelyik karakter                      .
Vagylagos: a vagy b                     a|b
Bármely üres karakter                   \s
Bármi, ami nem üres karakter            \S
Bármely számjegy                        \d
Bármi, ami nem számjegy                 \D
Bármely szókarakter                     \w
Bármi, ami nem szókarakter              \W
Unicode sorozat, sortörésekkel          \X
Unicode sortörések                      \R
Minden, kivéve a sortörést              \N
Függőleges üres karakter                \v
A \v tagadása                           \V
Vízszintes üres karakter                \h
A \h tagadása                           \H
A találat törlése                       \K
A # sorszámú részminta                  \#
X Unicode tulajdonság                   \pX
Unicode tulajdonság vagy kategória      \p{...}
A \pX tagadása                          \PX
A \p{...} tagadása                      \P{...}
Idézet: betű szerint veendő             \Q...\E
A 'név' nevű részminta                  \k{name}
A 'név' nevű részminta                  \k<name>
A 'név' nevű részminta                  \k'name'
Az n. részminta                         \gn
Az n. részminta                         \g{n}
Az n. korábbi relatív részminta         \g{-n}
Az n. rögzítő csoport kifejezése        \g<n>
A köv. n. rögzítő csoport kifejezése    \g<+n>
Az n. rögzítő csoport kifejezése        \g'n'
A köv. n. részminta kifejezése          \g'+n'
Megnevezett rögzítő csoport             \g{letter}
Megnevezett csoport kifejezése          \g<letter>
Megnevezett csoport kifejezése          \g'letter'
YY hexadecimális karakter               \xYY
YYYY hexadecimális karakter             \x{YYYY}
ddd oktális karakter                    \ddd
Y vezérlőkarakter                       \cY
Backspace karakter                      [\b]
Bármely karaktert betű szerintivé tesz  \
```

**Ismétlésjelek**

```
Nulla vagy egy a                        a?
Nulla vagy több a                       a*
Egy vagy több a                         a+
Pontosan 3 a                            a{3}
3 vagy több a                           a{3,}
3 és 6 közötti számú a                  a{3,6}
Mohó ismétlésjel                        a*
Lusta ismétlésjel                       a*?
Birtokos ismétlésjel                    a*+
```

**Karakterosztályok**

```
Egy karakter ezek közül: a, b, c        [abc]
Egy karakter, de nem a, b vagy c        [^abc]
Egy karakter az a-z tartományból        [a-z]
Egy karakter az a-z tartományon kívül   [^a-z]
Egy karakter a-z vagy A-Z közül         [a-zA-Z]
Betűk és számjegyek                     [[:alnum:]]
Betűk                                   [[:alpha:]]
ASCII kódok 0-127                       [[:ascii:]]
Csak szóköz vagy tabulátor              [[:blank:]]
Vezérlőkarakterek                       [[:cntrl:]]
Tízes számjegyek                        [[:digit:]]
Látható karakterek (szóköz nélkül)      [[:graph:]]
Kisbetűk                                [[:lower:]]
Látható karakterek                      [[:print:]]
Látható írásjelek                       [[:punct:]]
Üres karakterek                         [[:space:]]
Nagybetűk                               [[:upper:]]
Szókarakterek                           [[:word:]]
Hexadecimális számjegyek                [[:xdigit:]]
Szó eleje                               [[:<:]]
Szó vége                                [[:>:]]
```

**Jelzők és módosítók**

```
Többsoros                               m
Kis- és nagybetű nem számít             i
Üres karakterek mellőzése / bőbeszédű   x
Egysoros                                s
Unicode                                 u
eXtra                                   X
Nem mohó                                U
Horgony                                 A
Ismétlődő csoportnevek                  J
Nem rögzítő csoportok                   n
Minden üres mellőzése / bőbeszédű       xx
```

**Csoportszerkezetek**

```
Nem rögzítő csoport                     (?:...)
Rögzítő csoport                         (...)
Atomi csoport (nem rögzítő)             (?>...)
A részminta sorszámának törlése         (?|...)
Megjegyzéscsoport                       (?#...)
Megnevezett rögzítő csoport             (?'name'...)
Megnevezett rögzítő csoport             (?<name>...)
Megnevezett rögzítő csoport             (?P<name>...)
Soron belüli módosítók                  (?imsxUJnxx)
Helyi soron belüli módosítók            (?imsxUJnxx:...)
Feltételes szerkezet                    (?(1)yes|no)
Feltételes szerkezet                    (?(R)yes|no)
Rekurzív feltételes szerkezet           (?(R#)yes|no)
Feltételes szerkezet                    (?(R&name)yes|no)
Előretekintő feltétel                   (?(?=...)yes|no)
Visszatekintő feltétel                  (?(?<=...)yes|no)
Az egész minta rekurzív hívása          (?R)
Az 1. rögzítő csoport kifejezése        (?1)
Az első relatív rögzítő csoport         (?+1)
Megnevezett csoport kifejezése          (?&name)
A 'név' nevű részminta                  (?P=name)
A '{név}' csoport kifejezése            (?P>name)
Minták megadása használat előtt         (?(DEFINE)...)
Pozitív előretekintés                   (?=...)
Negatív előretekintés                   (?!...)
Pozitív visszatekintés                  (?<=...)
Negatív visszatekintés                  (?<!...)
Betűs körbetekintő állítások            (*pla:...)
Nem atomi körbetekintő állítás          (*non_atomic_positive_lookahead:...)
Egységes írásrendszer állítás           (*script_run:...)
Egységes írásrendszer (rövid)           (*sr:...)
Vezérlőszó                              (*ACCEPT)
Vezérlőszó                              (*FAIL)
Vezérlőszó                              (*MARK:NAME)
Vezérlőszó                              (*COMMIT)
Vezérlőszó                              (*PRUNE)
Vezérlőszó                              (*SKIP)
Vezérlőszó                              (*THEN)
```

# Színek <a id="colors"></a>

A M-Commander megpróbálja megállapítani azt, hogy a terminál
amelyet használsz, támogatja-e a színhasználatot a terminál adatbázis és
a terminál név segítségével. Néha ez összezavarodhat, ezért
előfordulhat, hogy neked kell megmondanod azt, hogy színes, vagy
színtelen módot használjon a -c illetve a -b kiegészítéssel.

Ha a programot a S-Lang képernyő kezelővel fordították az ncurses
helyett, szintén le fogja ellenőrizni a
**COLORTERM**
változó értékét, ha be van állítva, ez olyan hatású, mintha a -c flaggal
indítottál volna.

Magadhatod azt a terminálnak, hogy mindíg a színes módot használja a
Colors részben
*color_terminals*
változónál az indító fájlban. Így a terminál színtámogatásának vizsgálatát
a M-Commander nem végzi el. Például:

```
[Colors]
color_terminals=linux,xterm
color_terminals=terminal-name1,terminal-name2...
```

A program mindkét opcióval fordítható (ncurses és S-Lang).  Az ncurses nem
jelent feltétlenül színes üzemmódot; csak a terminál adatbázist használja.

# Skinek <a id="skins"></a>

A program külsejét meg lehet változtatni. Ehhez olyan fájlt kell megadni,
amely a színek és a keretek rajzolásához használt vonalak leírását
tartalmazza. A színek újradefiniálása teljesen megfelel annak, amit a
[Színek](#colors)
rész ír le.

Ha a skin valódi színeket (true-color) is megad, a [skin] szakaszban a
'truecolors' kulcsot TRUE értékre kell állítani. Ha nem valódi színt, hanem
256 színt használ, akkor helyette a '256colors' kulcsot.

A skin-fájlt a program a következő sorrendben keresi (az elsőig, amelyet
megtalál):

```
1) parancssori kapcsoló -S <skin>, --skin=<skin>
2) MC_SKIN környezeti változó
3) a [Midnight-Commander] szakasz skin paramétere
4) a {{sysconfdir}}/mcommander/skins/default.ini fájl
5) a {{pkgdatadir}}/skins/default.ini fájl
```

A parancssori kapcsoló, a környezeti változó és a konfigurációs fájlbeli
paraméter a skin-fájl abszolút útvonalát is tartalmazhatja (.ini
kiterjesztéssel vagy anélkül). A keresés itt történik (az elsőig, amelyet
megtalál):

```
1) ~/.local/share/mc6/skins/
2) {{sysconfdir}}/mcommander/skins/
3) {{pkgdatadir}}/skins/
```

A skin-fájlok formátumát a
**{{pkgdatadir}}/skins/README.txt**
írja le.

# Fájlnevek kiemelése <a id="filenames-highlight"></a>

Az aktuális skin-fájl [filehighlight] szakasza a kiemelési csoportok nevét
tartalmazza kulcsként, az értékek pedig színpárok.

A fájlnevek kiemelésének szabályai a {{pkgdatadir}}/filehighlight.ini fájlban
állnak (~/.config/mc6/filehighlight.ini). Az itteni szakaszok nevének meg
kell egyeznie a (skin-fájlbeli) [filehighlight] szakasz paramétereinek
nevével.

A csoportokban használható kulcsok:

*type*
: a fájl típusa. Ha szerepel, a program a többi beállítást nem veszi
figyelembe.

*regexp*
: reguláris kifejezés. Ha szerepel, az 'extensions' beállítás nem számít.

*extensions*
: a fájlkiterjesztések listája, ';' jellel elválasztva.

*extensions_case*
: (csak az 'extensions' beállítással együtt van értelme) az 'extensions'
szabály megkülönbözteti-e a kis- és a nagybetűt (true), vagy nem (false).

A 'type' kulcs értékei a következők lehetnek:

```
- FILE (minden fájl)
  - FILE_EXE
- DIR (minden könyvtár)
  - LINK_DIR
- LINK (minden link a törött linkek kivételével)
  - HARDLINK
  - SYMLINK
- STALE_LINK
- DEVICE (minden eszközfájl)
  - DEVICE_BLOCK
  - DEVICE_CHAR
- SPECIAL (minden speciális fájl)
  - SPECIAL_SOCKET
  - SPECIAL_FIFO
  - SPECIAL_DOOR
```

# Külső szerkesztő vagy fájlnéző paraméterei <a id="parameters-for-external-editor-or-viewer"></a>

A program módot ad arra, hogy a külső szerkesztőkhöz és fájlnézőkhöz
kapcsolókat adjunk meg. Az "[External editor or viewer parameters]" szakaszt
először a rendszerszintű inicializáló fájlban (a program könyvtárában levő
defaults.ini fájlban), majd a ~/.config/mc6/ini fájlban keresi. A beállítás
neve a külső szerkesztő vagy fájlnéző neve (teljes útvonala) legyen. Az
értékében a következő változók használhatók:

*%filename*
: a szerkesztendő vagy megnézendő fájl neve.

*%lineno*
: az a sor, amelynél a fájl megnyílik.

Például:

```
[External editor or viewer parameters]
    vi=%filename +%lineno
    joe=%filename +%lineno
    more=%filename +%lineno
```

A kezdősort a program csak akkor adja át a külső szerkesztőnek vagy
fájlnézőnek, ha az a
[Fájl keresés](#find-file)
eredményablakából indul.

Ha a külső szerkesztő vagy fájlnéző az F4, illetve az F3 billentyűvel indul,
a program arra számít, hogy a program (legalábbis a "joe", de valószínűleg
más is) maga nyitja meg a fájlt ott, ahol utoljára járt. A program nem
akadályozza meg a külső szerkesztőt vagy fájlnézőt abban, hogy a megnyitott
fájlokban a pozíciót elmentse és visszaállítsa.

# Speciális Beállítások <a id="special-settings"></a>

A legtöbb beállítás a menükből is elérhető. Van azonban néhány, amelyet csak
a beállításfájl szerkesztésével lehet megváltoztatni.

Ezeket a változókat a ~/.config/mc6/ini fájlban lehet megadni:

*clear_before_exec*
: Alapértelmezésben a program törli a képernyőt, mielőtt parancsot futtatna.
Ha a parancs kimenetét a képernyő alján szeretnéd látni, írd át a
~/.config/mc6/ini fájlban a clear_before_exec értékét 0-ra.

*confirm_view_dir*
: Ha könyvtáron nyomsz F3-at, a program rendes körülmények között belép a
könyvtárba. Ha ez az érték 1, akkor kijelölt fájlok esetén megerősítést kér,
mielőtt könyvtárat váltana.

*vfs_timeout*
: A virtuális fájlrendszer gyorstárának élettartama másodpercben. Archívumból
vagy tömörített fájlból kilépve a beolvasott lista és a kicsomagolt ideiglenes
fájl ennyi ideig megmarad, hogy a visszalépés azonnali legyen, azután
felszabadul. Alapértelmezés szerint 60; a 0 azonnal felszabadítja.

*only_leading_plus_minus*
: A '+', '-' és '\*' karaktert csak akkor kezeli külön a parancssorban
(kijelölés, kijelölés megszüntetése, megfordítása), ha a parancssor üres. Így
a parancssor közepén nem kell idézőjelbe tenni őket, viszont nem üres
parancssornál nem használhatók a kijelölés változtatására.

*alternate_plus_minus*
: Bekapcsolva a '+', a '-', a '\\' és a '\*' billentyű a szokásos módon
működik. Kijelölésre és a kijelölés megszüntetésére ilyenkor az 'Alt-+', az
'Alt--' és az 'Alt-\*' való.

*show_output_starts_shell*
: Ha a C-o billentyűvel a felhasználói képernyőre lépsz vissza, és ez be van
kapcsolva, új shellt kapsz. Egyébként bármely billentyű visszahoz a
programhoz.

*timeformat_recent*
: A hat hónapnál nem régebbi dátumok megjelenítési formája. A leírását lásd a
strftime vagy a date kézikönyvlapján. Ha ez a beállítás hiányzik, az
alapértelmezett forma érvényes.

*timeformat_old*
: A hat hónapnál régebbi vagy jövőbeli dátumok megjelenítési formája. A
leírását lásd a strftime vagy a date kézikönyvlapján. Ha ez a beállítás
hiányzik, az alapértelmezett forma érvényes.

*use_file_to_guess_type*
: Bekapcsolva (ez az alapértelmezés) a program a file parancsot hívja, hogy
megállapítsa a
[társítások fájljában](#edit-extension-file)
felsorolt típusokat.

*xtree_mode*
: Bekapcsolva (alapértelmezés szerint ki van kapcsolva), ha a fájlrendszert a
Fa panelben böngészed, a másik panel magától a kijelölt könyvtár tartalmát
mutatja.

*shell_directory_timeout*
: A könyvtár-gyorstár egy bejegyzésének élettartama másodpercben. Az
alapértelmezett érték 900 másodperc.

*clipboard_store*
: Egy külső vágólapkezelő útvonala (kapcsolókkal együtt), például az 'xclip',
amely fájlból olvas szöveget az X kijelölésébe. Például:

<!-- -->

```
clipboard_store=xclip -i
```

*clipboard_paste*
: Egy külső vágólapkezelő útvonala (kapcsolókkal együtt), például az 'xclip',
amely a kijelölést a szabványos kimenetre írja. Például:

<!-- -->

```
clipboard_paste=xclip -o
```

*autodetect_codeset*
: Ezzel a beállítással a program az 'enca' paranccsal állapítja meg a
szövegfájlok kódlapját a belső fájlnézőben és a szerkesztőben. Az érvényes
értékek listáját az 'enca --list languages | cut -d : -f1' parancs adja meg.
A beállításnak a [Misc] szakaszban kell állnia.

Például:

```
autodetect_codeset=russian
```

A belső fájlnéző beállításai ugyanennek a fájlnak a [Viewer] szakaszában
vannak. Mindegyikük megtalálható a
[Megjelenítő beállításai](mview.md#viewer-options)
ablakban is; az itteni nevek azok, amelyeket az az ablak ír ki.

*wrap*
: A képernyőnél szélesebb sort a következő képernyősorba töri.
Alapértelmezés szerint be van kapcsolva.

*syntax*
: A szöveget a szerkesztő szintaxisszabályai szerint színezi.
Alapértelmezés szerint ki van kapcsolva.

*mouse_move_pages*
: Az egérrel való gördítés lapokban történik, nem soronként. ASCII módban a
bal gomb szöveget jelöl ki, ezért ott ez a gördítés a jobb vagy a középső
gombbal megy. Alapértelmezés szerint be van kapcsolva.

*remember_file_position*
: A fájlt ott nyitja meg, ahol legutóbb abbamaradt. Alapértelmezés szerint ki
van kapcsolva.

*structured_auto*
: A támogatott fájlokat (json, yaml, yml, xml, html, htm) rögtön a szerkezeti
(fa) módban nyitja meg. Ha a fájl nem elemezhető, szó nélkül a sima szöveges
képet használja. Alapértelmezés szerint ki van kapcsolva.

*eof*
: A fájl utolsó sora után kiírt szöveg. Alapértelmezés szerint üres.

*structured_max_size*
: A legnagyobb fájl, amelyet a szerkezeti (fa) nézet feldolgoz, bájtban. A
nagyobbat olvasás előtt visszautasítja. Alapértelmezés szerint 67108864
(64 MB).

*structured_max_nodes*
: A legnagyobb fa, amelyet a szerkezeti nézet felépít, csomópontokban. A sűrű
dokumentum, például az apró címkékből álló XML, előbb ér ehhez a korláthoz,
mint a mérethez: nagyjából tizenkét bájtonként fogyaszt egy csomópontot, és
minden csomópont memóriába kerül. Alapértelmezés szerint 10000000, ami egy
ilyen fájlból mintegy 120 MB-ot fogad be körülbelül 1,5 GB-ban.

*dirt_limit*
: Hány képernyőfrissítés maradhat ki legfeljebb, amíg egy fájl beolvasása
tart. Ez az érték rendes körülmények között nem számít, mert a program a
beérkező billentyűk ütemétől függően maga állítja a kihagyott frissítések
számát. Nagyon lassú gépen vagy gyors billentyűismétlésű terminálon azonban a
nagy érték ugrálóvá teszi a képet. Alapértelmezés szerint 10, ez viselkedik a
legjobban.

A korábbi változatok ezeket a beállításokat a fő szakaszban, hosszabb néven
tartották (wrap_mode, viewer_syntax_highlighting, mouse_move_pages_viewer,
mcview_remember_file_position, mcview_structured_auto, mcview_eof és
max_dirt_limit). A program egyszer onnan olvassa be, majd a [Viewer]
szakaszba írja őket.

A panelek mögött a parancsértelmezőt futtató terminál beállításai ugyanennek
a fájlnak a [Terminal] szakaszában vannak. Nincs párbeszédablak, amely írná
őket.

*search_direction*
: Melyik irányban nézi át
**Alt-s**
a parancsértelmező kimenetét, és melyik irányban lép
**Alt-Shift-s**
a szűrő egyik soráról a következőre: a "down" a kurzortól a legújabb sor felé
megy, azon túl pedig a legrégebbire fordul, az "up" a legrégebbi sor felé megy
és a legújabbra fordul, ahogy a
**less**
keres. Alapértelmezés szerint "down".

*clipboard_write*
: Tehet-e a terminálban futó program szöveget a vágólapra az OSC 52
szekvenciával, ahogy a vim, a tmux vagy egy ssh munkamenet teszi. A szöveg
oda kerül, ahová a szerkesztőből másolt szöveg: a vágólap fájljába és a
*clipboard_store*
parancsnak. A program így soha nem olvashatja a vágólapot. Alapértelmezés
szerint "false": a terminál bármely kimenete, egy
**cat**
paranccsal kiírt fájl is, megváltoztathatná a vágólapot.

# Terminál adatbázisok <a id="terminal-databases"></a>

A M-Commander lehetőséget nyújt a terminál adatbázis root jogok
használata nélküli módosítására. A M-Commander a rendszer indító
fájlban (az defaults.ini fájlt a M-Commander library könyvtárában
találjuk), vagy az ~/.config/mc6/ini file "terminal:your-terminal-name" részében
keres, és, a "terminal:general" rész minden sora tartalmazza azokat a
billentyűzet szimbólumokat az egyenlőségjelet és a definiált szimbólumot
követően, amelyeket te mag akarsz határozni. A \\e speciális formátumot
az escape és a ^x-t a control-x karakter megjelenítésére használhatod.

A látható billentyű szimbólumok:

```
f0-tól f20-ig Funkció billentyűk f0-f20
bs            backspace
home          home gomb
end           end gomb
up            kurzor fel gomb
down          kurzor le gomb
left          kurzor balra gomb
right         kurzor jobbra gomb
pgdn          page down gomb
pgup          page up gomb
insert        az insert karakter
delete        a delete karakter
complete      a lezáró
```

Például ahhoz, hogy az insert gomb az Escape + [+ O + p-pel legyen
azonos, az alábbiakat állítsd be az ini fájlban:

```
insert=\e[Op
```

A
*complete*
billentyű szimbólum megjeleníti az escape szekvenciát, amely a leállító
folyamatot indítja el, az M-tab-bal indítható el, de definiálhatsz más
gombokat is ugyanerre a folyamatra (azokon a billentyűzeteken, ahol
valamelyik gomb nem használható).

<!-- help:break -->

# Környezeti változók <a id="environment"></a>

Az alábbi változókat olvassa az M-Commander, illetve ezeket állítja be az
általa indított programok számára. Az olyan változók, mint a **TERM**, a
**SHELL**, a **HOME** vagy a **PATH**, nem szerepelnek itt: a program azért
olvassa őket, hogy megtudja, hol fut, nem azért, hogy beállítsák vele.

## Induláskor olvasott változók <a id="read-at-start-up"></a>

**MC_DATADIR**
: A könyvtár, ahonnan az adatfájlok jönnek, a beépített helyett. Lásd
[Fájlok](#files).

**MC_PROFILE_ROOT**
: A felhasználói fájlok gyökere, abszolút útvonalként. Lásd [Fájlok](#files).

**MC_SKIN**
: A használandó skin, névvel vagy útvonallal. Lásd [Skinek](#skins).

**MC_KEYMAP**
: A használandó billentyűkiosztás-fájl. Lásd [Billentyűzet](#keys).

**MC_TMPDIR**
: A program ideiglenes fájljainak könyvtára.

**MC_NO_LUA**
: Az 1 érték hatására a program Lua futtatókörnyezet nélkül indul. Egyetlen
Lua csomag sem töltődik be, és semmi nem érhető el, aminek szüksége van rá.

**MC_SIXEL**
: A 0 érték azt mondja, hogy a terminál nem tud sixel grafikát, az 1 pedig
azt, hogy tud. A változó nélkül a program magát a terminált kérdezi meg.

**KEYBOARD_KEY_TIMEOUT_US**
: Mennyit várjon egy vezérlőszekvencia hátralévő részére, mikroszekundumban.

**COLORTERM**
: A színek kiválasztásakor olvasódik. Lásd [Színek](#colors).

**CDPATH**
: Azok a könyvtárak, amelyekben a beépített cd parancs keres.

**EDITOR**, **VIEWER**, **PAGER**
: A külső programok, amelyeket a program akkor használ, ha a beépített
szerkesztő vagy fájlnéző ki van kapcsolva. Lásd
[Külső szerkesztő vagy fájlnéző paraméterei](#parameters-for-external-editor-or-viewer).

## Az indított programok számára beállított változók <a id="set-for-the-programs-m-commander-starts"></a>

Ezeket nem kézzel kell beállítani. A program azért írja ki őket, hogy a
beépített terminálból indított saját másolata felismerje: már fut egy ilyenen
belül.

**MC_SID**
: A munkamenet, amelyben a program fut. Az ebből a munkamenetből indított
másolat nem nyit saját paneleket.

**MC_PID**
: A futó program folyamatazonosítója.

**MC_TTY**
: A terminál, amelyen a programot elindították.

## Hibakeresési naplók <a id="debug-logs"></a>

A napló csak akkor íródik, ha be van kapcsolva, és a kapcsoló értéke 1. A
bővítmények változói az általánosakra esnek vissza, így az általános pár
beállítása mindent naplóz.

**MC_LOG_ENABLE**, **MC_LOG_FILE**
: Az általános napló. **MC_LOG_FILE** nélkül az *ini* fájl *[Logging]*
szakaszának *logfile* bejegyzése szerinti fájl használódik, e bejegyzés nélkül
pedig az *mc.log* a többi felhasználói fájl mellett.

**MC_FTP_LOG_ENABLE**, **MC_FTP_LOG_FILE**
: Az ftp panelbővítmény naplója. A fájl alapértelmezése */tmp/mc-ftp.log*.

**MC_SMB_LOG_ENABLE**, **MC_SMB_LOG_FILE**
: A samba panelbővítmény naplója. A fájl alapértelmezése */tmp/mc-samba.log*.

**MC_SPELL_LOG**
: A fájl, amelybe a helyesírás-ellenőrzés ír. Nincs saját kapcsolója: a napló
akkor keletkezik, ha a változó megnevez egy fájlt.

Egy sikertelen ftp kapcsolat naplójának megőrzéséhez:

```
MC_FTP_LOG_ENABLE=1 MC_FTP_LOG_FILE=/tmp/ftp.log mcommander
```

# Fájlok <a id="files"></a>

A progam minden ezzel kapcsolatos infomációt az
**MC_DATADIR**
környezeti változóban tárol. Ha ezt a változót nem állítottuk be, akkor
ez vissza fog állítódni a /usr könyvtárra.

*{{pkgdatadir}}/help/mcommander.md*
: A program súgó fájlja.

*{{pkgdatadir}}/extensions.ini*
: Az alapértelmezett rendszerszintű kiterjesztés fájl.

*~/.config/mc6/extensions.ini*
: A felhasználó saját kiterjesztései, nézet beállítások és szerkesztési
beállítások. Ezek felülbírálják a rendszerszintű fájl bejegyzéseit, ha
van ilyen.

*{{pkgdatadir}}/mc.ini*
: Az alapértelmezett rendszerszintű M-Commander beállítás, amelyet
csak akkor használ, ha a felhasználónak nincs saját ~/.config/mc6/ini fájlja.

*{{pkgdatadir}}/defaults.ini*
: A M-Commander globális beállításai. Az ebben a fájlban elvégzett
beállítások minden felhasználó M-Commander-jére vonatkoznak, ez
használható a site-globális terminál beállításaihoz.

*~/.config/mc6/ini*
: A felhasználó saját beállításai. Ha ez a fájl elérhető, akkor a
beállítások ebből a fájlból olvasódnak be a rendszerszintű indító fájl
helyett.

*{{pkgdatadir}}/hints/hint*
: Ez a fájl tartalmazza a program által megjelenített útmutattásokat
(cookie-kat).

*~/.config/mc6/menu.ini*
: A felhasználói menü, amely saját magát szerkeszti, tételenként egy
csoporttal. Ahol ez a fájl létezik, az F2 ezt nyitja meg, és az aktuális
könyvtár .mc6menu fájlja mellette látszik.
*~/.config/mc6/menu*
: A falhasználó saját alkalmazás menüje. Ha ez a fájl elérhető a
rendszerszintű alkalmazás menü helyett ezt fogja használni.

*~/.cache/mc6/Tree*
: A könyvtárlista a Könyvtárfa és a Fa nézethez.  Minden sor egy
bejegyzés. Minden sor perjellel kezdik a teljes könyvtár neveknél. A
sorok egy számmal kezdődnek, amik azonosak az elöző könyvtáréval. Ha ezt
a fájlt el akarod készíteni a következő parancsot használd:

<!-- -->

```
find / -type d -print | sort > ~/.cache/mc6/Tree"
```

Normálisan nincs erre szükséged, mert a M-Commander automatikusan
frissíti ezt.

*./.usermenu*
: Helyi felhasználó által definiált menü. Ha ez a fájl létezik, ezt
használja a home, vagy rendszerszintű alkalmazás menü helyett.

To change default home directory of M-Commander, you can use
**MC_PROFILE_ROOT**
environment variable. The value of MC_PROFILE_ROOT must be an absolute path.
If MC_PROFILE_ROOT is unset or empty, HOME variable is used. If HOME is unset
or empty, M-Commander directories are get from GLib library.

# A M-Commander frissítése <a id="availability"></a>

A program legutolsó verzióját az ftp.nuclecu.unam.mx címen a
/linux/local könyvtárban találhatod meg, Európából pedig a
sunsite.mff.cuni.cz címen a /GNU/mc könyvtárban és az ftp.teuto.de címen
az /lmb/mc könyvtárban.

# Lásd még... <a id="see-also"></a>

ed(1), gpm(1), terminfo(1), view(1), sh(1), bash(1), tcsh(1),
zsh(1).

```
A M-Commander World Wide Web oldalának címe a
következő:
	https://github.com/blue-panels/mcommander
```

# Szerzők <a id="authors"></a>

Miguel de Icaza (miguel@roxanne.nuclecu.unam.mx), Janne Kukonlehto
(jtklehto@paju.oulu.fi), Radek Doulik (rodo@ucw.cz), Fred Leeflang
(fredl@nebula.ow.org), Dugan Porter (dugan@b011.eunet.es), Jakub Jelinek
(jj@sunsite.mff.cuni.cz), Ching Hui (mr854307@cs.nthu.edu.tw), Andrej
Borsenkow (borsenkow.msk@sni.de), Norbert Warmuth
(nwarmuth@privat.circular.de), Mauricio Plaza
(mok@roxanne.nuclecu.unam.mx), Paul Sheer (psheer@icon.co.za) and Pavel
Machek (pavel@ucw.cz) are the developers of this package; Alessandro
Rubini (rubini@ipvvis.unipv.it) has been especially helpful debugging
and enhancing the program's mouse support, John Davis
(davis@space.mit.edu) also made his S-Lang library available to us under
the GPL and answered my questions about it, and the following people
have contributed code and many bug fixes (in alphabetical order):

Adam Tla/lka (atlka@sunrise.pg.gda.pl), alex@bcs.zp.ua (Alex I.
Tkachenko), Antonio Palama, DOS port (palama@posso.dm.unipi.it), Erwin
van Eijk (wabbit@corner.iaf.nl), Gerd Knorr (kraxel@cs.tu-berlin.de),
Jean-Daniel Luiset (luiset@cih.hcuge.ch), Jon Stevens
(root@dolphin.csudh.edu), Juan Francisco Grigera, Win32 port
(j-grigera@usa.net), Juan Jose Ciarlante (jjciarla@raiz.uncu.edu.ar),
Ilya Rybkin (rybkin@rouge.phys.lsu.edu), Marcelo Roccasalva
(mfroccas@raiz.uncu.edu.ar), Massimo Fontanelli (MC8737@mclink.it),
Pavel Roskin (proski@gnu.org), Sergey Ya. Korshunoff
(seyko2@gmail.com), Thomas Pundt (pundtt@math.uni-muenster.de), Timur
Bakeyev (timur@goff.comtat.kazan.su), Tomasz Cholewo
(tjchol01@mecca.spd.louisville.edu), Torben Fjerdingstad
(torben.fjerdingstad@uni-c.dk), Vadim Sinolitis (vvs@nsrd.npi.msu.su)
and Wim Osterholt (wim@djo.wtm.tudelft.nl).

# Hibák bejelentése <a id="bugs"></a>

Ha a programmal kapcsolatos problémád van, akkor azt küld el az alábbi
címre: <https://github.com/blue-panels/mcommander/issues> .

Gondoskodj arról, hogy tartalmazza a hiba minél pontosabb
meghatározását, a futtatott program verziószámát (az mcommander -V parancs meg
fogja jeleníttetni ezt), az operációs rendszert, amin futtatod a
programot amikor az összeomlott, méltányolni fogjuk a részletes leírást.
