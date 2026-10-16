---
date: 2026. szeptember
---

<!-- help:topics "Tartalomjegyzék" -->
# NÉV <!-- help:skip -->

mcdiff6 - Belső összehasonlító.

# ALKALMAZÁSA <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] fájl1 fájl2

**mcdiff6**
[-bcCdfhstVx?] [-o kimenet] helyi alap távoli

**mcdiff6**
[-bcCdfhstVx?] [-o kimenet] -B alap helyi távoli

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

**Tab**
: Átváltja az aktív fájlt. Ennek kerete dupla, a másik fájl kerete szimpla és
szürke. Az aktív fájl keretén egy jel mutatja a felső sort: ezen dolgozik az
F5 és az F15.

**F4**
: Az aktív fájlt szerkeszti a belső szerkesztőben.

**F14**
: A másik fájlt szerkeszti a belső szerkesztőben.

**F5**
: Átmásolja az aktuális eltérést az aktív fájlból a másikba. Az aktuális
eltérés a felső sorban álló. Csak ez kerül át, a különbség pedig újra
kiszámolódik. Az irányt a billentyűsor mutatja: "Copy->" vagy "<-Copy".

**F15**
: Átmásolja az aktuális eltérést a másik irányba, a másik fájlból az aktívba.

**Shift-F6**
: A két fájl összefésülése a közös alapjukhoz képest: a nézet bekéri az
alapfájlt, és megmutatja a három fájl összefésülését. Ott az F2 előbb
megkérdezi, hová mentse az eredményt.

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
: Csak az aktív fájlt mutatja, a teljes képernyőn.

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

# Három fájl összefésülése <a id="merge"></a>

Három fájllal indítva az összehasonlító három oszlopot mutat: bal oldalon a
helyi fájlt, középen az összefésülés eredményét, jobb oldalon a távoli
fájlt. A két szélső fájl soha nem változik; csak a középső oszlop kerül
mentésre.

Induláskor a középső oszlop az alapfájl, amelybe már bekerült minden olyan
változtatás, amelyet csak az egyik oldal végzett. Ahol mindkét oldal
ugyanazokat a sorokat változtatta meg, mindegyik a maga módján, középen az
alap sorai maradnak: ez ütközés, az ütközés színével látszik, a számuk a
középső oszlop állapotsorában a "Cnf" után áll.

A középső oszlop keretének felső vonala a "Result:" után megnevezi a
fájlt, ahová az eredmény mentődik, az alsó vonala pedig a "Base:" után az
alapot.

A különbségek a középső oszlop minden változása után újra kiszámolódnak.
40 oszlopnál keskenyebb képernyőn a két fájl fent egymás mellett, a
középső oszlop alattuk látszik.

**F5**
: A bal oldali fájl sorait hozzáadja a középső oszlop aktuális blokkjához, a
már benne lévők után. Az aktuális blokk a legfelső soron lévő, vagy az első
a képernyőn.

**Shift-F5**
: A bal oldali fájl sorait újra kiveszi a blokkból. Ha egyik oldal sem
marad, a blokkban az alap sorai vannak: az ütközés újra ütközés.

**F6**
: A jobb oldali fájl sorait hozzáadja a blokkhoz. Ha mindkét oldal hozzá van
adva, a sorok a hozzáadás sorrendjében követik egymást.

**Shift-F6**
: A jobb oldali fájl sorait újra kiveszi.

**], [**
: Ugrás a következő vagy az előző ütközésre.

**Enter, Space, n; Backspace, p**
: Ugrás a következő vagy az előző blokkra.

**F4**
: A középső oszlop szerkesztése a szerkesztőben. Az ott módosított blokk
megoldottnak számít.

**F2**
: A középső oszlop mentése. Ha maradt ütközés, előbb rákérdez.

**F10**
: Kilépés. Ha a középső oszlop megváltozott és nincs mentve, rákérdez,
hogy mentse-e. Változtatás nélkül kilépve a fájlok nem változnak.

A program 0-s kilépési kóddal ér véget, ha az összefésülés ütközés nélkül
mentésre került, és 1-essel minden más esetben, így a git összefésülő
eszköze lehet:

```
[merge]
    tool = mcdiff
[mergetool "mcdiff"]
    cmd = mcdiff6 "$LOCAL" "$BASE" "$REMOTE" -o "$MERGED"
    trustExitCode = true
```

A
*-o*
kapcsoló nélkül az összefésülés az alapfájlba kerül.

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
A Normál a fájlokat úgy veti össze, ahogy vannak. A Leggyorsabb a histogram
algoritmust használja, amely akkor is gyors marad, ha a fájlok nagyon
eltérnek. A Legkisebb több időt tölt azzal, hogy kevesebb változást találjon.

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
