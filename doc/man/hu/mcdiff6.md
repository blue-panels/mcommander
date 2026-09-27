---
date: 2026. szeptember
---

<!-- help:topics "Tartalomjegyzék" -->
# NÉV <!-- help:skip -->

mcdiff6 - Belső összehasonlító.

# ALKALMAZÁSA <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] fájl1 fájl2

# LEÍRÁS

Az mcdiff6 az
**mcommander**
program, a fájlkezelő fő programja felé mutató link. A program ezen a néven
indítva a belső összehasonlítót nyitja meg, amely a parancssorban megadott
*fájl1*
és
*fájl2*
tartalmát veti össze.

# Belső összehasonlító <a id="diff-viewer"></a>

Az mcdiff6 szemléletes összehasonlító eszköz. Két fájlt lehet vele összevetni
és helyben szerkeszteni, a különbség pedig minden változtatás után újra
kiszámolódik. A git panelbővítmény is ezt nyitja meg, az egyik oldalon a
fájllal úgy, ahogy a HEAD tartalmazza, a másikon a munkapéldánnyal.

A belső összehasonlítóban a következő billentyűk használhatók:

**F1**
: Elindítja a beépített hypertext súgót.

**F2**
: Menti a módosított fájlokat.

**F4**
: A bal oldali fájlt szerkeszti a belső szerkesztőben.

**F14**
: A jobb oldali fájlt szerkeszti a belső szerkesztőben.

**F5**
: Átviszi az aktuális eltérést a jobb oldali fájlba. Csak az aktuális eltérés
kerül át, a különbség pedig újra kiszámolódik.

**F15**
: Átviszi az aktuális eltérést a másik irányba, a bal oldali fájlba.

**F7**
: Keresés indítása.

**F17**
: A keresés folytatása.

**F9**
: Megnyitja az
[összehasonlítás beállításait](#diff-options).

**Alt-e**
: Kiválasztja a kódlapot, amellyel a két fájlt beolvassa.

**F10, Esc, q, Q**
: Kilép az összehasonlítóból.

**Alt-s, s**
: Az eltérések állapotának megjelenítését kapcsolja.

**Alt-n, l**
: A sorszámok megjelenítését kapcsolja.

**Ctrl-s**
: Szintaxiskiemelést kapcsol. A sorok szövegét a szintaxisszabályok színezik,
ugyanúgy, ahogy a belső szerkesztőben, a sor állapota pedig a háttérre és a
jelölőoszlopra marad. Ahol a skin a megváltozott szót csak a szöveg színével
különbözteti meg a sor többi részétől, ott a szó aláhúzva jelenik meg. A
beállítás a szerkesztő ugyanilyen beállításától függetlenül őrződik meg.

**f**
: Teljes méretűre nyitja a bal oldali panelt.

**=**
: Egyforma szélességűre állítja a paneleket.

**>**
: Szűkíti a jobb oldali panelt.

**<**
: Szűkíti a bal oldali panelt.

**2, 3, 4, 8**
: Beállítja a tabulátor méretét.

**C-u**
: Felcseréli a két panel tartalmát.

**C-r**
: Újraolvassa mindkét fájlt, és újra kiszámolja a különbséget.

**C-o**
: Megmutatja a parancsképernyőt.

**Enter, szóköz, n**
: A következő eltérésre lép.

**Backspace, p**
: Az előző eltérésre lép.

**g, G**
: Ugrás a megadott sorra.

**Down**
: Egy sort gördít előre.

**Up**
: Egy sort gördít vissza.

**PageUp**
: Egy lapot lapoz vissza.

**PageDown**
: Egy lapot lapoz előre.

**Left, Right**
: Egy oszloppal tolja el a szöveget oldalra.

**C-Left, C-Right**
: Nyolc oszloppal tolja el a szöveget oldalra.

**Home**
: Visszatér az első oszlophoz.

**C-Home**
: A fájl elejére lép.

**C-End**
: A fájl végére lép.

# Az összehasonlítás beállításai <a id="diff-options"></a>

A
[belső összehasonlító](#diff-viewer)
beállításai, amelyeket ott az
**F9**,
a fájlkezelő Beállítások menüjében pedig az
**Összehasonlító beállításai**
tétel nyit meg. Az összehasonlító az indulásakor veszi át őket, így a már
megnyitott összevetés azokkal dolgozik tovább, amelyekkel elindult.

*Összehasonlító algoritmus.*
A Normál a fájlokat úgy veti össze, ahogy vannak. A Leggyorsabb nagy
fájlokat feltételez, és durvább eredménnyel is beéri. A Legkisebb több időt
tölt azzal, hogy kevesebb változást találjon.

*Kis- és nagybetű mellőzése.*
A kis- és a nagybetű ugyanannak a betűnek számít.

*Tabulátorra bontás mellőzése.*
Azok a sorok, amelyek csak abban térnek el, hogy ugyanaz a behúzás
tabulátorral vagy szóközökkel van írva, egyformának számítanak.

*Szóközváltozás mellőzése.*
Egy összefüggő üres rész bármely másik üres résszel egyenértékű.

*Minden szóköz mellőzése.*
Az üres helyek kimaradnak az összehasonlításból.

*Soremelés előtti kocsivissza levágása.*
A sor végén álló kocsivissza karakter lekerül, így a DOS sorvégű fájl
összevethető a Unix sorvégűvel.

# LICENC <!-- help:skip -->

Ez a program a Free Software Foundation által közzétett GNU General Public
License feltételei szerint terjeszthető. A licencről és a garancia hiányáról
a beépített súgó ad részletes tájékoztatást.

# Az M-Commander frissítése

Az M-Commander legfrissebb változata a
<https://github.com/blue-panels/mcommander/releases>
címen érhető el.

# Lásd még...

mcommander(1), mview(1), mcedit6(1), diff(1).

# Hibák bejelentése

A hibákat a
<https://github.com/blue-panels/mcommander/issues>
címen lehet bejelenteni.
