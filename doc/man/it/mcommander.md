---
date: settembre 2026
---

<!-- help:topics "Indice degli argomenti:" -->
# NOME <!-- help:skip -->

mcommander - file manager a due pannelli in modalità testo

# USO <!-- help:skip -->

**mcommander**
[-abcCdfhPstuUVx] [-l log] [dir1 [dir2]] [-e [file]] [-v file]

# DESCRIZIONE <a id="description"></a>

M-Commander è un file manager a due pannelli in modalità testo, basato su GNU
Midnight Commander. L'architettura è costruita attorno a un nucleo compatto e
a componenti aggiuntivi dei pannelli caricati dinamicamente. I componenti
aggiuntivi offrono un'interfaccia a pannelli uniforme per archivi, file system
remoti, repository e altre fonti di dati. I comandi vengono eseguiti in un
terminale incorporato. M-Commander comprende inoltre un editor di testo con
evidenziazione della sintassi e un visualizzatore che supporta formati di
testo e binari.


# OPZIONI <a id="options"></a>

*-a*
: Disabilita l'uso dei caratteri grafici per il disegno delle linee.

*-b*
: Forza la visualizzazione in bianco e nero.

*-c*
: Forza la modalità colore; consultare la sezione
[colori](#colors)
per ulteriori informazioni.

*-d*
: Disabilita il supporto mouse.

*-e [file]*
: Esegue l'editor interno. Se il file viene specificato, lo apre alla
partenza. Vedere anche
**mcedit6(1)**.

*-f*
: Mostra i percorsi di ricerca compilati per i file del M-Commander.

*-l file*
: Salva il dialogo ftpfs con il server in file.

*-P file*
: Quest'opzione indica al M-Commander di stampare l'ultima
directory di lavoro sul file specificato.
Questa funzione non è fatta per un uso diretto, ma dovrebbe essere
utilizzata da una speciale funzione shell che imposti automaticamente
l'ultima directory corrente della shell come l'ultima directory in cui
stava il M-Commander. Prelevate i file
**{{pkglibexecdir}}/mc6.sh**
(utenti bash e zsh),
**{{pkglibexecdir}}/mc6.csh**
(utenti tcsh) o rispettivamente
**{{pkglibexecdir}}/mc6.fish**
(utenti fish) per definire
**mcommander**
come un alias allo script di shell appropriato.

*-s*
: Abilita il modo terminale lento, in questa modalità il programma
non disegna le linee e disabilita la modalità prolissa.

*-t*
: Usata solo se il codice è stato compilato con S-Lang e terminfo: fa
in modo che il M-Commander usi il valore della variabile
**TERMCAP**
per le informazioni sul terminale invece delle informazioni di sistema
sul database terminali.

*-u*
: Disabilita l'uso della shell concorrente (ha senso solo se il
M-Commander è stato compilato con il supporto per la shell
concorrente).

*-U*
: Abilita l'uso della shell concorrente (ha senso solo se il
M-Commander è stato compilato con il supporto per la shell
concorrente impostato come una caratteristica opzionale).

*-v file*
: Lancia il visualizzatore interno per il file specificato.

*-V*
: Mostra la versione del programma.

*-x*
: Forza la modalità xterm. Usata quando è in funzione su terminali
abilitati-xterm (due modalità video e in grado di spedire sequenze
mouse di escape).

*-X, --no-x11*
: Do not use X11 to get the state of modifiers Alt, Ctrl, Shift

*-g, --oldmouse*
: Force a "normal tracking" mouse mode. Used when running on
xterm-capable terminals (tmux/screen).

Se specificato, il primo percorso è la directory mostrata nel
pannello selezionato; il secondo è la directory mostrata nell'altro
pannello.

# Panoramica <a id="overview"></a>

Lo schermo del M-Commander è diviso in quattro parti. Quasi tutto
lo spazio è occupato dai due pannelli directory. Come impostazione
predefinita la seconda riga dal fondo è la riga di comando, mentre
quella in basso mostra le etichette dei tasti funzione. La riga più in
alto è la
[riga dei menu](#menu-bar).
La barra dei menu può essere invisibile, ma compare se clicchi la
riga più in alto con il mouse o se premi il tasto F9.

Il M-Commander fornisce la vista di due directory
contemporaneamente. Uno dei due pannelli è quello corrente (la barra
di selezione è presente solo in questo). Quasi tutte le operazioni
hanno luogo nel pannello corrente. Alcune azioni come Rinomina e
Copia usano la directory del pannello non selezionato come valore
predefinito di destinazione (ma si richiede sempre una conferma prima).
Per informazioni aggiuntive, vedere le sezioni sui
[pannelli directory](#directory-panels),
i
[menu sinistra e destra](#left-and-right-menus)
e
[menu file](#file-menu).

E' possibile eseguire comandi di sistema dal M-Commander
semplicemente battendoli. Ogni cosa scritta apparirà sulla riga di
comando e quando si preme l'invio il M-Commander eseguirà la
riga di comando appena battuta; leggere le sezioni
[shell a riga di comando](#shell-command-line)
e
[tasti della riga di ingresso](#input-line-keys)
per saperne di più sulla riga di comando.

# Supporto mouse <a id="mouse-support"></a>

Il M-Commander è fornito di supporto mouse. Esso viene
attivato ogniqualvolta lo si esegue in un terminale
**xterm(1)**
(funziona anche se si fa una connessione telnet, ssh o rlogin con
un'altra macchina da un xterm) o se sta funzionando su una console Linux
e si ha il mouse server
**gpm**
in funzione.

Quando si fa clic con il tasto sinistro in un file nel pannello
directory, il file viene selezionato; se si fa clic con il tasto destro
il file viene marcato (o smarcato, a seconda dello stato precedente).

Se il file è un programma eseguibile, il doppio clic su di esso lo eseguirà
altrimenti se il
[file estensioni](#edit-extension-file)
ha un programma specifico per quell'estensione del file, il suddetto programma
verrà eseguito.

E' anche possibile eseguire i comandi assegnati ai tasti funzione
cliccando sulle etichette dei tasti.

Se un tasto del mouse viene premuto sulla riga in cima al pannello directory,
il pannello sfoglia di una pagina in alto. Allo stesso modo, un clic sulla
riga in basso provocherà un cambio di pagina in basso. Questo metodo dei bordi
funziona anche nel
[visualizzatore dell'aiuto](#contents)
e nell'
[albero directory](#directory-tree).

L'auto ripetizione predefinita per il mouse è di 400 millisecondi. Questo
valore può essere cambiato modificando il file
[~/.config/mc6/ini](#save-setup)
e cambiando il parametro
*mouse_repeat_rate.*

Se il Commander sta funzionando con il supporto mouse, si può saltarlo
ed ottenere il funzionamento del mouse normale (taglia e incolla di testo)
tenendo premuto il tasto Maiuscole.

<!-- help:break -->

# Tasti <a id="keys"></a>

Alcuni comandi nel M-Commander presuppongono l'uso dei tasti
*Control*
(talvolta chiamato CTRL o CTL) e
*Meta*
(talvolta chiamato ALT o anche Compose). In questo manuale si utilizzeranno
le seguenti abbreviazioni:

**C-\<chr>**
: significa premere il tasto control mentre si batte il carattere \<chr>.
Perciò C-f sarà: premi e tieni premuto il tasto Control e premi f.

**M-\<chr>**
: significa premere il tasto Meta o Alt mentre si batte \<chr>.
Se non c'è un tasto Meta o Alt, premere
*ESC,*
rilasciarlo, poi premere il carattere \<chr>.

**S-\<chr>**
: significa premere il tasto Maiuscole mentre si batte il carattere \<chr>.

Tutte le linee di ingresso nel M-Commander usano un'approssimazione
dei tasti usati dall'editor GNU Emacs.

Ci sono molte sezioni che parlano dei tasti. Le seguenti sono le
più importanti.

La sezione
[menu file](#file-menu)
documenta le abbreviazioni di tasti per i comandi che appaiono nel
menu file. Questa sezione include i tasti funzione. Molti di questi comandi
lavorano sui file selezionati o sui file marcati.

La sezione
[pannelli directory](#directory-panels)
documenta i tasti che selezionano o marcano i file come oggetto
per una seguente azione (l'azione normalmente deriva dal menu file).

La sezione
[shell a riga di comando](#shell-command-line)
elenca i tasti utilizzati per immettere e modificare linee di comando.
Molti di questi copiano nomi di file o altro dal pannello directory
alla riga di comando (per evitare troppo lavoro di battitura) o per
accedere alla cronologia comandi.

I
[tasti della riga di ingresso](#input-line-keys)
sono usati per modificare le righe di ingresso. Cioè sia la riga di comando
che le righe di ingresso nelle finestre di interrogazione.

## Ridefinizione dei tasti <a id="keys_redefine"></a>

Lo stesso si può fare nel programma stesso, dal menu
**Opzioni**.
La finestra
[Associazioni dei tasti](#key-bindings)
elenca ogni azione con i tasti a cui risponde, li cambia e scrive il
risultato in
**~/.config/mc6/keymap.ini**,
cioè nel file che l'opzione cerca. La finestra
[Impara tasti](#learn-keys)
si occupa dell'altro lato del problema: insegna al programma le sequenze che
il terminale invia per i tasti che riconosce male. Il
[Analizzatore di tasti](#key-sniffer)
mostra che cosa arriva quando si preme un tasto, insieme all'azione a cui
quel tasto è associato nella mappa corrente, ed è la cosa da guardare quando
un'associazione sembra non fare nulla.

Le associazioni dei tasti possono essere lette da un file esterno. All'inizio
il programma costruisce la mappa dalle associazioni definite nel codice
sorgente. Poi vengono sempre caricati i due file
**{{pkgdatadir}}/keymap.ini**
e
**{{sysconfdir}}/mcommander/keymap.ini**,
che ridefiniscono in ordine le associazioni precedenti.
Il pacchetto mette le proprie mappe in
**{{sysconfdir}}/mcommander**:
**keymap.default.ini**,
**keymap.emacs.ini**
e
**keymap.vim.ini**,
dove
**keymap.ini**
è un collegamento a quella predefinita.
L'opzione
**--nokeymap**
non legge alcun file e lascia le associazioni del codice sorgente.

Il file dell'utente viene cercato con questo ordine (fino al primo trovato):

```
1) opzione a riga di comando -K <mappa>, --keymap=<mappa>
2) variabile d'ambiente MC_KEYMAP
3) parametro keymap della sezione [Midnight-Commander]
4) file ~/.config/mc6/keymap.ini
```

I primi tre accettano un nome o un percorso assoluto. A un nome che non
finisce in
**.keymap**
viene aggiunta questa estensione, e il file viene cercato in (fino al primo
trovato):

```
1) ~/.config/mc6/
2) {{pkgdatadir}}/
```

Per via di quell'estensione le mappe del pacchetto, i cui nomi finiscono in
**.ini**,
non si possono scegliere in questo modo. Per usarne una, copiarla o
collegarla a
**~/.config/mc6/keymap.ini**,
che viene letto per ultimo e non richiede alcuna opzione:

```
ln -s {{sysconfdir}}/mcommander/keymap.vim.ini ~/.config/mc6/keymap.ini
```

## Tasti vari <a id="miscellaneous-keys"></a>

Qua ci sono alcuni tasti che non sono classificabili in nessuna delle
altre categorie:

**Invio**
: se c'è del testo nella riga di comando (quella in fondo ai pannelli),
allora quel comando viene eseguito. Se non c'è testo nella riga di
comando allora se la barra di selezione è sopra una directory il
M-Commander esegue un
**chdir(2)**
alla directory selezionata e ricarica le informazioni sul pannello;
se la selezione è un file eseguibile allora esso viene eseguito.
Per ultimo, se l'estensione del file selezionato corrisponde ad una
delle estensioni presenti nel
[file estensioni](#edit-extension-file),
il comando corrispondente viene eseguito.

**C-l**
: ridisegna tutto nel M-Commander.

**C-x c**
: esegue il comando
[chmod](#chmod)
su un file o su un gruppo di file marcati.

**C-x o**
: esegue il comando
[chown](#chown)
sul file corrente o sui file marcati.

**C-x l**
: crea un collegamento.

**C-x s**
: crea un collegamento simbolico.

**C-x i**
: imposta la modalità della visualizzazione dell'altro pannello a informazioni.

**C-x q**
: imposta la modalità della visualizzazione dell'altro pannello a vista rapida.

**C-x !**
: esegue il comando
[pannellizza comando](#external-panelize).

**C-x h**
: esegue il comando aggiungi directory alla lista
[directory favorite](#hotlist).

**M-!**
: esegue il comando vista filtrata, descritto in
[visualizzatore di file interno](mview.md#internal-file-viewer).

**M-?**
: esegue il comando
[trova file](#find-file).

**M-c**
: mostra la finestra
[cambia dir veloce](#quick-cd).

**C-o**
: quando il programma viene eseguito in una console Linux o FreeBSD o in
un xterm, mostrerà il risultato del comando precedente. Eseguito in
console Linux, il M-Commander usa un programma esterno
(cons.saver) per gestire il salvataggio e recupero delle informazioni
sullo schermo.

Se è stato compilato il supporto alla subsell, è possibile premere C-o
in ogni momento per tornare alla schermata principale del M-Commander;
per tornare all'applicazione basta premere C-o. Se si ha un'applicazione
sospesa usando questo trucco, non si sarà in grado di eseguire altri
programmi dal M-Commander finché non si terminerà l'applicazione
sospesa.

## Pannelli directory <a id="directory-panels"></a>

Questa sezione elenca i tasti che operano sui pannelli directory.
Se si desidera sapere come cambiare la visualizzazione dei pannelli,
date un'occhiata alla sezione su
[menu sinistra e destra](#left-and-right-menus).

**Tab, C-i**
: cambia il pannello corrente. L'altro pannello diventa il nuovo pannello
corrente mentre il pannello corrente diventa l'altro pannello.
La barra di selezione si sposta dal vecchio pannello al nuovo corrente.

**Ins, C-t**
: DEPRECATED! per marcare i file si può usare il tasto di Inserimento (la sequenza
teminfo kich1) o la sequenza C-t (Control-t). Per smarcare i file
basta marcare un file già marcato.

**Insert, C-t**
: to tag files you may use the Insert key (the kich1 terminfo sequence).
To untag files, just retag a tagged file.

**M-e**
: to change charset of panel you may use M-e (Alt-e).
Recoding is made from selected codepage into system codepage. To
cancel the recoding you may select "directory up" (..) in active panel.
To cancel the charsets in all directories, select "No translation " in
the dialog of encodings.

**M-g, M-r, M-j**
: usato per selezionare rispettivamente il file superiore, il file centrale o
quello inferiore in un pannello.

**M-t**
: cambia il modo di visualizzazione corrente per mostrare la modalità
successiva. In questo modo è possibile cambiare velocemente da listati
lunghi a listati normali a listati definiti dall'utente.

**C-\\ (control-barra retroversa)**
: mostra le
[directory favorite](#hotlist)
e va alla directory selezionata.

**+  (più)**
: viene utilizzato per selezionare (marcare) un gruppo di file. Il
M-Commander richiederà un'espressione regolare per descrivere il gruppo.
Quando i
*modelli della shell*
sono abilitati, le espressioni regolari sono molto simili alle espressioni
regolari in una shell (\* significa zero o più caratteri e ? un carattere). Se i
*modelli della shell*
sono disabilitati, la marcatura dei file viene fatta con le normali espressioni
regolari (vedere ed (1)).

**\\ (barra retroversa)**
: usare il tasto "\\" per deselezionare un gruppo di file. Questo è l'opposto
del tasto più.

**freccia-su, C-p**
: sposta la barra di selezione alla voce precedente nel pannello.

**freccia-giù, C-n**
: sposta barra di selezione alla voce successiva nel pannello.

**home, a1, M-<**
: sposta la barra di selezione alla prima voce nel pannello.

**fine, c1, M->**
: sposta la barra di selezione all'ultima voce nel pannello.

**pagina-giù, C-v**
: sposta la barra di selezione di una pagina in basso.

**pagina-su, M-v**
: sposta la barra di selezione di una pagina in alto.

**M-o**
: rende la directory corrente del pannello corrente, la directory
corrente dell'altro pannello. Mette l'altro pannello in modalità
elenco se necessario. Se il pannello corrente è pannellizzato,
l'altro non diventa pannellizzato.

**C-PaginaSu, C-PaginaGiù**
: solo quando si esegue in console Linux: rispettivamente cambia
directory a ".." e alla directory correntemente selezionata.

**M-y**
: sposta la directory precedente nella cronologia, equivalente a
premere '<' con il mouse.

**M-u**
: sposta la directory successiva nella cronologia, equivalente a
premere '>' con il mouse.

**M-S-h, M-H**
: mostra la cronologia directory, equivalente a premere 'v' con il mouse.

## Ricerca rapida e filtro rapido <a id="quick-search"></a>

La modalità di ricerca rapida permette di trovare in fretta i nomi dei file
in un pannello. Premendo
**C-s**
o
**Alt-s**
si avvia la ricerca del nome nell'elenco della directory. Con
**Alt-Maiusc-s**
si avvia il filtro rapido, che usa lo stesso modello ma nasconde le voci che
non lo contengono. La voce della directory superiore si vede sempre.

Con una delle due modalità attiva, i tasti premuti si aggiungono al modello
comune invece che alla riga di comando. Se l'opzione
*Mostra mini-stato*
è attiva, il modello si vede nella riga di mini-stato. Mentre si scrive, la
barra di selezione si sposta sul file successivo il cui nome inizia con le
lettere scritte; in modalità filtro l'elenco si riduce inoltre alle voci che
corrispondono. I tasti
**Backspace**
o
**Canc**
servono a correggere gli errori di battitura.

Premere
**C-s**
o
**Alt-s**
con il filtro rapido attivo passa alla ricerca rapida e mostra tutte le voci
senza perdere il modello né il file corrente. Premere
**Alt-Maiusc-s**
con la ricerca rapida attiva riporta al filtro. Ripetere il tasto della
modalità attiva porta all'occorrenza successiva.

I tasti di movimento, le frecce,
**Inizio**,
**Fine**,
**PagSu**
e
**PagGiù**
si muovono dentro l'elenco filtrato senza chiudere il filtro.

I file si possono selezionare e deselezionare con il filtro attivo. Le
selezioni si mantengono cambiando modalità o chiudendo il filtro.

Se una delle due modalità viene avviata premendo due volte il suo tasto,
viene ripreso il modello precedente.

Oltre ai caratteri dei nomi si possono usare anche i caratteri jolly '\*' e
'?'.

## Shell a riga di comando <a id="shell-command-line"></a>

Questa sezione elenca i tasti utili ad evitare troppe battiture
nell'immissione dei comandi.

**M-Invio**
: copia nella riga di comando il nome del file attualmente selezionato.

**C-Invio**
: come M-Invio, ma funziona solo dalla console Linux.

**M-Tab**
: esegue automaticamente il
[completamento](#completion)
del nome del file, variabile, nome utente e nome host.

**C-x t, C-x C-t**
: copia i file marcati (o se non vi sono file marcati, il file selezionato)
del pannello corrente (C-x t) o dell'altro pannello (C-x C-t) sulla
riga di comando.

**C-x p, C-x C-p**
: la prima sequenza di tasti copia il percorso corrente sulla riga di comando
e la seconda copia il percorso del pannello non selezionato sulla riga
di comando.

**C-q**
: il comando di inserimento letterale serve per inserire caratteri che
sarebbero altrimenti interpretati dal M-Commander (come il simbolo '+')

**M-p, M-n**
: Usa questi tasti per navigare attraverso la cronologia comandi. M-p va alla voce
precedente, M-n va alla successiva.

**M-h**
: mostra la cronologia per la riga di ingresso corrente.

## Tasti generali di movimento <a id="general-movement-keys"></a>

Il visualizzatore dell'aiuto, il visualizzatore dei file e l'albero directory
usano un codice comune per gestire il movimento. Per questa ragione essi
accettano esattamente gli stessi tasti. Ognuno di questi però accetta anche
altri tasti indipendenti.

Diverse parti del M-Commander usano gli stessi tasti di
movimento, questa sezione riguarda quelle parti.

**Su, C-p**
: si sposta di una riga indietro.

**Giù, C-n**
: si sposta di una riga avanti.

**Pag. Prec., Pagina Su, M-v**
: si sposta di una pagina in alto.

**Pag. Succ., Pagina Giù, C-v**
: si sposta di una pagina in basso.

**Home, A1**
: si sposta all'inizio.

**Fine, C1**
: si sposta alla fine.

In aggiunta a quelli menzionati sopra, il visualizzatore dell'aiuto accetta
i seguenti tasti:

**b, C-b, C-h, Backspace, Canc**
: si sposta di una pagina in alto.

**Barra spaziatrice**
: si sposta di una pagina in basso.

**u, d**
: si sposta di mezza pagina in alto o in basso.

**g, G**
: si sposta all'inizio o alla fine.

## Tasti di riga di ingresso <a id="input-line-keys"></a>

I tasti di riga di ingresso (sono usati
per la
[riga di comando](#shell-command-line)
e per i dialoghi di richiesta dati nel programma) accettano
questi tasti:

**C-a**
: sposta il cursore all'inizio della riga.

**C-e**
: sposta il cursore alla fine della riga

**C-b, freccia-sinistra**
: sposta il cursore di una posizione a sinistra.

**C-f, freccia-destra**
: sposta il cursore di una posizione a destra.

**M-f**
: sposta il cursore di una parola in avanti.

**M-b**
: sposta il cursore di una parola indietro.

**C-h, backspace**
: cancella il carattere precedente.

**C-d, Canc**
: cancella il carattere nel punto (sopra il cursore).

**C-@**
: imposta il marcatore per tagliare.

**C-w**
: copia il testo tra il cursore e il marcatore in un kill buffer
e rimuove il testo dalla riga di ingresso.

**M-w**
: copia il testo tra il cursore ed il marcatore in un kill buffer.

**C-y**
: inserisce il contenuto del kill buffer.

**C-k**
: elimina il testo dal cursore alla fine della riga.

**Ctrl-Ins**
: copia il testo selezionato nel file di scambio e negli appunti del sistema.
Senza selezione: i file marcati del pannello sullo schermo, uno per riga;
altrimenti l'intera riga; altrimenti il file sotto il cursore del pannello.

**Maiusc-Canc**
: taglia il testo selezionato nel file di scambio e negli appunti del
sistema.

**Maiusc-Ins**
: incolla il file di scambio nella riga come una sola riga: gli a capo e gli
altri caratteri di controllo diventano spazi. Nella riga di comando funziona
con i pannelli visibili e nascosti. Più di 2 KB di testo vengono incollati
solo dopo una conferma.

**M-p, M-n**
: usa questi tasti per navigare attraverso la cronologia dei comandi. M-p
posiziona sull'ultima voce, M-n posiziona sulla seguente.

**M-C-h, M-Backspace**
: cancella una parola indietro.

**M-Tab**
: fa del nomefile, comando, variabile, nomeutente e nomehost il
[completamento](#completion)
automatico.

<!-- help:break -->

# Barra dei menu <a id="menu-bar"></a>

La barra dei menu compare premendo F9 o cliccando con il mouse sopra la riga
superiore dello schermo. La barra menu possiede sei menu: "Sinistra", "File",
"Attributi", "Comando", "Opzioni" e "Destra".

I
[menu sinistra e destra](#left-and-right-menus)
permettono di modificare l'aspetto dei pannelli directory di
sinistra e di destra.

Il
[menu file](#file-menu)
elenca le azioni che possono essere condotte sui file correntemente selezionati
o marcati.

Il
[menu attributi](#attributes-menu)
elenca i comandi che cambiano i permessi, il proprietario e gli attributi del
filesystem degli stessi file.

Il
[menu comando](#command-menu)
elenca le azioni più generali e non ha relazione con il file correntemente
selezionati o marcati.

Il
[menu opzioni](#options-menu)
elenca le azioni che permettono di personalizzare il M-Commander.

## Menu sinistra e destra (sopra e sotto) <a id="left-and-right-menus"></a>

L'apparenza dei pannelli directory è modificabile tramite i menu
**sinistra**
e
**destra**
(vengono chiamati
**sopra**
e
**sotto**
se la divisione pannello nella finestra
[aspetto](#layout)
del menu opzioni è orizzontale).

### Modalità lista... <a id="listing-format"></a>

La modalità lista serve a mostrare un elenco di file; ci sono quattro
modalità elenco disponibili:
**completa**,
**breve**,
**lunga**
e
**definita dall'utente**.
La modalità completa mostra il nome del file, l'ampiezza del file e
la data di modifica.

La modalità breve mostra solo il nome del file in due colonne
(perciò mostrando il doppio del numero dei file che nelle altre
modalità). La modalità lunga è simile a quella del comando
**ls -l**.
La modalità lunga usa tutta l'ampiezza dello schermo.

Se si sceglie il formato definibile dall'utente, è necessario specificare
il formato della vista.

Il formato definibile dall'utente deve cominciare con una specifica
dell'ampiezza del pannello. Questa può essere "half" o "full", che descrive
un pannello di mezza grandezza o completa rispettivamente.

Dopo l'ampiezza del pannello, è possibile specificare la modalità a
due colonne aggiungendo il numero "2" alla stringa di formato.

Dopodiché si aggiunge il nome dei campi con una specifica di ampiezza
opzionale. Questi sono i campi disponibile per la visualizzazione:

**name**
: mostra il nome del file.

**size**
: mostra l'ampiezza del file.

**bsize**
: è una forma alternativa del formato
**size**
mostra l'ampiezza del file e per le directory mostra solo
SUB-DIR o UP--DIR.

**type**
: mostra un campo di un carattere. Questo carattere è simile a quello
mostrato dal comando ls con la flag -F -
**\***
per i file eseguibili,
**/**
per le directory,
**@**
per i collegamenti,
**=**
per i socket,
**-**
per i dispositivi a carattere,
**+**
per i dispositivi a blocchi,
**|**
per le pipe,
**~**
per i collegamenti simbolici a directory e
**!**
per i collegamenti simbolici stallati (che non puntano a niente).

**mark**
: un asterisco se il file è marcato, uno spazio se non lo è.

**mtime**
: la data dell'ultima modifica al file.

**atime**
: la data dell'ultimo accesso al file.

**ctime**
: la data della creazione del file.

**perm**
: una stringa che rappresenta i bit dei permessi del file.

**mode**
: un valore ottale con i permessi correnti del file.

**nlink**
: il numero dei collegamenti al file.

**ngid**
: il GID (numerico).

**nuid**
: l'UID (numerico).

**owner**
: il proprietario del file.

**group**
: il gruppo del file.

**inode**
: l'inode del file.

Puoi usare ache questi campi per sistemare la visualizzazione:

**space**
: uno spazio nel formato visualizzazione.

**|**
: aggiunge una linea verticale al formato di visualizzazione.

Per forzare un campo ad un'ampiezza fissa (una specifica di ampiezza),
basta semplicemente aggiungere
**:**
ed il numero dei caratteri che si vuole che il campo abbia. Se il
numero è seguito dal simbolo
**+**,
allora la specifica definisce l'ampiezza minima - se il programma
trova che serve più spazio sullo schermo, espanderà il campo.

Per esempio la modalità
**completa**
corrisponde a questo formato:

half type name | size | mtime

E quella
**lunga**
corrisponde a questo formato:

full perm space nlink space owner space group space size space mtime
space name

Questa è una modalità interessante:

half name | size:7 | type mode:3

I pannelli possono anche essere impostati alle modalità seguenti:

**Informazioni**
: La modalità informazioni mostra alcuni dati relativi al file
correntemente selezionato e se possibile informazioni circa il file
system corrente.

**Albero**
: La vista ad albero è abbastanza simile al comando
[albero directory](#directory-tree).
Vedere la sezione corrispondente per maggiori informazioni.

**Vista rapida**
: In questa modalità il pannello si imposta come un
[visualizzatore](mview.md#internal-file-viewer)
ridotto che mostra i contenuti del file correntemente selezionato;
se si seleziona il pannello (con il tasto tab o con il mouse), si ha
accesso ai normali comandi del visualizzatore.

### Ordina per... <a id="sort-order"></a>

Gli otto possibili ordinamenti sono per nome, estensione, data
di modifica, data di accesso, data di modifica informazioni di
inode, ampiezza, per inode e non ordinato. Nella finestra di dialogo
di ordinamento è possibile scegliere il tipo di ordinamento ed è anche
possibile specificare se si desidera l'ordinamento inverso selezionando
la voce inverso.

Normalmente le directory sono ordinate prima dei file ma quest'impostazione
può essere modificata dal
[menu opzioni](#options-menu)
(opzione
**mescola tutti i file**).

### Filtro... <a id="filter"></a>

Il comando di filtro permette di indicare un modello (per esempio
**\*.tar.gz**)
a cui i file e le directory devono corrispondere per essere mostrati. La
[riga di ingresso](#input-line-keys)
riceve il modello dei nomi da mostrare nel pannello.

Se la casella
*Solo file*
è attiva, il filtro vale solo per i file e tutte le directory si vedono.
Altrimenti vengono filtrati sia i file sia le directory. Se la casella
*Modelli di shell*
è attiva, il modello funziona come l'espansione dei nomi nella shell (\* sta
per zero o più caratteri e ? per uno). Altrimenti il confronto avviene con le
normali espressioni regolari (vedere ed(1)). Se la casella
*Distingui maiuscole*
è attiva, il filtro distingue maiuscole e minuscole; altrimenti non ne tiene
conto.

### Ricarica <a id="reread"></a>

Il comando ricarica l'elenco dei file nella directory. E' utile
se un'altro processo ha creato o rimosso dei file. Se
si ha pannellizzato dei nomi di file in un pannello, questo ricaricherà
il contenuto della directory e rimuoverà le informazioni pannellizzate
(vedere sezione
[pannellizza comando](#external-panelize)
per ulteriori informazioni).

## Menu file <a id="file-menu"></a>

Il M-Commander usa i tasti F1 - F10 come tasti veloci
per i comandi che appaiono nel menu file. Le sequenze di escape
per i tasti funzione sono capacità terminfo da kf1 a kf10. Su terminali
senza supporto per i tasti funzione, è possibile ottenere la stessa
funzionalità premendo il tasto ESC e un numero da 1 a 9 più lo 0
(corrispondentemente ai tasti da F1 a F9 e F10 rispettivamente).

Il file menu comprende i comandi seguenti (tasti veloci tra parentesi):

**Aiuto (F1)**

Invoca il visualizzatore incorporato di ipertesti per l'aiuto.
All'interno del
[visualizzatore aiuto](#contents),
è possibile usare il tasto tab per selezionare il successivo collegamento
e il tasto invio per seguirlo. I tasti Barra spaziatrice e Backspace vengono
utilizzati per muoversi avanti e indietro nella pagina di aiuto. Premere F1
nuovamente per ottenere la lista completa dei tasti accettati.

**Menu (F2)**

Invoca il
[menu utente](#edit-menu-file).
Il menu utente fornisce un modo semplice per dare agli utenti un menu ed
aggiungere nuove funzionalità al M-Commander.

**Visualizza (F3, Maiusc-F3)**

Visualizza il file correntemente selezionato. Nell'impostazione predefinita
viene invocato il
[visualizzatore interno di file](mview.md#internal-file-viewer)
ma se l'opzione "Usa visualizzatore interno" è deselezionata, verrà invocato
un visualizzatore esterno specificato dalla variabile ambiente
**PAGER**.
Se
**PAGER**
non è definita, verrà invocato il comando "view". Se si usa invece il comando
Maiusc-F3, il visualizzatore verrà invocato senza nessun tipo di formattazione
o preprocessamento sul file.

**Vista filtrata (M-!)**

Questo tasto richiede all'utente un comando ed i suoi argomenti (l'argomento
predefinito è il nome del file attualmente selezionato), il risultato di tale
comando viene mostrato nel visualizzatore di file interno.

**Cambia (F4)**

Invoca l'editor
**vi**
o l'editor specificato nella variabile d'ambiente
**EDITOR**
oppure
[l'editor di file interno](mcedit6.md#internal-file-editor)
se l'opzione, "usa editor interno" è stata impostata.

**Copia (F5)**

Mostra una finestra di dialogo con destinazione predefinita alla
directory del pannello non selezionato, che copia il file selezionato (o
i file marcati, se ce n'è almeno uno) sulla directory specificata
dall'utente nella finestra di dialogo. Space for destination
file may be preallocated relative to preallocate_space configure option.
Durante il processo è possibile
premere C-c o ESC per abortire l'operazione. Per maggiori dettagli sulla
maschera sorgente (che sarà normalmente \* o ^\\(.\*\\)$ a seconda
dell'impostazione di "modelli della shell") o sui caratteri jolly sulla
destinazione vedere
[maschera copia/rinomina](#mask-copyrename).

In alcuni sistemi è possibile eseguire la copia in background cliccando
sul bottone background (o premendo M-b nella finestra di dialogo). Il
comando
[processi in background](#background-jobs)
è utile per controllarne l'andamento.

**Collegamento (C-x l)**

Crea un collegamento fisico (hard link) al file corrente.

**Collegamento Simbolico (C-x s)**

Crea un collegamento simbolico al file corrente. Per chi non sapesse
cosa sono i collegamenti: creare un collegamento ad un file è come
copiare il file ma sia il nome sorgente che destinazione rappresentano
la stessa immagine fisica del file. Per esempio, se si modifica uno dei
due file, tutti i cambiamenti appariranno su tutti i file. Alcuni li
chiamano anche alias o scorciatoie (o link come in originale inglese).

Un collegamento fisico appare come un file reale. Dopo che sia stato
creato non c'è modo di distinguere quale sia il collegamento e quale sia
l'originale. Se si cancella uno dei due l'altro rimarrà intatto. E' molto
difficile notare che i file rappresentano la stessa immagine. Usate i
collegamenti fisici quando non volete proprio saperlo.

Un collegamento simbolico è un riferimento al nome del file originale.
Se il file originale viene cancellato, il collegamento è inutile.
E' facile distinguere i collegamenti simbolici dall'immagine stessa.
Se il file è un collegamento simbolico a qualcosa, il M-Commander
mostra un simbolo "@" davanti al nome del file (eccetto se punta ad una
directory, nel qualcaso mostrerà una tilde  (~)).
Il file originale sul quale punta il collegamento simbolico viene mostrato
sulla riga di mini-stato se
*Mostra Mini-stato*
è abilitata. Usare i collegamenti simbolici se si vuole evitare la confusione
che creano i collegamenti fisici.

**Rinomina/Sposta (F6)**

Mostra una finestra di dialogo con destinazione predefinita alla
directory del pannello non selezionato, che sposta il file selezionato (o
i file marcati, se ce n'è almeno uno) sulla directory specificata dall'utente
nella finestra di dialogo. Durante il processo è possibile
premere C-c o ESC per abortire l'operazione. Per maggiori dettagli vedere la
sezione precedente Copia, dato che il comando è molto simile.

In alcuni sistemi è possibile eseguire la copia in background cliccando
sul bottone background (o premendo M-b nella finestra di dialogo). Il
comando
[processi in background](#background-jobs)
è utile per controllarne l'andamento.

**Crea Directory (F7)**

Mostra una finestra di dialogo che crea la directory specificata.

**Elimina (F8)**

Cancella il file correntemente selezionato o i file marcati nel
pannello corrente. Durante il processo è possibile premere C-c
o ESC per abortire l'operazione.

**Cambia dir veloce (M-c)**
Usare il comando
[Cambia Dir veloce](#quick-cd)
se si vuole cambiare directory corrente e si ha la riga di comando occupata.

**Seleziona gruppo (+)**

Viene utilizzato per selezionare (marcare) un gruppo di file. Il
M-Commander richiedera un'espressione regolare per descrivere il
gruppo; se l'opzione
*modelli della shell*
è abilitata, l'espressione regolare è simile al file globbing nella shell
(\* significa zero o più caratteri e ? significa un carattere). Se l'opzione
*modelli della shell*
è disabilitata, allora la selezione dei file viene eseguita con le normali
espressioni regolari (vedere ed (1)).

**Deseleziona gruppo (\\)**

Usata per deselezionare un gruppo di file. E' l'opposto di del comando
*Seleziona gruppo.*

**Uscita (F10, Maiusc-F10)**

Termina l'esecuzione del M-Commander. Maiusc-F10 viene usata se
si esce e si sta usando lo shell wrapper. Maiusc-F10 in tal caso non
vi porterà all'ultima directory utilizzata dal M-Commander ma
vi lascerà nella directory dalla quale avete fatto partire il
M-Commander.

### Cambia dir veloce <a id="quick-cd"></a>

Questo comando è utile se si ha la riga di comando piena e si vuole
eseguire
[cd](#the-cd-internal-command)
per cambiare directory senza dover cancellare e riscrivere la riga di comando.
Questo comando fa uscire una piccola finestra di dialogo che richiede
l'immissione degli stessi argomenti che si darebbero al comando
**cd**
a riga di comando. Questo ha le stesse caratteristiche già presenti nel
comando
[comando interno cd](#the-cd-internal-command).

## Menu attributi <a id="attributes-menu"></a>

I comandi di questo menu cambiano ciò che il filesystem sa del file, non il suo
contenuto: i permessi di accesso, il proprietario e il gruppo, e gli attributi
del filesystem. Ognuno agisce sul file selezionato, o sui file marcati se ce ne
sono.

**Permessi... (C-x c)**
: Cambia i permessi di accesso nella finestra
[Permessi](#chmod).

**Proprietario... (C-x o)**
: Cambia il proprietario e il gruppo nella finestra
[Proprietario](#chown).

**Proprietario avanzato...**
: Cambia i permessi, il proprietario e il gruppo in una sola finestra, vedere
[Proprietario avanzato](#advanced-chown).

**Attributi chattr... (C-x e)**
: Cambia gli attributi di un filesystem ext2, ext3 o ext4 nella finestra
[Attributi dei file](#chattr). La voce c'è solo se il programma è compilato con
il supporto per quegli attributi.

## Menu comando <a id="command-menu"></a>

Il comando
[albero directory](#directory-tree)
mostra un disegno ad albero delle directory.

Il comando
[trova file](#find-file)
permette di cercare un file specifico. Il comando "Scambia pannelli"
scambia il contenuto dei due pannelli directory.

Il comando "attiva/disattiva pannelli" mostra il risultato dell'ultimo
comando shell. Quest'ultimo funziona solo su xterm e sulle console Linux
e FreeBSD.

Il comando Confronta directory (C-x d) confronta il contenuto dei
pannelli directory uno con l'altro. E' poi possibile usare il comando
Copia (F5) per rendere i pannelli identici. Ci sono tre metodi di
confronto. Il metodo veloce confronta solo l'ampiezza e la data del
file. Il metodo completo fa un confronto byte-per-byte. Il metodo
solo dimensione confronta solo l'ampiezza dei
file e non controlla il contenuto né la data del file.

Il comando cronologia comandi mostra un'elenco dei comandi battuti. Il
comando selezionato viene copiato sulla riga di comando. Alla cronologia
comandi vi si  può accedere premendo M-p o M-n.

Il comando
[directory favorite](#hotlist) (C-\\)
permette un cambio più veloce dalla directory corrente ad una di quelle usate
più spesso.

Il comando
[pannellizza comando](#external-panelize)
permette di eseguire un coamndo esterno e di mettere il risultato nel pannello
corrente.

### Albero directory <a id="directory-tree"></a>

Il comando albero directory mostra una rappresentazione ad albero delle
directory. Selezionando una directory dalla rappresentazione il
M-Commander cambierà a quella directory.

Ci sono due modi di invocare l'albero. Il vero comando di albero directory
è accessibile dal menu Comandi. L'altro modo è di selezionare la vista ad
albero dai menu Sinistra o Destra.

Per evitare i lunghi ritardi il M-Commander crea la rappresentazione ad
albero scansionando solo una piccola porzione di tutte le directory.
Se manca la directory che si vuole visualizzare, spostarsi sulla sua directory
genitrice e premere C-r (o F2).

E' possibile utilizzare i tasti seguenti:

Sono accettati i
[tasti generali di movimento](#general-movement-keys).

**Invio.**
Nell'albero directory, esce dall'albero della directory e lo cambia
alla directory corrente nel pannello selezionato. Nella vista ad albero,
cambia a questa directory nell'altro pannello e rimane nella modalità
vista ad albero in quello corrente.

**C-r, F2 (Ricarica).**
Ricarica la directory. Usare questo comando quando la rappresentazione ad
albero non è aggiornata: mancano directory o mostra alcune sottodirectory
che non esistono più.

**F3 (Scorda).**
Cancella questa directory dalla rappresentazione ad albero. Usare questo
comando per eliminare la confusione dal'albero. Se si vuole nuovamente
visualizzare l'albero completo premere F2 nella sua directory genitrice.

**F4 (Statico/Dinamico).**
Cambia tra modo di navigazione dinamico (predefinito) e statico.

Nella navigazione statica si usano i tasti Su e Giù per
selezionare la directory. Tutte le directory conosciute vengono mostrate.

Nella navigazione dinamica si usano i tasti Su e Giù per selezionare
una directory sorella, il tasto Sinistra sposta sulla directory genitrice
e il tasto Destra sposta sulla directory figlia. Solo i parenti, sorelle
e figlie, vengono mostrate; le altre sono tralasciate. La rappresentazione
ad albero cambia dinamicamente come la si attraversa.

**F5 (Copia).**
Copia la directory.

**F6 (RinSpo).**
Sposta la directory.

**F7 (CreDir).**
Crea una nuova directory sotto questa directory.

**F8 (CancDir).**
Cancella questa directory dal file system.

**C-s, M-s.**
Cerca la prossima directory che corrisponde alla stringa di ricerca.
Se tale directory non esiste, questi tasti faranno scendere di una riga
(il cursore).

**C-h, Backspace.**
Cancella l'ultimo carattere nella stringa di ricerca.

**Qualsiasi altro carattere.**
Aggiunge un carattere alla stringa di ricerca e sposta alla nuova directory
che comincia con questi caratteri (il cursore). Nella vista ad albero
si deve prima attivare la ricerca premendo C-s. La stringa di ricerca è
visibile nella riga di mini stato.

Le azioni seguenti sono disponibili solo nell'albero directory.
Non sono supportate nella vista ad albero.

**F1 (Aiuto).**
Invoca il visualizzatore dell'aiuto e mostra questa sezione.

**Esc, F10.**
Esce dalla rappresentazione ad albero. Non cambia directory.

Il mouse è supportato. Un doppio clic si comporta come premere Invio.
Vedere anche la sezione
[supporto mouse](#mouse-support).

### Trova file <a id="find-file"></a>

Il comando Trova file chiede prima la directory da cui iniziare la ricerca e
poi il nome del file da cercare. Premendo il pulsante Albero si può scegliere
la directory iniziale dall'
[albero directory](#directory-tree).

Il campo "Nome file" contiene il modello del nome da cercare. Viene
interpretato come modello di shell o come espressione regolare a seconda
dello stato della casella "Usa modelli di shell". Un valore vuoto è valido e
corrisponde a qualsiasi nome.

Il campo "Contenuto" contiene il testo da cercare dentro i file. Lasciandolo
vuoto non si cerca nel contenuto.

L'opzione "Parole intere" limita la ricerca ai file in cui la parte trovata
forma una parola intera, come fa grep -w.

La ricerca si avvia con il pulsante Ok. Durante la ricerca la si può fermare
con il pulsante Ferma e riprenderla con il pulsante Continua.

L'elenco mostra per ogni file trovato la data di modifica, la dimensione e i
permessi insieme al nome. Quando si cerca nel contenuto, un file compare una
sola volta: un'unica occorrenza si vede accanto al nome come
"file.c:12", mentre un file con più occorrenze ne mostra il numero ed è
marcato con "[+]". Le occorrenze di quel file si aprono con il tasto Sinistra
o con un clic sul segno: il numero di riga e la riga stessa. Lì Invio porta
al file, F3 lo mostra e F4 lo modifica all'occorrenza scelta.

Nell'elenco ci si muove con i tasti freccia. Il pulsante Cambia dir va alla
directory del file scelto. Il pulsante Ancora chiede i parametri di una nuova
ricerca. Il pulsante Esci chiude la ricerca. Il pulsante Pannellizza mette i
file trovati nel pannello corrente, così da poterci fare altre operazioni
(vedere, copiare, spostare, eliminare e così via). Per tornare all'elenco
normale basta andare nella directory ".."; per rivedere il risultato
pannellizzato si sceglie la voce Pannellizza nel menu sinistra o destra.

La casella "Ignora directory" e il campo sotto di essa indicano l'elenco
delle directory che la ricerca deve saltare (per esempio un CD-ROM o una
directory NFS montata su un collegamento lento). Gli elementi dell'elenco si
separano con i due punti:

```
/cdrom:/nfs/wuarchive:/afs
```

Sono ammessi anche percorsi relativi. L'esempio seguente salta anche le
directory dei sistemi di controllo di versione:

```
/cdrom:/nfs/wuarchive:/afs:.svn:.git:CVS
```

Attenzione: il campo può contenere un punto (.), che significa il percorso
assoluto corrente.

Per alcune operazioni vale la pena usare il comando
[Pannellizza comando](#external-panelize).
Trova file serve per ricerche semplici, mentre con Pannellizza comando si
possono fare ricerche di ogni complessità.

### Pannellizza comando <a id="external-panelize"></a>

Pannellizza comando permette di eseguire un programma esterno, e
mettere il risultato del programma nel pannello corrente.

Per esempio, se si vuole manipolare in uno dei pannelli tutti i collegamenti
simbolici nella directory corrente, basta usare pannellizza comando per
eseguire il seguente:

```
find . -type l -print
```

Al completamento del comando, il contenuto del pannello non sarà più
il listato della directory ma tutti i file che rappresentano
collegamenti simbolici.

Volendo pannellizzare tutti i file che sono stati scaricati dal
proprio server ftp preferito, si può usare questo comando awk per
estrarre il nome del file dal file di log del traferimento:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

Se si desidera si può salvare i comandi di pannellizzazione usati più
spesso con uno nome più descrittivo, in maniera da richiamarli più velocemente.
Per fare ciò basta battere il comando sulla riga di ingresso e premere il tasto
Aggiungi nuovo. Poi si deve dare un nome al quale associare il comando che si
desidera salvare. La prossima volta sarà possibile scegliere quel comando
dall'elenco e non servirà ribatterlo nuovamente.

### Directory favorite <a id="hotlist"></a>

Il comando Directory favorite mostra le etichette dei posti nell'elenco delle
favorite, e il programma va al posto dell'etichetta scelta. Un posto può
essere una directory, un percorso dentro un filesystem virtuale o
l'indirizzo di un componente di pannello, per esempio
*sftp:host/dir.*
Dalla finestra si possono rimuovere le coppie etichetta/posto già create e
aggiungerne di nuove. Il posto corrente, directory o pannello di un
componente, si aggiunge più in fretta con il comando Aggiungi alle favorite
(C-x h), che chiede solo l'etichetta. Un posto già presente nell'elenco non
viene aggiunto due volte: la finestra mostra la voce esistente.

Tasti della finestra:

```
Invio        va al posto scelto
Alt-o        apre il posto scelto nell'altro pannello
Ctrl-Invio   mette "cd posto" nella riga di comando
Alt-Invio    lo stesso, per terminali senza Ctrl-Invio
Ins          aggiunge il posto corrente
Maiusc-F4    nuova voce: chiede etichetta e posto
F7           nuovo gruppo
F4           modifica etichetta e posto della voce
Canc         elimina la voce
Ctrl-Su      sposta la voce una riga in su
Ctrl-Giù     sposta la voce una riga in giù
F6           sposta la voce in un altro gruppo: la finestra
             elenca i gruppi, Invio ne apre uno, Sposta o un
             altro F6 mette la voce in fondo al gruppo mostrato,
             compreso quello di partenza
F9           ordina il gruppo corrente per etichetta, prima i
             gruppi
Ctrl-s       cerca nell'elenco mentre si scrive, Ctrl-s ripete
Destra, Sin  entra in un gruppo ed esce da esso
```

Questo rende più veloce il cd verso directory usate spesso. Considera l'uso
della variabile CDPATH come descritto in
[comando cd interno](#the-cd-internal-command).

### Processi in background <a id="background-jobs"></a>

Questo comando permette di controllare lo stato di ogni processo
in background del M-Commander (in background possono essere
eseguite solo operazioni di copia e rinomina). Da qui si può bloccare,
far ripartire e uccidere un lavoro in background.

### Modifica file menu <a id="edit-menu-file"></a>

Il menu utente è un menu di comandi utili che l'utente personalizza. Si
presenta in due forme: il menu che si modifica da sé, tenuto in un file di
chiavi, e il vecchio file di menu scritto a mano. Dove esiste un file di
chiavi, è quello il menu che F2 apre; dove non c'è, viene letto il vecchio
file come sempre.

**Il menu che si modifica da sé**

Le voci stanno nel file .mc6menu della directory corrente e in
~/.config/mc6/menu.ini, e vengono mostrate insieme. Un .mc6menu viene letto
solo se appartiene a questo utente o a root e nessun altro può scriverci,
perché le sue voci eseguono comandi. Nient'altro viene letto: nel menu c'è
ciò che il suo proprietario vi ha messo, e il programma non porta con sé
alcuna voce, quindi il menu di un utente nuovo è vuoto e chiede la prima.
Dentro il menu:

```
Invio        esegue la voce
Ins          aggiunge una voce
F4           modifica la voce
F5           importa voci da un menu scritto a mano
Maiusc-F4    apre il file in cui sta la voce
Canc         elimina la voce
Ctrl-Su      sposta la voce in su
Ctrl-Giù     sposta la voce in giù
Alt-A        mostra anche le voci nascoste qui
```

Una voce è fatta di un tasto di scelta rapida, un'etichetta, i comandi e le
condizioni che dicono dove viene mostrata. Nei comandi valgono le stesse
sostituzioni del vecchio menu, %f, %s, %{prompt} e le altre, descritte in
[sostituzione di macro](#macro-substitution).
Due caselle dicono che cosa fare dell'uscita: se vada al visualizzatore e se
il comando venga eseguito senza la shell del pannello.

La riga "Show when" della finestra mostra le condizioni della voce, e il
pulsante Conditions apre una finestra per esse: maschere del percorso, i
tipi di file a cui la voce è destinata (nessuna casella marcata vuol dire
qualsiasi tipo), solo file eseguibili, solo quando ci sono
file marcati, i programmi di cui i comandi hanno bisogno, il pannello a cui
le condizioni guardano, e se il menu si apre su questa voce: mai, sempre,
o quando valgono le condizioni che apre il pulsante accanto a "When". Una
voce le cui condizioni non valgono non è nell'elenco. La finestra mostra
un pannello alla volta; le condizioni che guardano entrambi i pannelli si
cambiano nel file.

Con la casella "Regular expression" spuntata, il campo del percorso
accetta un'espressione regolare al posto delle maschere; nel file è
path~=.

Il titolo del menu dice quante voci sono nascoste in quel punto. Alt-A
mostra anche quelle, in un colore attenuato: lì si possono modificare,
spostare e cancellare, ma non eseguire.

L'etichetta viene mostrata con le sostituzioni già fatte, così un'etichetta
"print %f" sta nell'elenco con il nome del file sotto il cursore. Quello che
il file contiene non cambia, e %{...} resta com'è scritto: un elenco non è il
posto per fare domande.

Una voce può essere un sottomenu invece che un comando: Ins chiede quale
delle due aggiungere. Un sottomenu si vede con una barra dopo il nome, come
una directory; Invio lo apre, e il titolo nomina i sottomenu in cui ci si
trova. Esc sale di un livello, e al primo esce dal menu. Eliminare un
sottomenu elimina anche ciò che contiene. Nel file un sottomenu è un gruppo
con submenu=true e senza comando, e una voce al suo interno nomina il
sottomenu in parent=.

I comandi sono un campo di più righe: Invio ne apre una nuova, e le frecce,
Inizio e Fine percorrono il testo. Maiusc con un movimento seleziona ciò su
cui il movimento passa, il mouse seleziona trascinando, e Ctrl-Ins,
Maiusc-Ins e Maiusc-Canc copiano, incollano e tagliano attraverso il file di
scambio, come in una riga di ingresso. Il pulsante Editor esce dalla finestra
e apre il file in cui sta la voce, per ciò che è più comodo scrivere lì.

Una voce viene riscritta nel file da cui è venuta, e l'ordine dell'elenco è
quello del file. Viene scritto solo ciò che è cambiato: i commenti, le righe
vuote e le chiavi che il menu non conosce restano come sono, e un commento
sopra una voce si sposta con essa. Maiusc-F4 apre il file.

**Il file del menu**

Il file è fatto per essere letto e cambiato anche a mano. Ogni voce è un
gruppo: la sua etichetta tra parentesi quadre, poi una chiave=valore per
riga. Un valore di una sola riga sta dopo "=" così com'è, senza virgolette
né sequenze di escape. Un comando di più righe sta tra due righe di tre
apici inversi, e il testo tra di esse viene preso esattamente come è
scritto:

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

Se i comandi contengono essi stessi una riga di tre apici inversi, il
blocco si apre e si chiude con quattro. Le righe che cominciano con '#'
sono commenti. La prima riga è sempre "# mc menu format 2", e il menu
legge solo i file che la contengono. Per un file senza di essa, scritto da
una versione precedente o a mano, il menu propone di convertirlo una volta
e conserva il file com'era in menu.ini.old.

Queste chiavi dicono dove una voce viene mostrata. Se ci sono più chiavi,
devono valere tutte.

*path=*
: Maschere del percorso di ciò su cui sta il cursore, lette nel modo in
cui il file .gitignore di git legge i suoi modelli; vedere sotto.

*path~=*
: Un'espressione regolare al posto delle maschere, cercata in qualunque
punto del percorso: path~=^/home/me/dev/. Senza '/' guarda solo l'ultima
parte del percorso: path~=^ttyS. Un '!' davanti la inverte. Il valore è
una sola espressione, quindi ';' e '|' ne fanno parte.

*on=*
: Ciò su cui sta il cursore: file, dir, link, broken, char, block, fifo,
socket, separati da ';'; ne basta uno qualsiasi. Un collegamento a una
directory conta come directory, un collegamento a un file come file, e
link corrisponde a qualsiasi collegamento. Un '!' rovescia un tipo:
on=!dir è qualsiasi cosa tranne una directory.

*exec=true*
: Il file sotto il cursore non è una directory ed è eseguibile.

*marked=true*
: Il pannello ha file marcati; marked=false, non ne ha.

*needs=*
: Programmi che devono essere trovati nel PATH, o percorsi completi; tutti
quanti.

*other.path=, other.path~=, other.on=, other.exec=, other.marked=*
: Lo stesso per l'altro pannello.

*default=true*
: Il menu si apre su questa voce ovunque essa sia mostrata.

*default.path=, default.on=, default.other.path=, ...*
: Qualsiasi chiave qui sopra con "default." davanti: il menu si apre su
questa voce dove queste chiavi valgono, e la voce è comunque mostrata dove
dicono le sue chiavi. default.path=ttyS\* apre il menu sulla voce quando il
cursore sta su una porta seriale. Dove si possono scegliere più voci, vince
la prima.

Una maschera di path= viene confrontata con il percorso di ciò su cui sta
il cursore, nel modo in cui .gitignore confronta i suoi modelli:

```
*.c               un file .c in qualunque punto
~/dev/mc/*.c      un file .c direttamente in ~/dev/mc
~/dev/mc/**/*.c   un file .c lì o in una directory sotto
~/dev/mc/**       qualsiasi cosa in quell'albero
**/src/*          qualsiasi cosa direttamente in una src
build/            una directory di nome build
*.c;!test_*.c     i sorgenti C tranne i test
```

Una maschera senza '/' viene confrontata solo con l'ultima parte del
percorso, a qualsiasi livello: \*.1 vale su una directory rrr.1, ma non sul
file uu.2 al suo interno. Una maschera con un '/' all'inizio viene
confrontata con il percorso intero; una con un '/' nel mezzo parte dalla
directory di .mc6menu, e in menu.ini vale a qualsiasi livello. '\*' e '?'
non passano oltre un '/', '\*\*' passa per qualsiasi numero di directory,
[abc] e [a-z] sono insiemi di caratteri, un '/' alla fine chiede una
directory, e '~' all'inizio è la directory home. A differenza di
quanto fa .gitignore, una maschera che vale su una directory non dice
nulla dei file che contiene.

Le maschere vengono lette da sinistra a destra, e decide l'ultima che
corrisponde; una maschera con '!' davanti dice no. Dove nessuna maschera
corrisponde, la voce non viene mostrata: !\*.o da sola non mostra nulla, e
"qualsiasi cosa tranne i file oggetto" è \*;!\*.o. Su "..", il percorso è
la directory del pannello seguita da "/..", quindi una maschera di quella
directory vale su di esso, e una maschera di un nome no.

Un file che il menu non riesce a leggere non viene mostrato come un menu
vuoto: un messaggio indica la riga sbagliata e offre di aprire il file, e il
menu non scrive nel file finché non viene corretto.

**Il file di menu scritto a mano**

L'installazione non ne porta più; quello che segue viene letto dove qualcuno
tiene un menu proprio nella forma precedente: viene usato il file .usermenu
della directory corrente, se esiste, ma solo se è di proprietà dell'utente o
di root e non è scrivibile da tutti. Se tale file non c'è, allo stesso modo
si prova con ~/.config/mc6/menu.

Se il menu che si modifica da sé non ha ancora un file e ne viene trovato un
altro (quello proprio scritto a mano, il usermenu di un mcommander installato
o un menu.ini di una versione precedente), il programma offre una volta per
sessione di importarlo; F5 nel menu, e il pulsante Importa del menu vuoto, lo
chiedono in qualsiasi momento, per quel file o per uno indicato a mano. Poi
mostra che cosa il file contiene: lo spazio marca una voce, Ins la marca e
scende, '\*' inverte tutte le marcature, e Invio porta quelle marcate in
~/.config/mc6/menu.ini, dove si possono già modificare con la finestra. Il
file di origine resta dov'è.

Le condizioni sopra una voce diventano le sue chiavi dove le chiavi le
sanno dire: "+ f \\.c$ | f \\.h$ & t r" diventa path=\*.c;\*.h e on=file,
e un'espressione regolare diventa maschere dove esse corrispondono agli
stessi nomi. Una riga "=", che sceglieva la voce su cui il menu si apre,
diventa allo stesso modo chiavi default.*. Una condizione che le chiavi
non sanno dire, come un'alternativa dopo un "&", resta come commento sopra
la voce, e l'importazione dice quante ce n'erano.

Un'espressione regolare che nessuna maschera può dire passa così com'è
in path~=.

Il formato del file menu è molto semplice. Le righe che cominciano
con qualsiasi cosa che non sia uno spazio o una tabulazione sono
considerate voci per il menu (per fare in modo di usarle anche come
scelta rapida, il primo carattere deve essere una lettera). Tutte le
righe che cominciano con uno spazio o un tab sono i comandi che verranno
eseguiti quando la voce viene selezionata.

Quando un'opzione viene selezionata tutte le linee di comando dell'opzione
vengono copiate in un file temporaneo nella directory temporanea (normalmente
/usr/tmp) e poi il file viene eseguito. Ciò permette all'utente di mettere
normali costrutti shell nei menu. Prima dell'esecuzione del codice del menu
ha luogo una semplice sostituzione di macro. Per ulteriori informazioni vedere
[sostituzione macro](#macro-substitution).

Ecco un esempio di un file usermenu:

```
A	Mostra un dump del file correntemente selezionato
	od -c %f

B	Modifica un rapporto bachi e lo spedisce a root
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
	vi $I
	mail -s "M-Commander bug" root < $I
	rm -f $I

M	Legge la posta
	emacs -f rmail

N	Legge le news 
	emacs -f gnus

H	Chiama il visualizzatore ipertestuale info
	info

J	Copia la directory corrente nell'altro pannello ricorsivamente
	tar cf - . | (cd %D && tar xvpf -)

K	Crea un rilascio della directory corrente
	echo -n "Nome del file di distribuzione: "
	read tar
	ln -s %d `dirname %d`/$tar 
	cd ..
	tar cvhf ${tar}.tar $tar

= f *.tar.gz | f *.tgz & t n
X       Estrae il contenuto di un file tar compresso
	tar xzvf %f
```

**Condizioni Predefinite**

Ogni voce di menu può essere preceduta da una condizione. La condizione
deve cominciare nella prima colonna con un carattere '='. Se la condizione
è vera, la voce di menu sarà la voce predefinita.

```
Sintassi condizione: 	= <sotto-cond>
  oppure:		= <sotto-cond> | <sotto-cond> ...
  oppure:		= <sotto-cond> & <sotto-cond> ... 

Sotto-condizione è una delle seguenti:

  y <modello>	        sintassi della corrispondenza modello file corrente?
                        (solo per modifica menu).
  f <modello>	        corrispondenza modello file corrente?
  F <modello>	        corrispondenza modello altro file?
  d <modello>	        corrispondenza modello directory corrente?
  D <modello>	        corrispondenza modello altra directory?
  t <tipo>		file corrente di tipo?
  T <tipo>		altro file di tipo?
  x <nomefile>		nomefile è eseguibile?
  ! <sotto-cond>	nega il risultato di una sotto-condizione
```

Modello è un normale modello della shell o un'espressione regolare,
a seconda dell'opzione modelli della shell. E' possibile scavalcare
il valore globale dell'opzione modelli della shell scrivendo
"shell_patterns=x" sulla prima riga del file menu (dove "x" è 0 o 1).

```
Tipo è uno o più dei seguenti caratteri:

  n	non directory
  r	file regolare 
  d	directory
  l	collegamento
  c	carattere speciale
  b	blocco speciale
  f	fifo (pipe)
  s	socket
  x	eseguibile
  t	marcato
```

Per esempio 'rlf' significa file regolare, collegamento o fifo. Il
tipo 't' è particolare perché agisce sul pannello invece che sul file.
La condizione '=t t' è vera se ci sono file marcati nel pannello corrente
e falsa se non ce ne sono.

Se la condizione comincia con '=?' invece che '=' una traccia di debug
sarà mostrata ogniqualvolta viene calcolato il valore della condizione.

Le condizioni sono calcolate da sinistra a destra. Ciò significa che

```
	= f *.tar.gz | f *.tgz & t n
```

viene calcolata come

```
	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

Ecco un esempio dell'uso delle condizioni:

```
= f *.tar.gz | f *.tgz & t n
L	Elenca i contenuti di un archivio compresso tar
	gzip -cd %f | tar xvf -
```

**Condizioni Addizione**

Se la condizione comincia con '+' (o '+?') invece che '=' (o '=?') è
una condizione addizione. Se la condizione è vera la voce di menu sarà
inclusa nel menu. Se la condizione è falsa la voce di menu non sarà
inclusa nel menu.

E' possibile combinare condizioni predefinite e addizione iniziando
la condizione con '+=' o '=+' (o '+=?' o '=+?' se vuoi una traccia di
debug). Se si vuole usare due differenti condizioni, una per addizionale
e l'altra per predefinita, si può precedere una voce di menu con due righe
di condizione, una che comincia con '+' e l'altra con '='.

I commenti cominciano con '#'. Linee di commento aggiuntive devono cominciare con
'#', spazi o tabulazioni.

## Menu opzioni <a id="options-menu"></a>

Il programma possiede alcune opzioni che si possono abilitare e disabilitare
in varie finestre accessibili da questo menu. Le opzioni sono abilitate se
hanno davanti un asterisco o una "x". Il menu contiene, in quest'ordine:

Il comando
[configurazione](#configuration)
apre una finestra dalla quale si possono cambiare quasi tutte le
impostazioni.

Il comando
[aspetto](#layout)
apre una finestra con le opzioni su come il programma appare sullo schermo.

Il comando
[opzioni dei pannelli](#panel-options)
apre le impostazioni dei pannelli del gestore di file.

Il comando
[modalità dei pannelli file](#panel-modes)
apre l'elenco dei formati di elenco con un nome, dove se ne crea, se ne
modifica e se ne elimina uno.

Il comando
[conferme](#confirmation)
apre una finestra dalla quale si indica per quali azioni si vuole una
richiesta di conferma.

Il comando
[aspetto (skin)](#appearance)
serve a scegliere lo skin.

Il comando
[impara tasti](#learn-keys)
insegna al programma i tasti che alcuni terminali non inviano come si deve.

Il comando
[associazioni dei tasti](#key-bindings)
apre l'elenco delle azioni con i tasti a cui rispondono, dove un tasto si
riassegna e il risultato viene scritto nel file delle associazioni.

Il comando
[analizzatore di tasti](#key-sniffer)
mostra che cosa invia il terminale per il tasto premuto, e l'azione a cui
quel tasto è associato.

I comandi
**Opzioni del confronto**,
[Opzioni del visualizzatore](mview.md#viewer-options)
e
**Opzioni dell'editor**
aprono le finestre dei tre programmi che mostrano un file: il confronto, il
visualizzatore e l'editor. Le stesse finestre stanno nel menu Opzioni di
ciascuno di essi; qui si raggiungono senza aprire prima un file. Il confronto
prende le sue opzioni quando parte, quindi un confronto già sullo schermo
mantiene quelle con cui è stato aperto.

Il comando
[gestione dei componenti](#panel-plugins)
elenca i componenti caricati, ne disattiva uno e ne apre le impostazioni.

Il comando
[modifica file estensioni](#edit-extension-file)
permette di specificare i programmi che devono essere eseguiti quando
si prova ad eseguire, visualizzare, modificare e un mucchio di altre
cose, file con una specifica estensione (la fine del nome del file).

Il comando
**Modifica file gruppo di evidenziazione**
apre il file che dice quali nomi e quali tipi di file il pannello mostra con
quale colore, vedere
[Evidenziazione dei nomi](#filenames-highlight).

Il comando
[salva configurazione](#save-setup)
salva le impostazioni correnti dei menu sinistra, destra e opzioni. Viene
salvato anche un piccolo numero di altre opzioni.

Il comando
**Informazioni**
mostra la versione del programma e chi lo ha scritto.

### Configurazione <a id="configuration"></a>

Le opzioni in questa finestra sono divise in tre gruppi:
Opzioni del pannello, Pausa dopo l'esecuzione e Altre opzioni.

**Opzioni del pannello**

*Mostra file di backup.*
Se abilitata, il M-Commander mostrerà i file che terminano con una tilde.
Altrimenti essi non verranno mostrati (come nell'opzione -B del comando GNU ls).

*Mostra file nascosti.*
Se abilitata, il M-Commander mostrerà tutti i file che cominciano con
un punto (come ls -a).

*Cursore in basso mentre seleziona.*
Se abilitata, la barra di selezione si muoverà in basso dopo aver selezionato
un file (sia con tasto Ins).

*Rilascia menu a cascata.*
Quando quest'opzione è abilitata, la discesa dei menu sarà attivata non appena
si preme il tasto
**F9**.
Altrimenti si otterrà solo il titolo del menu e si dovrà attivare il menu con
i tasti freccia o con i tasti di selezione rapida.
E' raccomandata se si stanno usando i tasti di selezione rapida.

*Mescola tutti i file.*
Se quest'opzione è abilitata, tutti i file e le directory vengono mostrati
mescolati insieme. Se l'opzione è spenta, le directory (e i collegamenti a
sottodirectory) vengono mostrati all'inizio dell'elenco con gli altri file
a seguire.

*Aggiornamento rapido directory.*
Se quest'opzione è abilitata, il M-Commander userà un trucco per
determinare se i contenuti della directory sono cambiati. Il trucco consiste
nel ricaricare la directory solo se l'i-node della directory è cambiato.
Ciò significa che la ricarica accade solo quando i file vengono creati o
cancellati. Se quello che cambia è l'i-node di un file nella directory
(cambia l'ampiezza di un file, cambiano il proprietario o le flag, etc.)
la visualizzazione non viene aggiornata. In questi casi se l'opzione è
abilitata, è necessario ricaricare la directory manualmente (con C-r).

**Pausa dopo l'esecuzione**

Dopo l'esecuzione di comandi, il M-Commander può fermarsi, in
maniera da permettere di esaminare il risultato del comando. Ci sono
tre possibili impostazioni per questa variabile:

> *Mai.*
> Significa che non si vuole vedere il risultato del comando. Se si sta
> usando la console Linux o FreeBSD o un xterm, ci sarà la possibilità di
> vedere il risultato del comando premendo C-o.

> *Su terminali stupidi.*
> Si avrà il messaggio di pausa su quei terminali che non sono in grado di
> mostrare il risultato dell'ultimo comando eseguito (qualsiasi terminale
> che non sia un xterm o una console Linux o FreeBSD).

> *Sempre.*
> Il programma si fermerà dopo l'esecuzione di tutti i comandi.

**Altre opzioni**

*Operazioni prolisse.*
Quest'opzione decide se le operazioni di Copia, Spostamento o Cancellazione
saranno prolisse (cioè se mostreranno una finestra di dialogo per ogni
operazione). Se si ha un terminale lento potresti voler disabilitare
quest'opzione. Viene automaticamente spenta se la velocità del proprio
terminale è inferiore a 9600 bps.

*Calcola totali.*
Se quest'opzione è abilitata, il M-Commander calcolerà i totali
delle ampiezze in byte e il numero totale dei file prima di ogni operazione di
Copia, Spostamento o Cancellazione. Questo genererà una barra di progressione
più accurata a discapito di un po' di velocità. Quest'opzione non ha effetto se
*Operazioni prolisse*
è disabilitata.

*Modelli della shell.*
Normalmente i comandi Seleziona, Deseleziona e Filtro usano espressioni
regolari di tipo shell. Le seguenti conversioni vengono eseguite per
ottenere questo risultato: '\*' viene rimpiazzato da '.\*' (zero o più
caratteri); '?' viene rimpiazzato da '.' (esattamente un carattere) e '.'
dal carattere letterale punto. Se l'opzione è disabilitata, allora le
espressioni regolari sono quelle descritte in ed(1).

*Autosalva configurazione.*
Se quest'opzione è abilitata, quando si esce dal M-Commander le
opzioni configurabili del M-Commander vengono salvate nel file
~/.config/mc6/ini.

*Menu automatici.*
Se quest'opzione è abilitata, il menu utente sarà invocato alla partenza.
Utile per creare menu per utenti non abituati a unix.

*Usa editor interno.*
Se quest'opzione è abilitata, verrà usato l'editor integrato interno per
modificare i file. Se l'opzione è disabilitata, verrà usato l'editor
specificato dalla variabile ambiente
**EDITOR**.
Se nessun editor è stato specificato, verrà usato
**vi**.
Vedere la sezione
[editor di file interno](mcedit6.md#internal-file-editor).

*Usa il visualizzatore interno.*
Se quest'opzione è abilitata, verrà usato il visualizzatore di file
interno per visualizzare i file. Se l'opzione è disabilitata, verrà
utilizzato il visualizzatore specificato dalla variabile ambiente
**PAGER**.
Se il visualizzatore non è definito, verrà usato il comando
**view**.
Vedere sezione
[visualizzatore file interno](mview.md#internal-file-viewer).

*Completamento: visualizza tutto*
Normalmente il M-Commander
mostra tutti i possibili
[completamenti](#completion)
se il completamento è
ambiguo se si preme
**M-Tab**
una seconda volta, la prima completa per quanto possibile
e, in caso di ambiguità, emette un suono. Se si vuole vedere
tutti i possibili completamenti già alla pressione del primo
M-Tab**,**
abilitare quest'opzione.

*Barre che girano.*
Se quest'opzione è abilitata, il M-Commander mostra
una barra rotante nell'angolo in alto a destra come indicatore
di progressione.

*Navigazione stile Lynx.*
Se quest'opzione è abilitata, è possibile usare i tasti freccia per
cambiare automaticamente directory se la selezione corrente è
una subdirectory e se la riga di comando è vuota. Normalmente
quest'opzione è spenta.

*Cd segue i collegamenti.*
Quest'opzione, se impostata, fa in modo che il M-Commander
segua la catena logica delle directory, quando si cambia la directory
corrente in ogni pannello o usando il comando cd. Questo è il
comportamento predefinito di bash. Quando non è impostata, il
M-Commander segue la reale struttura della directory, perciò
eseguendo cd .. se si è entrati in una directory attraverso un
collegamento, ci porterà alla genitrice reale della directory corrente
e non alla directory dov'era il collegamento.

*Cancellazione sicura.*
Se quest'opzione è abilitata, la cancellazione non intenzionale dei file
sarà più difficile. La preimpostazione della finestra di dialogo della
conferma cambia da "Si" a "No". Normalmente quest'opzione è
disabilitata.

### Aspetto <a id="layout"></a>

Questa finestra permette di cambiare l'aspetto generale dello schermo. Le
opzioni sono divise in tre gruppi: "Divisione dei pannelli", "Uscita della
console" e "Altre opzioni".

**Divisione dei pannelli**

Il resto dell'area dello schermo viene usato dai due pannelli. Si può
indicare se l'area sia divisa in direzione
*verticale*
oppure
*orizzontale*.
La divisione si cambia anche con Alt-, (Alt-virgola).

*Divisione uguale.*
Come impostazione predefinita i pannelli hanno la stessa dimensione. Con
questa opzione si può dividere in maniera asimmetrica.

**Uscita della console**

Sulle console Linux o FreeBSD si può indicare quante righe siano visibili
nella finestra di uscita. Questa opzione è disponibile solo sulla console
nativa.

**Altre opzioni**

*Barra dei menu visibile.*
Se attiva, il menu principale è sempre visibile nella riga in alto, sopra i
pannelli. Attiva come impostazione predefinita.

*Riga di comando.*
Se attiva, la riga di comando è disponibile. Attiva come impostazione
predefinita.

*Barra dei tasti visibile.*
Se attiva, le dieci etichette dei tasti F1-F10 stanno nella riga in fondo
allo schermo. Attiva come impostazione predefinita.

*Riga dei suggerimenti visibile.*
Se attiva, i suggerimenti di una riga si vedono sotto i pannelli. Attiva come
impostazione predefinita.

*Titolo della finestra XTerm.*
In un emulatore di terminale per X11 il programma imposta il titolo della
finestra alla directory corrente e lo aggiorna quando serve. Se l'emulatore
di terminale è difettoso e all'avvio o al cambio di directory si vede
un'uscita strana, disattivare questa opzione. Attiva come impostazione
predefinita.

*Mostra lo spazio libero.*
Se attiva, lo spazio libero e quello totale del filesystem corrente si vedono
nella cornice in fondo al pannello. Attiva come impostazione predefinita.

### Conferme <a id="confirmation"></a>

In questo menu è possibile configurare le opzioni di conferma per la
cancellazione e sovrascrittura dei file, esecuzione dei file premendo invio e
per l'uscita dal programma.

### Impara tasti <a id="learn-keys"></a>

Questa finestra insegna al programma le sequenze che il terminale invia per i
tasti funzione, per le frecce e per i tasti di movimento.

Si sceglie con le caselle la combinazione di modificatori (Ctrl, Alt,
Maiusc), poi si preme il pulsante del tasto cercato. Si preme il tasto vero e
proprio e si aspetta che il messaggio di cattura sparisca. La sequenza
imparata compare accanto al pulsante.

**Canc**
\- dimentica un tasto imparato.

**Salva**
\- scrive i tasti imparati in ~/.config/mc6/term/\<TERM>.

**Modifica file del terminale**
\- apre nell'editor il file con le definizioni dei tasti del terminale.

Le vecchie definizioni della sezione [terminal:TERM] di ~/.config/mc6/ini
vengono trasferite da sole al primo avvio.

### Gestione dei componenti <a id="manage-plugins"></a>

I componenti aggiuntivi che il programma ha caricato, in una tabella: il tipo,
il nome e quello che il componente dice di sé. La casella della riga lo
disattiva e lo riattiva; ciò che è disattivato non viene caricato nemmeno la
volta successiva.

**Invio, F4**
: Apre le impostazioni del componente su cui si trova il cursore. Quello che
non ne ha lo dice.

Qui compaiono i
[componenti dei pannelli](#panel-plugins)
insieme a quelli dell'editor e ai pacchetti di script Lua; gli script di un
pacchetto sono mostrati dalla finestra
[Script Lua](#lua-scripts)
delle sue impostazioni.

### Script Lua <a id="lua-scripts"></a>

Gli script di un pacchetto Lua, in una tabella: il nome, l'identificatore,
dove si trova lo script, cosa offre e cosa fa. La casella della riga lo
disattiva e lo riattiva.

**Impostazioni**
: Esegue lo script che porta le impostazioni del pacchetto, se c'è.

### Il file esiste <a id="plugin-file-exists"></a>

Una copia verso il pannello di un componente ha trovato lì un file con quel
nome. La finestra mostra il percorso, la dimensione e la data di ciò che si
copia e di ciò che c'è già, e chiede cosa fare: sovrascriverlo, saltarlo,
riprendere la copia da dove si era fermata, quando il componente sa
riprenderla, oppure interrompere tutta l'operazione.

### Scelta della codifica <a id="codepages-translation"></a>

L'elenco delle codifiche che il programma conosce, preso da
**{{pkgdatadir}}/charsets**.
Sceglierne una dice al programma in quale codifica sono scritti i nomi o il
testo in questione, mentre
**\<Nessuna traduzione>**
li lascia come byte. L'elenco si apre con
**Alt-e**
in un pannello, nel visualizzatore e nell'editor, e con la voce corrispondente
dei loro menu.

### Cronologia della riga di ingresso <a id="history-query"></a>

L'elenco di ciò che è stato scritto prima in una riga di ingresso, dall'ultimo
in poi, che
**Alt-h**
apre per la riga in cui si trova il cursore. Invio mette nella riga la voce su
cui è il cursore, Esc lascia la riga com'era e
**F8, Canc**
toglie la voce dalla cronologia.

### Opzioni dei pannelli <a id="panel-options"></a>

**Opzioni principali dei pannelli**

*Mostra mini-stato.*
Se attiva, in fondo ai pannelli si vede una riga di informazioni sulla voce
sotto il cursore. Attiva come impostazione predefinita.

*Unità di misura SI.*
Se attiva, il programma usa i prefissi SI (base 10) per le dimensioni. Se
disattiva (predefinito), usa i prefissi IEC (base 2).

*Mescola tutti i file.*
Se attiva, file e directory si vedono mescolati. Se disattiva (predefinito),
le directory (e i collegamenti a directory) stanno all'inizio dell'elenco e
gli altri file sotto.

*Mostra i file di backup.*
Se attiva, si vedono anche i file che finiscono con una tilde, altrimenti no
(come l'opzione -B di ls). Attiva come impostazione predefinita.

*Mostra i file nascosti.*
Se attiva, si vedono anche i file che iniziano con un punto (come ls -a).
Disattiva come impostazione predefinita.

*Ricarica veloce delle directory.*
Se attiva, il programma usa un trucco per capire se il contenuto della
directory è cambiato: rilegge la directory solo se il suo i-node è cambiato,
cioè quando un file viene creato o eliminato. Se cambia l'i-node di un file
(dimensione, modo, proprietario), la vista non si aggiorna; in quel caso
bisogna rileggere a mano (con C-r). Disattiva come impostazione predefinita.

*La marcatura scende.*
Se attiva, la barra di selezione scende quando si marca un file (con il tasto
Ins). Attiva come impostazione predefinita.

*Inverti solo i file.*
Se attiva, "Inverti selezione" del menu File vale solo per i file e non anche
per le directory. Attiva come impostazione predefinita.

*Scambio semplice.*
Se entrambi i pannelli mostrano un elenco di file, lo scambio semplice
significa che i pannelli si scambiano di posto sullo schermo: quello sinistro
diventa il destro e viceversa. Se disattiva, i due pannelli si scambiano il
contenuto mantenendo formato e ordinamento. Disattiva come impostazione
predefinita.

*Salvataggio automatico delle impostazioni dei pannelli.*
Se attiva, all'uscita il programma salva le impostazioni correnti dei
pannelli nel file ~/.config/mc6/panels.ini. Disattiva come impostazione
predefinita.

*Sorveglia le directory.*
Se attiva, il programma chiede al kernel di essere avvisato dei cambiamenti
nelle directory mostrate dai pannelli, e rilegge un pannello quando un file
al suo interno viene creato, eliminato o cambiato da qualcos'altro: un altro
terminale, una compilazione o la shell della finestra. Un pannello fuori
schermo viene riletto quando torna. Una sorveglianza copre un'intera
directory, quindi il costo non dipende dal numero di file, e una raffica di
cambiamenti porta a una sola rilettura. Le directory di un filesystem
virtuale e quelle su un filesystem che il kernel non può sorvegliare, come
NFS, funzionano come prima: lì è C-r a fare il lavoro. Mentre questa opzione è
attiva, la ricarica veloce non ha nulla da risparmiare e si vede disattivata.
Attiva come impostazione predefinita.

**Navigazione**

*Movimento stile lynx.*
Se attiva, si possono usare i tasti freccia per cambiare directory quando
sotto il cursore c'è una sottodirectory e la riga di comando è vuota.
Disattiva come impostazione predefinita.

*Scorrimento a pagine.*
Se attiva (predefinito), il pannello scorre di mezzo schermo quando il
cursore arriva in fondo o in cima, altrimenti scorre di un file alla volta.

*Scorrimento centrato.*
Se attiva, il pannello scorre quando il cursore arriva a metà, e arriva in
cima o in fondo solo sul primo o sull'ultimo file. Vale per lo scorrimento
file per file, non per i tasti di pagina.

*Scorrimento a pagine con il mouse.*
Stabilisce se la rotellina del mouse scorre i pannelli a pagine o riga per
riga.

**Evidenziazione dei file**

Si può indicare se i
*permessi*
e i
*tipi di file*
debbano essere evidenziati con
[colori](#colors)
distinti. Se l'evidenziazione dei permessi è attiva, le parti dei campi
*perm*
e
*mode*
[del formato di visualizzazione](#listing-format)
che riguardano l'utente che esegue il programma vengono colorate con il
colore definito dalla parola chiave
*marked*.
Se sono attivi i
*colori dei permessi*,
ogni carattere del campo
*perm*
prende il colore di ciò che rappresenta: i colori
*permread ,*
*permwrite ,*
*permexec ,*
*permspecial*
e
*permnone*
dello skin per r, w, x, s/t e -. Le due cose possono valere insieme; la
terna che riguarda l'utente mantiene allora il colore
*marked*.
Se l'evidenziazione dei tipi è attiva, i nomi dei file vengono colorati
secondo le regole descritte nel file
{{sysconfdir}}/mcommander/filehighlight.ini. Per saperne di più vedere
[Evidenziazione dei nomi](#filenames-highlight).

**Ricerca rapida e filtro rapido**

Si può indicare come debbano funzionare la
[ricerca rapida](#quick-search)
e il filtro rapido: senza distinguere le maiuscole, distinguendole, oppure
seguendo l'ordinamento del pannello, che a sua volta può distinguerle o no.

### Aspetto (skin) <a id="appearance"></a>

La scelta della skin che dà l'aspetto al programma. L'elenco mostra le skin
che si trovano in
**{{pkgdatadir}}/skins**
e in
**~/.local/share/mc6/skins**;
quella scelta entra in vigore subito. La struttura delle skin è descritta
nella sezione
[Skins](mcommander.md#skins)
del manuale inglese.

### Modifica file estensioni <a id="edit-extension-file"></a>

Questo comando invocherà l'editor sul file
*~/.config/mc6/extensions.ini.*
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.

### Salva configurazione <a id="save-setup"></a>

Alla partenza il M-Commander prova a caricare le informazioni di
inizializzazione dal file ~/.config/mc6/ini. Se questo file non esiste,
caricherà le informazioni dal file di configurazione di sistema
posizionato in {{pkgdatadir}}/mc.ini. Se il file di configurazione di
sistema non esiste, M-Commander userà le impostazioni predefinite.

Il comando
*salva configurazione*
crea il file ~/.config/mc6/ini salvando le impostazioni correnti
dei menu
[sinistra, destra](#left-and-right-menus)
e
[opzioni](#options-menu).

Se si attiva l'opzione
*autosalva configurazione,*
M-Commander salverà sempre le impostazioni correnti all'uscita.

Esistono anche impostazioni che non possono essere cambiate dai menu.
Per cambiare queste impostazioni è necessario modificare il file di
configurazione con il vostro editor preferito. Vedere sezione
[impostazioni speciali](#special-settings)
per ulteriori informazioni.

<!-- help:break -->

# Esecuzione comandi del sistema operativo <a id="executing-operating-system-commands"></a>

E' possibile eseguire comandi del sistema operativo direttamente
nella riga di comando del M-Commander o selezionando il
programma che si vuole eseguire con la barra di selezione in uno
dei pannelli e premendo Invio.

Se si preme Invio su di un file che non è eseguibile, il
M-Commander confronta l'estensione del file selezionato con ciò
che trova nel
[file estensioni](#edit-extension-file).
Se viene trovata una corrispondenza, verrà eseguito il codice associato.
Verrà eseguita una semplice
[espansione di macro](#macro-substitution)
prima di eseguire il comando.

## Il comando cd interno <a id="the-cd-internal-command"></a>

Il comando
*cd*
non viene passato alla shell per l'esecuzione ma viene interpretato
dal M-Commander. Perciò esso non può gestire tutte quelle
simpatiche espansioni di macro e sostituzioni che fa la shell, malgrado
alcune le possa ancora fare:

*Sostituzione della tilde.*
La (~) verrà sostituita con la vostra directory home e se si appende
un nome utente dopo la tilde, allora verrà sostituita con la directory
di login dell'utente indicato.

Per esempio, ~ospite è la directory home dell'utente ospite, mentre
~/ospite è la directory ospite nella vostra home directory.

*Directory precedente.*
E' possibile saltare alla directory dove si era precedentemente usando
il nome directory speciale '-' così:
**cd -**

*Directory CDPATH.*
Se la directory indicata al comando
**cd**
non è nella directory corrente, il M-Commander userà il
valore della viariabile ambiente
**CDPATH**
per cercare la directory in ognuna delle directory nominate.

Per esempio si può impostare la variabile
**CDPATH**
a ~/src:/usr/src, permettendo di cambiare directory verso ognuna
delle directory presenti nelle directory ~/src e /usr/src da qualunque
parte nel file system (per esempio cd linux vi porterà in
/usr/src/linux).

## Sostituzione di macro <a id="macro-substitution"></a>

Quando si accede ad un
[menu utente](#edit-menu-file),
o si esegue un
[comando dipendente dall'estensione](#edit-extension-file),
o si esegue un comando dalla riga di ingresso,
viene eseguita una semplice sostituzione di macro.

Le macro sono:

*%i*
: Indentazione di spazi, uguale alla colonna della
posizione del cursore. Solo per la modifica menu.

*%y*
: Il tipo di sintassi del file corrente. Solo per la modifica menu.

*%b*
: Nome del file di blocco.

*%e*
: Nome del file di errore.

*%m*
: Nome del menu corrente.

*%fe%p*
: Nome del file corrente.

*%x*
: L'estensione del file corrente.

*%n*
: Nome del file corrente ma senza estensione.

*%d*
: Nome della directory corrente.

*%F*
: Il file corrente nel pannello non selezionato.

*%D*
: La directory corrente nel pannello non selezionato.

*%t*
: I file attualmente marcati.

*%T*
: I file attualmente marcati nel pannello non selezionato.

*%ue%U*
: Simili alle macro %t e %T, in aggiunta i file vengono deselezionati.
E' possibile usare questa macro solo una volta per voce di menu file
o per voce di file estensione, dato che la volta successiva non ci
saranno file marcati.

*%se%S*
: I file selezionati se ce ne sono. Altrimenti il file corrente.

*%cd*
: Questa è una macro speciale usata per cambiare la directory corrente
alla directory specificata di fronte ad essa. Usata principalmente
come interfaccia al
[file system virtuale](#virtual-file-system).

*%view*
: Questa macro serve per invocare il visualizzatore interno. Può essere
usata da sola o con argomenti. Se si passa argomenti a questa macro,
questi dovrebbero essere racchiusi da parentesi.

> Gli argomenti sono:
> *ascii*
> per forzare il visualizzatore in modo ascii;
> *hex*
> per forzare il visualizzatore in modo esadecimale;
> *nroff*
> per dire al visualizzatore che deve interpretare le sequenze di
> grassetto e sottolineato di nroff;
> *unformatted*
> per dire al visualizzatore di non interpretare i comandi nroff
> per rendere il testo grassetto o sottolineato.

*%%*
: Il carattere %

*%{testo}*
: Visualizza una richiesta di sostituzione. Viene mostrata una finestra
contenente il testo all'interno delle graffe. La macro viene sostituita
dal testo immesso dall'utente. L'utente può premere ESC o F10 per annullare.
Questa macro non funziona ancora sulla riga di comando.

*%var{ENV:default}*
: Se la variabile di ambiente
*ENV*
non è impostata, la sostituzione prenderà
*default.*
Altrimenti, verrà sostituito il valore di
*ENV.*

## Il terminale <a id="the-terminal"></a>

Il programma tiene la shell in uno pseudo terminale dietro ai pannelli.
Funziona con le shell bash, ash (BusyBox e Debian), (o/m)ksh, tcsh, zsh e
fish.

La shell è quella definita dalla variabile
**SHELL**
e, se non è definita, quella presente nel file /etc/passwd. Invece di
invocare una nuova shell ogni volta che si esegue un comando, il comando
viene passato a quella shell come se lo si fosse battuto personalmente.
Questo permette di cambiare le variabili d'ambiente, usare funzioni della
shell e definire alias che restano validi fino all'uscita dal programma.

**bash**
: comandi di avvio in ~/.local/share/mc6/bashrc (altrimenti ~/.bashrc) e
mappature speciali della tastiera in ~/.local/share/mc6/inputrc (altrimenti
~/.inputrc).

**ash/dash**
: (BusyBox o Debian) comandi di avvio in ~/.local/share/mc6/ashrc
(altrimenti ~/.profile).

**ksh/oksh**
: comandi di avvio in ~/.local/share/mc6/kshrc (altrimenti
*ENV*
o ~/.profile).

**mksh**
: (MirBSD ksh) comandi di avvio in ~/.local/share/mc6/mkshrc (altrimenti
*ENV*
o ~/.mkshrc).

**zsh**
: comandi di avvio in ~/.local/share/mc6/.zshrc (altrimenti ~/.zshrc).

**tcsh, fish**
: per ora non hanno file di avvio propri per questo programma, valgono solo
quelli della shell stessa.

Si può sospendere l'applicazione in esecuzione in ogni momento con
**C-o**
e tornare al programma. Se si è interrotto un comando, non si potrà eseguire
un altro comando esterno finché l'applicazione interrotta non è finita.

Dietro ai pannelli il terminale conserva tutto quello che la shell ha
scritto, e finché i pannelli sono via lo si può leggere, selezionare e
cancellare. I tasti freccia percorrono l'uscita e, con Maiusc, la
selezionano, entrambe le cose finché è il terminale stesso a ricevere i
tasti; i tasti che spostano soltanto la vista funzionano comunque, chiunque
stia scrivendo. Ogni tasto non elencato qui sotto va alla shell.

```
Ctrl-Ins       copia la selezione negli appunti
Ctrl-Maiusc-u  toglie la selezione
Alt-s          cerca nell'uscita ciò che si scrive dopo
Alt-Maiusc-s   mostra solo le righe che corrispondono
Ctrl-l         pulisce lo schermo, tenendo l'uscita
Ctrl-Maiusc-l  pulisce lo schermo e tutta l'uscita
               (anche Ctrl-Alt-l)
```

Alt-s e Alt-Maiusc-s prendono il modello come nei pannelli: lo si scrive
nella riga in alto dello schermo, e l'uscita lo segue mentre cresce. Le
maiuscole non contano. La ricerca va verso il basso dal cursore e seleziona
l'occorrenza più vicina; un altro Alt-s seleziona quella sotto, e oltre la
riga più recente la ricerca torna alla più vecchia. Dove scrive la shell non
è stato ancora letto nulla, e la ricerca prende l'uscita dalla sua riga più
vecchia. Il filtro mostra solo le righe che corrispondono, e i tasti freccia
le percorrono mentre il modello si sta ancora scrivendo; un altro
Alt-Maiusc-s porta il cursore alla riga sotto. L'impostazione
*search_direction*
inverte entrambi, e la ricerca va verso l'alto come in
**less**
e come faceva prima. Premuti senza nulla di scritto, entrambi i tasti riprendono il modello
precedente. Backspace toglie un carattere, e un carattere a cui non
corrisponde nulla non viene accettato. Invio termina la scrittura e lascia la
vista su quel che si è trovato, con l'occorrenza ancora selezionata; Esc la
termina e rimette la vista com'era prima della scrittura: il cursore dove si
stava leggendo, o al prompt se non si leggeva nulla, e il filtro e la
selezione che c'erano. Qualsiasi altro tasto termina la scrittura e poi fa
quello che fa.

Con i pannelli via, la maggior parte dei tasti funzione è del terminale, e la
barra dei pulsanti li nomina. Vedi, Modifica, Copia, Sposta ed Elimina del
gestore di file non sono offerti lì: lavorano sul file sotto il cursore del
pannello, e quel cursore non si vede. F8 è lasciato vuoto di proposito, così
che il gesto verso l'eliminazione non faccia qualcos'altro.
F7 crea una directory e Maiusc-F4 modifica un nuovo file, come con i pannelli
visibili: entrambi lavorano nella directory del pannello, che è quella in cui
sta la shell.

```
F2           copia la selezione negli appunti
F3           seleziona tutta l'uscita, o toglie la selezione
F4           lascia solo le righe che corrispondono alla
             selezione o alla parola sotto il cursore
F5           toglie quel filtro e lo rimette
F6           pulisce lo schermo e tutta l'uscita
```

Finché la shell attende al suo prompt, F1, F7, Maiusc-F4, F9 e F10 restano
del gestore di file, e F1 apre l'aiuto su questa sezione. Appena un comando è
in esecuzione, lo schermo e ogni tasto sono suoi, compresi questi. I cinque
qui sopra sono l'eccezione: restano del terminale mentre il comando lavora.
Un'applicazione a pieno schermo, un editor o un paginatore, prende per sé
tutti i tasti, anche questi. Tutti sono elencati nella sezione
**[mcterm]**
del file delle associazioni e lì si possono ridefinire.

Quando la shell dietro ai pannelli termina, con
**exit**
o con un Ctrl-D che legge essa stessa, M-Commander termina con lei. Nella
riga di comando Ctrl-D cancella un carattere, come con i pannelli visibili;
un comando che legge il suo input, come
**cat**,
lo riceve come fine dell'input. Con mksh e tcsh, che non hanno una riga di
comando di M-Commander, Ctrl-D su una riga vuota termina la shell, e
M-Commander con lei.

Se al prompt della shell, dietro ai pannelli nascosti, si scrive
**mcommander**
senza argomenti, il programma già in esecuzione rimostra i suoi pannelli
invece di avviare una seconda copia. Con un argomento, per esempio il nome di
una directory, parte un programma annidato, come prima.

Il prompt di base mostrato dal programma è della forma
"utente@host:percorso$ ". Con una shell capace, come Bash, il prompt sarà lo
stesso che si usa nella shell.

(Problema noto con fish: il prompt si vede solo in modalità a pieno schermo
(Ctrl-o), non con i pannelli visibili.)

Per usare una shell diversa da quella della variabile SHELL o da quella
indicata in /etc/passwd, si può avviare il programma così:
**SHELL=/bin/miashell mcommander**

La sezione
[OPZIONI](#options)
contiene altre informazioni su come controllare la shell.

# Permessi <a id="chmod"></a>

La finestra Permessi serve a cambiare i bit di attributo in gruppi di
file o directory. La si può invocare con la combinazione di tasti C-x c.

La finestra dei Permessi ha due parti -
*Permessi*
e
*File.*

La sezione File mostra il nome del file o della directory ed i suoi
permessi in forma ottale, oltre che il proprietario ed il gruppo.

Nella sezione Permessi c'è un set di caselle che corrispondono
agli attributi dei file. Come si cambia il bit di attributo,
si può vedere il valore in ottale aggiornato nella sezione File.

Per muoversi attraverso le sezioni (bottoni e caselle) usare i
*tasti freccia*
oppure
*Tab.*
Per cambiare lo stato delle caselle o per selezionare un bottone
usare lo
*Spazio.*
Si può usare anche i tasti di scelta rapida sui bottoni per attivarli
velocemente. I tasti di scelta rapida corrispondono alle lettere evidenziate
dei bottoni.

Per impostare i bit degli attributi, usare il tasto Invio.

Quando si lavora con un gruppo di file o directory, basta cliccare sui
bit che si vogliono impostare o cancellare. Una volta selezionati i bit
da cambiare, selezionare una delle azioni (Imposta marcati o Cancella
marcati).

Infine, per impostare gli attributi esattamente come specificato, usare
il tasto
**[Imposta tutti]**,
che agisce su tutti i file marcati.

**[Modifica tutti]**
modifica solo gli attributi marcati su tutti i file.

**[Imposta marcati]**
pone a uno i bit marcati degli attributi di tutti i file selezionati.

**[Cancella marcati]**
pone a zero i bit marcati degli attributi di tutti i file selezionati.

**[Imposta]**
imposta gli attributi di un file.

**[Cancella]**
cancella il comando Permessi.

# Proprietario <a id="chown"></a>

Il comando proprietario serve a cambiare il proprietario/gruppo di un
file. Il tasto di scelta rapida per questo comando è C-x o.

# Proprietario avanzato <a id="advanced-chown"></a>

Il comando Proprietario avanzato consiste nel comando
[permessi](#chmod)
e
[proprietario](#chown)
combinati assieme in una finestra. E' così possibile cambiare i permessi
ed il proprietario/gruppo dei file in un sol colpo.

# Operazioni sui file <a id="file-operations"></a>

Quando si copia, sposta o cancella dei file il M-Commander
mostra la finestra di operazioni sui file. Essa mostra i file sui cui
si sta operando attualmente e ci possono essere fino a tre barre di
progressione. La barra file mostra quanta parte del file corrente è
stata copiata. La barra conteggio mostra quanti dei file selezionati
sono stati gestiti. La barra byte comunica quanto dell'ampiezza totale
dei file selezionati è stata elaborato. Se l'opzione operazioni prolisse
è deselezionata, non verranno mostrate la barra file e la barra byte.

Ci sono due bottoni sul fondo della finestra di dialogo. Premendo
il tasto Salta si salterà il resto del file. Premendo il tasto
Esci si bloccherà tutta l'operazione ed il resto dei file saranno
ignorati.

Ci sono tre altre finestre di dialogo che si possono incontrare
durante le operazioni sui file.

La finestra di dialogo di errore informa circa le condizioni di
errore ed ha tre scelte. Normalmente si seleziona il tasto
Salta per saltare il file o Esci per bloccare l'operazione. E'
possibile anche selezionare il tasto Riprova se nel frattempo si ha
risolto il problema da un'altro terminale.

### Sovrascrittura del file <a id="replace"></a>

Questa finestra viene mostrata quando si tenta di copiare o spostare un file
sopra un file esistente. La finestra mostra la data e la dimensione di
entrambi i file e offre questi pulsanti:

**[Sì]**
: sovrascrive il file.

**[No]**
: salta il file.

**[Aggiungi]**
: aggiunge il file sorgente in fondo a quello di destinazione.

**[Continua]**
: aggiunge a quello di destinazione ciò che resta del file sorgente. Questo
pulsante compare solo se la dimensione del file di destinazione non è zero ed
è minore di quella del sorgente.

**[Tutti]**
: sovrascrive tutti i file.

**[Aggiorna]**
: sovrascrive se il file sorgente è più recente di quello di destinazione.

**[Nessuno]**
: non sovrascrive alcun file.

**[Più piccoli]**
: sovrascrive se la dimensione del sorgente è minore di quella della
destinazione.

**[Dimensione diversa]**
: sovrascrive i file di dimensione diversa.

**[Esci]**
: interrompe l'intera operazione.

Se la casella
**Non sovrascrivere con file di lunghezza zero**
è attiva, un file sorgente di dimensione zero non sovrascrive un file di
destinazione che non lo è.

La finestra della cancellazione ricorsiva appare quando si tenta di
cancellare una directory che non è vuota. I suoi pulsanti sono:

**[Sì]**
: cancella la directory con tutto il suo contenuto.

**[No]**
: salta la directory.

**[Tutti]**
: cancella tutte le directory.

**[Nessuno]**
: salta tutte le directory non vuote.

**[Esci]**
: interrompe l'intera operazione.

Se ci sono file marcati e si esegue un'operazione su di essi, solo i file su
cui l'operazione è riuscita perdono la marcatura. I file saltati e quelli in
cui l'operazione è fallita restano marcati.

# Maschera Copia/Rinomina <a id="mask-copyrename"></a>

L'operazione di copia/rinomina permette di cambiare il nomi dei file in
maniera semplice. Per farlo, è necessario specificare la maschera di
sorgente corretta e generalmente, nella parte finale della destinazione,
specificare alcuni caratteri jolly.
Tutti i file corrispondenti alla maschera sorgente sono
copiati/rinominati secondo la maschera destinazione. Se ci sono file
marcati, vengono rinominati solo i file marcati che corrispondono alla
maschera sorgente.

Queste le opzioni che possono essere impostate:

Segue i collegamenti, specifica se creare i collegamenti simbolici o no
(hard link), presenti nella directory sorgente (e ricorsivamente nelle
sue sotto directory) come nuovi collegamenti oppure se invece si
desidera che venga copiato il loro contenuto.

In una sottodir se esiste già, specifica cosa fare se nella
directory obiettivo esiste una directory con lo stesso nome del
file/directory in copia. L'azione predefinita è di copiare
il suo contenuto in quella directory, ma selezionando quest'opzione
si può copiare la directory sorgente in questa directory.
Forse un esempio aiuterà:

Si vuole copiare il contenuto di una directory pallo su /pinco/pallo,
che è una directory che esiste già. Normalmente (quando l'opzione
non è impostata), mcommander farebbe la copia in /pinco/pallo. Abilitando
quest'opzione verrà eseguita la copia in /pinco/pallo/pallo, perché
la directory esiste già.

Mantiene gli attributi, specifica se si vuole preservare i permessi
originali del file, le date e se si è l'utente root, gli attributi
UID e GID. Se quest'opzione non è impostata verrà rispettato il valore
corrente di umask.

**Usa i modelli della shell, opzione abilitata**

Quando l'opzione dei modelli della shell è abilitata è possibile usare
i caratteri jolly '\*' e '?' nella maschera sorgente. Questi lavorano
come nella shell. Nella maschera obbiettivo sono permessi solo i caratteri
jolly '\*' e '\\\<cifra>'. Il primo carattere jolly '\*' nella maschera
obbiettivo corrisponde al primo gruppo di caratteri jolly nella maschera
sorgente, il secondo '\*' al secondo gruppo e così via. Il carattere
jolly '\\1' corrisponde al primo gruppo di caratteri jolly nella maschera
sorgente, '\\2' corrisponde al secondo gruppo e così via fino al '\\9'.
Il carattere jolly '\\0' rappresenta tutto il nome del file sorgente.

Due esempi:

Se la maschera sorgente è "\*.tar.gz", la destinazione è "/bla/\*.tgz" e
il file da copiare è "foo.tar.gz", la copia sarà "foo.tgz" in "/bla".

Supponiamo si voglia scambiare la base e l'estensione di un file cosicché
"file.c" divenga "c.file" e così via. La maschera sorgente per questa
operazione sarà "\*.\*" e la destinazione sarà "\\2.\\1".

**Usa i modelli della shell, opzione disabilitata**

Quando l'opzione dei modelli della shell è disabilitata, M-Commander non
esegue più il raggruppamento automatico. E' necessario usare
espressioni tipo '\\(...\\)' nella maschera sorgente per dare significato
ai caratteri jolly nella maschera obbiettivo. Altrimenti le maschere
obbiettivo si trovano nella situazione di quando i modelli della shell
sono abilitati.

Due esempi:

Se la maschera sorgente è "^\\(.\*\\)\\.tar\\.gz$", la destinazione è
"/bla/\*.tgz" e il file da copiare è "foo.tar.gz", la copia sarà
"/bla/foo.tgz".

Supponiamo che si voglia scambiare la base e l'estensione di un file
cosicché "file.c" divenga "c.file" e così via. La maschera sorgente per
questa  operazione sarà "^\\(.\*\\)\\.\\(.\*\\)$" e la destinazione sarà
"\\2.\\1".

**Conversioni Maiuscole/Minuscole**

E' anche possibile cambiare tra maiuscole e minuscole i caratteri dei file.
Se si usa '\\u' o '\\l' nella maschera obbiettivo, il carattere successivo
sarà convertito rispettivamente in maiuscolo o minuscolo.

Se si usa '\\U' o '\\L' nella maschera obbiettivo, il caratteri successivi
saranno convertiti rispettivamente in maiuscolo o minuscolo fino alla
prossima corrispondenza di '\\E' o '\\U', '\\L' o alla fine del nome del file.

Notare che '\\u' e '\\l' sono più forti di '\\U' e '\\L'.

Per esempio, se la maschera sorgente è '\*' (modelli della shell abilitati)
o '^\\(.\*\\)$' (modelli della shell disabilitati) e la maschera obbiettivo
è '\\L\\u\*' i nomi dei file saranno convertiti ad avere maiuscola iniziale
ed il resto minuscolo.

Si può usare '\\' come carattere di protezione. Per esempio, '\\\\' è
una barra retroversa e '\\\*' è un asterisco.

# Completamento <a id="completion"></a>

Ovvero lascia che il M-Commander scriva per te.

Tentativi per eseguire un completamento del testo prima della posizione
corrente. M-Commander tenta il completamento trattando il testo come una variabile
(se il testo comincia con
**$**),
nomeutente (se il testo comincia con
**~**),
nomehost (se il testo comincia con
**@**)
o comando (se si è sulla riga di comando nell'atto di battere un comando,
allora possibili completamenti includerebbero parole riservate e comandi
integrati della shell). Se nessuno di questi produce una corrispondenza,
viene tentato un completamento del nome del file.

Completamenti di nomefile, nomeutente, variabili e nomehost funzionano
su tutte le righe di ingresso, il completamento dei comandi invece è
specifico della riga di comando.
Se il completamento è ambiguo (ci sono più possibilità differenti),
M-Commander emette un suono e l'azione seguente dipenderà a seconda delle impostazioni
dell'opzione
*completamento: visualizza tutto*
nella finestra
[configurazione](#configuration).
Se è abilitata, un elenco di tutte le possibilità viene mostrato vicino
alla posizione corrente per poter selezionare con i tasti freccia e
**Invio**
la voce corretta. Si può anche battere le prime lettere nelle quali le varie
possibilità differiscono per muoversi in un sottoinsieme di tutte le
possibilità e completare il più possibile. Se si preme nuovamente
**M-Tab**,
verrà mostrato solo il sottoinsieme nella finestra dell'elenco, altrimenti
la prima voce che corrisponde a tutti i caratteri precedenti verrà evidenziata.
Non appena non c'è più ambiguità, la finestra scompare, ma la si può nascondere
con i tasti di cancellamento
**Esc**,
**F10**
e i tasti di freccia sinistra e destra. Se
[completamento: visualizza tutto](#configuration)
è disabilitato, la finestra viene mostrata solo se si preme
**M-Tab**
una seconda volta; la prima volta M-Commander emette solo un suono.

# File system virtuale <a id="virtual-file-system"></a>

M-Commander dispone di uno strato di codice per accedere al file system; questo
strato si chiama commutatore dei file system virtuali e permette al programma
di lavorare su file che non si trovano nel file system Unix.

Oltre a
*local,*
che è il normale file system Unix, nel programma sono incorporati due file
system virtuali:
*extfs,*
che con uno script proprio mostra un file o un elenco del sistema come un
albero di directory, e
*sfs,*
che fa passare un singolo file attraverso un comando e mostra ciò che ne esce.
Tutto ciò che richiede una connessione a un'altra macchina, e anche gli
archivi, ora sono
[componenti aggiuntivi dei pannelli](#panel-plugins),
non file system del commutatore.

Il commutatore interpreta tutti i nomi di percorso usati e li passa al file
system che compete; la forma del nome di ciascuno è descritta nella sua
sezione.

## Componenti aggiuntivi dei pannelli <a id="panel-plugins"></a>

Un pannello non è legato a un file system: un componente aggiuntivo può
riempirlo con tutto ciò che sa elencare. Con il programma arrivano

```
arcmc        archivi e il loro contenuto
ftp, sftp    file su un'altra macchina
shell-link   file su un'altra macchina tramite ssh
samba        condivisioni di un server SMB
s3           bucket di uno storage S3
git          lo stato di un repository
docker       contenitori, immagini e i loro registri
k8s          gli oggetti di un cluster
mongo        le collezioni di un database
sqlite       le tabelle di un database
systemd      le unità del sistema
panelize     il risultato di un comando come pannello
mcpeek       uno sguardo dentro un file
mcstruct     un file binario come albero di campi con nome
skineditor   l'aspetto del programma
```

Ogni componente porta la propria guida, che
**F1**
apre dentro il suo pannello o la sua finestra. La voce
**Gestione dei componenti**
del menu Opzioni elenca ciò che è caricato, disattiva un componente e ne apre
le impostazioni. Il pannello di un componente si apre dai
[menu sinistra e destra](#left-and-right-menus),
dalle directory favorite o scrivendo l'indirizzo del componente sulla riga di
comando.

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

## File system di un solo file <a id="single-file-filesystem"></a>

**sfs**
fa passare un file attraverso un comando e mostra il risultato come un file a
sé, ed è così che si legge un file compresso senza scompattarlo a mano. Il
nome del file system si aggiunge a quello del file, come in extfs:

```
  cd documenti.gz/ugz://
```

I comandi sono elencati in
**{{sysconfdir}}/mcommander/sfs.ini**,
uno per riga: il nome del file system, una barra, il numero del comando, un
tabulatore e il comando stesso, dove
*%1*
è il file su cui si trova il pannello e
*%3*
il file su cui scrivere. Il file fornito contiene le coppie che comprimono e
scompattano gz, bz2, lz, lz4, lzma, lzo, xz e zst, e qualcuna in più.

# Attributi dei file <a id="chattr"></a>

Questa finestra serve a cambiare gli attributi di un gruppo di file e
directory su un filesystem Linux. Si apre con C-x e.

Non tutti i filesystem conoscono tutti gli attributi. L'elenco degli
attributi disponibili è mostrato come un insieme di caselle che corrispondono
agli indicatori di attributo (vedere
**chattr(1)**
per i dettagli). Man mano che le caselle cambiano, cambia con esse il valore
simbolico sotto il nome del file.

Per muoversi tra gli elementi della finestra si usano i
*tasti freccia*
o il tasto
*Tab*.
Per cambiare una casella o scegliere un pulsante si usa lo
**spazio**.

Per applicare gli attributi si preme Invio.

Lavorando su un gruppo di file o directory basta marcare gli attributi che si
vogliono attivare o togliere e scegliere poi uno dei pulsanti di azione
(Imposta i marcati o Togli i marcati).

**[Imposta tutti]**
: imposta esattamente gli attributi indicati su tutti i file selezionati.

**[Marca tutti]**
: imposta solo gli attributi marcati su tutti i file selezionati.

**[Imposta i marcati]**
: attiva gli indicatori marcati negli attributi dei file selezionati.

**[Togli i marcati]**
: disattiva gli indicatori marcati negli attributi dei file selezionati.

**[Imposta]**
: imposta gli attributi di un solo file.

**[Annulla]**
: esce dal comando.

# Selettore di schermi <a id="screen-selector"></a>

Il programma può tenere in funzione più parti interne insieme (l'editor, il
visualizzatore, il confronto) e passare dall'una all'altra senza chiudere i
file aperti. Usare più gestori di file alla volta per ora non è possibile.

Chiamiamo schermo ognuna di queste parti. Ci sono tre modi per passare da uno
schermo all'altro, con queste scorciatoie globali:

**Alt-}**
: passa allo schermo successivo;

**Alt-{**
: passa allo schermo precedente;

**Alt-\`**
: apre una finestra con l'elenco degli schermi aperti (oppure la voce di menu
"Elenco schermi").

# Selezione dei file <a id="selectunselect-files"></a>

I tasti
**+**
e
`\`
chiedono un modello e selezionano o deselezionano i file a cui corrisponde;
**\***
inverte la selezione. La finestra ricorda l'ultimo modello e permette di dire
se sia un modello di shell, se contino maiuscole e minuscole e se valga anche
per le directory.

# Modalità dei pannelli <a id="panel-modes"></a>

Una modalità di pannello è un formato di elenco con un nome, che si può
riusare. L'elenco delle modalità è comune ai due pannelli.

**Alt-t**
(e la voce
**Modalità pannello...**
dei menu sinistra e destra) apre il
**selettore:**
l'elenco delle modalità definite. Invio applica al pannello la modalità su cui
è il cursore, Esc lo lascia com'era.

La voce
**Modalità dei pannelli file...**
del menu
**Opzioni**
apre il
**gestore:**
lo stesso elenco, modificato con i tasti.
**Ins**
crea una modalità,
**F4**
(o
**Invio**)
modifica quella selezionata,
**F5**
la duplica e
**Canc**
(o
**F8**)
la elimina. Il tasto
**Predefiniti**
sostituisce l'elenco con le modalità incorporate,
**Ok**
lo salva e
**Annulla**
(o
**Esc**)
scarta tutto ciò che è stato fatto nella finestra.

Il gestore modifica l'elenco comune delle modalità; non cambia la modalità di
alcun pannello.

L'editor delle modalità ha campi distinti per i tipi di campo delle colonne e
le loro larghezze, e per la riga di mini-stato, con i nomi di campo descritti
in
[Modalità lista...](#listing-format)
\. L'elenco dei tipi è separato da virgole, una voce per colonna; una colonna
può contenere più campi separati da spazi (per esempio
**type name**).
Una larghezza di 0 (o vuota) lascia al campo la sua larghezza automatica.
Nel campo dei tipi si può anche incollare una stringa di formato completa
(per esempio
**half name | size:7**):
i separatori
**|**
e i suffissi
**:larghezza**
vengono allora distribuiti tra i due elenchi.

Le modalità definite e quella scelta da ciascun pannello si conservano tra una
sessione e l'altra.

# Riferimento rapido delle espressioni regolari <a id="regex-quick-reference"></a>

**Elementi comuni**

```
Un carattere tra: a, b o c              [abc]
Un carattere che non sia a, b o c       [^abc]
Un carattere nell'intervallo a-z        [a-z]
Un carattere fuori dall'intervallo a-z  [^a-z]
Un carattere tra a-z o A-Z              [a-zA-Z]
Un carattere qualsiasi                  .
Alternativa: a oppure b                 a|b
Un carattere di spazio                  \s
Tutto ciò che non è spazio              \S
Una cifra                               \d
Tutto ciò che non è cifra               \D
Un carattere di parola                  \w
Tutto ciò che non è carattere di parola \W
Gruppo senza cattura                    (?:...)
Gruppo con cattura                      (...)
Zero o una a                            a?
Zero o più a                            a*
Una o più a                             a+
Esattamente 3 a                         a{3}
3 a o più                               a{3,}
Tra 3 e 6 a                             a{3,6}
Inizio della stringa                    ^
Fine della stringa                      $
Confine di parola                       \b
Fuori da un confine di parola           \B
```

**Ancore**

```
Inizio dell'occorrenza                  \G
Inizio della stringa                    ^
Fine della stringa                      $
Inizio della stringa                    \A
Fine della stringa                      \Z
Fine assoluta della stringa             \z
Confine di parola                       \b
Fuori da un confine di parola           \B
```

**Elementi generali**

```
A capo                                  \n
Ritorno carrello                        \r
Tabulazione                             \t
Carattere nullo                         \0
```

**Metasequenze**

```
Un carattere qualsiasi                  .
Alternativa: a oppure b                 a|b
Un carattere di spazio                  \s
Tutto ciò che non è spazio              \S
Una cifra                               \d
Tutto ciò che non è cifra               \D
Un carattere di parola                  \w
Tutto ciò che non è carattere di parola \W
Sequenza Unicode, a capo compresi       \X
A capo Unicode                          \R
Tutto tranne un a capo                  \N
Spazio verticale                        \v
Negazione di \v                         \V
Spazio orizzontale                      \h
Negazione di \h                         \H
Azzera l'occorrenza                     \K
Sottomodello numero #                   \#
Proprietà Unicode X                     \pX
Proprietà Unicode o categoria           \p{...}
Negazione di \pX                        \PX
Negazione di \p{...}                    \P{...}
Citazione: prendi alla lettera          \Q...\E
Sottomodello 'nome'                     \k{name}
Sottomodello 'nome'                     \k<name>
Sottomodello 'nome'                     \k'name'
Sottomodello n                          \gn
Sottomodello n                          \g{n}
n-esimo sottomodello relativo prec.     \g{-n}
Espressione del gruppo n                \g<n>
Espressione del gruppo n successivo     \g<+n>
Espressione del gruppo n                \g'n'
Espressione del sottomodello n succ.    \g'+n'
Gruppo di cattura con nome              \g{letter}
Espressione del gruppo con nome         \g<letter>
Espressione del gruppo con nome         \g'letter'
Carattere esadecimale YY                \xYY
Carattere esadecimale YYYY              \x{YYYY}
Carattere ottale ddd                    \ddd
Carattere di controllo Y                \cY
Carattere backspace                     [\b]
Rende letterale ogni carattere          \
```

**Quantificatori**

```
Zero o una a                            a?
Zero o più a                            a*
Una o più a                             a+
Esattamente 3 a                         a{3}
3 a o più                               a{3,}
Tra 3 e 6 a                             a{3,6}
Quantificatore avido                    a*
Quantificatore pigro                    a*?
Quantificatore possessivo               a*+
```

**Classi di caratteri**

```
Un carattere tra: a, b o c              [abc]
Un carattere che non sia a, b o c       [^abc]
Un carattere nell'intervallo a-z        [a-z]
Un carattere fuori dall'intervallo a-z  [^a-z]
Un carattere tra a-z o A-Z              [a-zA-Z]
Lettere e cifre                         [[:alnum:]]
Lettere                                 [[:alpha:]]
Codici ASCII 0-127                      [[:ascii:]]
Solo spazio o tabulazione               [[:blank:]]
Caratteri di controllo                  [[:cntrl:]]
Cifre decimali                          [[:digit:]]
Caratteri visibili (senza spazio)       [[:graph:]]
Lettere minuscole                       [[:lower:]]
Caratteri visibili                      [[:print:]]
Segni di punteggiatura visibili         [[:punct:]]
Spazi                                   [[:space:]]
Lettere maiuscole                       [[:upper:]]
Caratteri di parola                     [[:word:]]
Cifre esadecimali                       [[:xdigit:]]
Inizio di parola                        [[:<:]]
Fine di parola                          [[:>:]]
```

**Indicatori e modificatori**

```
Multiriga                               m
Senza distinguere le maiuscole          i
Ignora gli spazi / prolisso             x
Riga singola                            s
Unicode                                 u
eXtra                                   X
Non avido                               U
Ancorato                                A
Nomi di gruppo ripetuti                 J
Gruppi senza cattura                    n
Ignora tutti gli spazi / prolisso       xx
```

**Costrutti di gruppo**

```
Gruppo senza cattura                    (?:...)
Gruppo con cattura                      (...)
Gruppo atomico (senza cattura)          (?>...)
Azzera il numero del sottomodello       (?|...)
Gruppo di commento                      (?#...)
Gruppo di cattura con nome              (?'name'...)
Gruppo di cattura con nome              (?<name>...)
Gruppo di cattura con nome              (?P<name>...)
Modificatori in linea                   (?imsxUJnxx)
Modificatori in linea locali            (?imsxUJnxx:...)
Costrutto condizionale                  (?(1)yes|no)
Costrutto condizionale                  (?(R)yes|no)
Condizionale ricorsivo                  (?(R#)yes|no)
Costrutto condizionale                  (?(R&name)yes|no)
Condizionale con sguardo avanti         (?(?=...)yes|no)
Condizionale con sguardo indietro       (?(?<=...)yes|no)
Ricorsione dell'intero modello          (?R)
Espressione del gruppo 1                (?1)
Primo gruppo di cattura relativo        (?+1)
Espressione del gruppo con nome         (?&name)
Sottomodello 'nome'                     (?P=name)
Espressione del gruppo '{nome}'         (?P>name)
Definisci i modelli prima dell'uso      (?(DEFINE)...)
Sguardo avanti positivo                 (?=...)
Sguardo avanti negativo                 (?!...)
Sguardo indietro positivo               (?<=...)
Sguardo indietro negativo               (?<!...)
Asserzioni di sguardo alfabetiche       (*pla:...)
Asserzione di sguardo non atomica       (*non_atomic_positive_lookahead:...)
Asserzione di scrittura uniforme        (*script_run:...)
Scrittura uniforme (abbreviato)         (*sr:...)
Verbo di controllo                      (*ACCEPT)
Verbo di controllo                      (*FAIL)
Verbo di controllo                      (*MARK:NAME)
Verbo di controllo                      (*COMMIT)
Verbo di controllo                      (*PRUNE)
Verbo di controllo                      (*SKIP)
Verbo di controllo                      (*THEN)
```

# Colori <a id="colors"></a>

Il M-Commander tenta di stabilire se il terminale corrente
supporta i colori usando il database dei terminali e il nome del
terminale corrente. Capita che possa sbagliarsi, perciò si può essere
costretti a forzare la modalità a colori o a disabilitarla usando
rispettivamente le opzioni -c e -b.

Se il programma è compilato con il manager dello schermo S-Lang invece
che ncurses, controllerà se è impostata anche la variabile
**COLORTERM,**
con lo stesso effetto dell'opzione -c.

E' possibile specificare i terminali su cui si vuole forzare sempre
la modalità colore aggiungendo la variabile
*color_terminals*
nella sezione Colors del file di inizializzazione.
Questo previene il M-Commander dal tentare di controllare se
il terminale supporta i colori. Per esempio:

```
[Colors]
color_terminals=linux,xterm
```

```
color_terminals=nome_terminale-1,nome-terminale-2...
```

Il programma può essere compilato sia con il supporto di ncurses che
di S-Lang ma ncurses non fornisce alcun modo per forzare la modalità
colore: ncurses userà solo le informazioni nel database dei terminali.

# Skin <a id="skins"></a>

L'aspetto del programma si può cambiare. Per farlo occorre indicare un file
che contenga la descrizione dei colori e delle linee con cui disegnare i
riquadri. La ridefinizione dei colori è del tutto compatibile con
l'assegnazione descritta nella sezione
[Colori](#colors).

Se lo skin contiene definizioni a colore pieno (true-color), nella sezione
[skin] va messa la chiave 'truecolors' con valore TRUE. Se non si usa il
colore pieno ma i 256 colori, va messa invece '256colors'.

Il file dello skin viene cercato con questo ordine (fino al primo trovato):

```
1) opzione a riga di comando -S <skin>, --skin=<skin>
2) variabile d'ambiente MC_SKIN
3) parametro skin della sezione [Midnight-Commander]
4) file {{sysconfdir}}/mcommander/skins/default.ini
5) file {{pkgdatadir}}/skins/default.ini
```

L'opzione a riga di comando, la variabile d'ambiente e il parametro nel file
di configurazione possono contenere il percorso assoluto del file dello skin
(con l'estensione .ini o senza). La ricerca avviene in (fino al primo
trovato):

```
1) ~/.local/share/mc6/skins/
2) {{sysconfdir}}/mcommander/skins/
3) {{pkgdatadir}}/skins/
```

Il formato dei file degli skin è descritto in
**{{pkgdatadir}}/skins/README.txt**.

# Evidenziazione dei nomi <a id="filenames-highlight"></a>

La sezione [filehighlight] del file dello skin corrente contiene come chiavi
i nomi dei gruppi di evidenziazione e come valori le coppie di colori.

Le regole di evidenziazione dei nomi stanno nel file
{{pkgdatadir}}/filehighlight.ini (~/.config/mc6/filehighlight.ini). Il nome
delle sezioni di questo file deve essere uguale ai nomi dei parametri della
sezione [filehighlight] (nel file dello skin corrente).

Le chiavi di questi gruppi sono:

*type*
: tipo di file. Se presente, tutte le altre opzioni vengono ignorate.

*regexp*
: espressione regolare. Se presente, l'opzione 'extensions' viene ignorata.

*extensions*
: elenco delle estensioni dei file, separate dal segno ';'.

*extensions_case*
: (ha senso solo con il parametro 'extensions') rende la regola 'extensions'
sensibile alle maiuscole (true) o no (false).

La chiave 'type' può avere questi valori:

```
- FILE (tutti i file)
  - FILE_EXE
- DIR (tutte le directory)
  - LINK_DIR
- LINK (tutti i collegamenti tranne quelli rotti)
  - HARDLINK
  - SYMLINK
- STALE_LINK
- DEVICE (tutti i file di dispositivo)
  - DEVICE_BLOCK
  - DEVICE_CHAR
- SPECIAL (tutti i file speciali)
  - SPECIAL_SOCKET
  - SPECIAL_FIFO
  - SPECIAL_DOOR
```

# Parametri per editor o visualizzatore esterni <a id="parameters-for-external-editor-or-viewer"></a>

Il programma permette di indicare opzioni per gli editor e i visualizzatori
esterni. La sezione "[External editor or viewer parameters]" viene cercata
prima nel file di inizializzazione di sistema (il file defaults.ini nella
directory del programma) e poi nel file ~/.config/mc6/ini. Il nome
dell'opzione deve essere il nome (percorso completo) dell'editor o del
visualizzatore esterno. Il valore può contenere queste variabili:

*%filename*
: il nome del file da modificare o vedere.

*%lineno*
: la riga su cui il file viene aperto.

Per esempio:

```
[External editor or viewer parameters]
    vi=%filename +%lineno
    joe=%filename +%lineno
    more=%filename +%lineno
```

La riga iniziale viene passata all'editor o al visualizzatore esterno solo se
questo parte dalla finestra dei risultati di
[Trova file](#find-file).

Se l'editor o il visualizzatore esterno viene avviato con F4 o F3, il
programma conta sul fatto che quel programma (almeno "joe", ma probabilmente
anche altri) apra da sé il file dov'era rimasto l'ultima volta. Il programma
non impedisce all'editor o al visualizzatore esterno di salvare e ripristinare
la posizione nei file aperti.

# Impostazioni speciali <a id="special-settings"></a>

Quasi tutte le impostazioni si possono cambiare dai menu. Ce n'è però un
piccolo numero che si cambia solo agendo sul file di configurazione.

Queste sono le variabili che si possono impostare nel file
~/.config/mc6/ini:

*clear_before_exec*
: Come impostazione predefinita il programma pulisce lo schermo prima di
eseguire un comando. Se si preferisce vedere il risultato di un comando in
fondo allo schermo, si modifichi il file ~/.config/mc6/ini e si cambi il
valore del campo clear_before_exec a 0.

*confirm_view_dir*
: Se si preme F3 su una directory, normalmente il programma entra nella
directory. Se questo valore è 1, chiede una conferma prima di cambiare
directory quando ci sono file marcati.

*vfs_timeout*
: Il tempo di vita della cache di un filesystem virtuale, in secondi. Uscendo
da un archivio o da un file compresso, l'elenco letto e il file temporaneo
scompattato restano per quel tempo, così rientrare è immediato, e poi vengono
rilasciati. Predefinito 60; 0 li rilascia subito.

*only_leading_plus_minus*
: Permette una gestione speciale per '+', '-' e '\*' nella riga di comando
(selezione, deselezione, selezione inversa) solo se la riga di comando è
vuota. Non è necessario proteggere questi caratteri in mezzo alla riga di
comando, ma non si può cambiare la selezione se la riga di comando non è
vuota.

*alternate_plus_minus*
: Se attiva, i tasti '+', '-', '\\' e '\*' funzionano normalmente. Per
selezionare e deselezionare si usano allora 'Alt-+', 'Alt--' e 'Alt-\*'.

*show_output_starts_shell*
: Quando si usa C-o per tornare allo schermo utente, se questa opzione è
attiva si ottiene una nuova shell. Altrimenti, premendo un tasto qualsiasi si
torna al programma.

*timeformat_recent*
: La forma di data e ora usata per le date di meno di sei mesi fa. Per la
descrizione del formato vedere la pagina di manuale di strftime o di date. Se
questa opzione manca, vale il formato predefinito.

*timeformat_old*
: La forma di data e ora usata per le date di più di sei mesi fa o nel
futuro. Per la descrizione del formato vedere la pagina di manuale di
strftime o di date. Se questa opzione manca, vale il formato predefinito.

*use_file_to_guess_type*
: Se attiva (predefinito), il programma usa il comando file per riconoscere i
tipi elencati nel
[file extensions.ini](#edit-extension-file).

*xtree_mode*
: Se attiva (predefinito: no), navigando il filesystem in un pannello ad
albero l'altro pannello mostra da sé il contenuto della directory scelta.

*shell_directory_timeout*
: Il tempo di vita di una voce della cache delle directory, in secondi. Il
valore predefinito è 900 secondi.

*clipboard_store*
: Il percorso (con le opzioni) di un programma esterno per gli appunti, come
'xclip', che legge testo da un file nella selezione di X. Per esempio:

<!-- -->

```
clipboard_store=xclip -i
```

*clipboard_paste*
: Il percorso (con le opzioni) di un programma esterno per gli appunti, come
'xclip', che scrive la selezione sullo standard output. Per esempio:

<!-- -->

```
clipboard_paste=xclip -o
```

*autodetect_codeset*
: Con questa opzione il programma usa il comando 'enca' per riconoscere da sé
il set di caratteri dei file di testo nel visualizzatore e nell'editor
interni. L'elenco dei valori validi si ottiene con
'enca --list languages | cut -d : -f1'. L'opzione deve stare nella sezione
[Misc].

Per esempio:

```
autodetect_codeset=russian
```

Le impostazioni del visualizzatore di file interno stanno nella sezione
[Viewer] dello stesso file. Si trovano tutte anche nella finestra
[Opzioni del visualizzatore](mview.md#viewer-options);
i nomi qui sono quelli che quella finestra scrive.

*wrap*
: Manda a capo nella riga di schermo successiva ciò che non entra nella
larghezza. Attiva come impostazione predefinita.

*syntax*
: Colora il testo con le regole di sintassi dell'editor. Disattiva come
impostazione predefinita.

*mouse_move_pages*
: Lo scorrimento con il mouse avviene a pagine invece che riga per riga. In
modalità ASCII il tasto sinistro seleziona il testo, quindi lì questo
scorrimento si fa con il tasto destro o centrale. Attiva come impostazione
predefinita.

*remember_file_position*
: Apre il file nel punto in cui era stato lasciato l'ultima volta. Disattiva
come impostazione predefinita.

*structured_auto*
: Apre subito in modalità strutturata (albero) i file supportati (json, yaml,
yml, xml, html, htm). Se un file non si può analizzare, viene usata senza
avvisi la vista di testo. Disattiva come impostazione predefinita.

*eof*
: Il testo scritto dopo l'ultima riga del file. Vuoto come impostazione
predefinita.

*structured_max_size*
: Il file più grande che la vista strutturata analizza, in byte. Uno più
grande viene rifiutato prima di essere letto. Predefinito 67108864 (64 MB).

*structured_max_nodes*
: L'albero più grande che la vista strutturata costruisce, contato in nodi.
Un documento denso, come un XML di piccoli tag, arriva a questo limite prima
che a quello della dimensione: consuma circa un nodo ogni dodici byte, e ogni
nodo costa memoria. Predefinito 10000000, che contiene circa 120 MB di un
file simile in circa 1,5 GB.

*dirt_limit*
: Quanti aggiornamenti dello schermo si possono saltare al massimo mentre un
file viene letto. Di solito questo valore non conta, perché il programma
regola da sé il numero di aggiornamenti saltati secondo il ritmo dei tasti
che arrivano. Su macchine molto lente, o su terminali con ripetizione
automatica veloce, un valore grande rende lo schermo a scatti. Il valore
predefinito è 10, che si comporta meglio.

Le versioni precedenti tenevano queste impostazioni nella sezione principale
con nomi più lunghi (wrap_mode, viewer_syntax_highlighting,
mouse_move_pages_viewer, mcview_remember_file_position,
mcview_structured_auto, mcview_eof e max_dirt_limit). Vengono lette da lì una
volta e riscritte nella sezione [Viewer].

Le impostazioni del terminale che esegue la shell dietro i pannelli sono
nella sezione [Terminal] dello stesso file. Nessuna finestra le scrive.

*search_direction*
: In quale verso
**Alt-s**
percorre l'uscita della shell, e in quale
**Alt-Maiusc-s**
passa da una riga del filtro alla successiva: "down" va dal cursore verso la
riga più recente e oltre di essa torna alla più vecchia, "up" va verso la
riga più vecchia e torna alla più recente, come cerca
**less**
e come si faceva prima. "down" in modo predefinito.

*clipboard_write*
: Se un programma nel terminale può mettere testo negli appunti con la
sequenza OSC 52, come fanno vim, tmux o una sessione ssh. Il testo va dove va
una copia dall'editor: nel file degli appunti e al comando di
*clipboard_store*.
In questo modo un programma non può mai leggere gli appunti. "false" in modo
predefinito: qualsiasi uscita nel terminale, anche un file mostrato con
**cat**,
potrebbe cambiare gli appunti.

# Database di terminali <a id="terminal-databases"></a>

Il M-Commander fornisce una maniera per correggere il database
dei terminali si sistema senza richiedere i privilegi di
amministratore (root). Il M-Commander ricerca nel file di
inizializzazione di sistema (il file defaults.ini collocato nella directory
di libreria del M-Commander) e nel file ~/.config/mc6/ini la sezione
"terminal:nome-del-terminale-in-uso" e poi la sezione "terminal:general",
ogni riga della sezione contiene il simbolo chiave che si vuol definire,
seguito da un segno di uguale e la definizione per quel tasto.
E' possibile usare la forma speciale \\e per rappresentare il carattere
di escape e ^x per rappresentare il carattere control-x.

I simboli chiave possibili sono:

```
f0 a f20      tasti funzione f0-f20
bs	      backspace
home          tasto inizio
end           tasto fine
up            tasto freccia in su
down          tasto freccia in giù
left          tasto freccia a sinistra
right         tasto freccia a destra
pgdn          tasto pagina in giù
pgup          tasto pagina in su
insert        tasto inserimento
delete        tasto cancellazione
complete      per fare il completamento
```

Per esempio, per definire il tasto di inserimento come escape + [ + O + p
impostare il seguente nel file ini:

```
insert=\e[Op
```

Il tasto di
*completamento*
rappresenta le sequenze di escape usate per invocare il processo di
completamento, invocato tramite M-tab, ma ridefinibile ad altri tasti
per fare lo stesso lavoro (su quelle tastiere con tonnellate di
simpatici tasti dappertutto).

<!-- help:break -->

# VARIABILI D'AMBIENTE <a id="environment"></a>

Le variabili elencate qui sotto sono quelle che M-Commander legge e quelle che
imposta per i programmi che avvia. Variabili come **TERM**, **SHELL**, **HOME**
o **PATH** non compaiono qui: il programma le legge per capire dove sta
girando, non per esserne configurato.

## Lette all'avvio <a id="read-at-start-up"></a>

**MC_DATADIR**
: La directory da cui vengono presi i file di dati, al posto di quella
incorporata. Vedere [FILE](#files).

**MC_PROFILE_ROOT**
: La radice dei file dell'utente, come percorso assoluto. Vedere
[FILE](#files).

**MC_SKIN**
: Lo skin da usare, per nome o per percorso. Vedere [Skin](#skins).

**MC_KEYMAP**
: Il file di associazione dei tasti da usare. Vedere [Tasti](#keys).

**MC_TMPDIR**
: La directory dei file temporanei del programma.

**MC_NO_LUA**
: Con il valore 1 il programma parte senza l'ambiente di esecuzione Lua. Non
viene caricato alcun pacchetto Lua e niente che ne abbia bisogno è
disponibile.

**MC_SIXEL**
: Con il valore 0 si dichiara che il terminale non ha la grafica sixel, con 1
che ce l'ha. Senza la variabile viene interrogato il terminale stesso.

**KEYBOARD_KEY_TIMEOUT_US**
: Quanto attendere il resto di una sequenza di escape, in microsecondi.

**COLORTERM**
: Letta quando vengono scelti i colori. Vedere [Colori](#colors).

**CDPATH**
: Le directory in cui cerca il comando cd interno.

**EDITOR**, **VIEWER**, **PAGER**
: I programmi esterni usati quando l'editor o il visualizzatore incorporati
sono disattivati. Vedere
[Parametri per editor o visualizzatore esterni](#parameters-for-external-editor-or-viewer).

## Impostate per i programmi che M-Commander avvia <a id="set-for-the-programs-m-commander-starts"></a>

Non sono pensate per essere impostate a mano. Il programma le scrive perché
una copia di sé stesso avviata dal terminale incorporato possa capire che sta
già girando dentro a uno.

**MC_SID**
: La sessione in cui gira il programma. Una copia avviata da quella sessione
non apre pannelli propri.

**MC_PID**
: L'identificatore di processo del programma in esecuzione.

**MC_TTY**
: Il terminale su cui il programma è stato avviato.

## Registri di debug <a id="debug-logs"></a>

Un registro viene scritto solo quando è attivato, e l'interruttore prende il
valore 1. Le variabili dei componenti aggiuntivi ricadono su quelle generali,
per cui impostare la sola coppia generale registra tutto.

**MC_LOG_ENABLE**, **MC_LOG_FILE**
: Il registro generale. Senza **MC_LOG_FILE** viene usato il file indicato da
*logfile* nella sezione *[Logging]* del file *ini*, e senza quella voce
*mc.log* accanto agli altri file dell'utente.

**MC_FTP_LOG_ENABLE**, **MC_FTP_LOG_FILE**
: Il registro del componente aggiuntivo di pannello ftp. Il file ricade su
*/tmp/mc-ftp.log*.

**MC_SMB_LOG_ENABLE**, **MC_SMB_LOG_FILE**
: Il registro del componente aggiuntivo di pannello samba. Il file ricade su
*/tmp/mc-samba.log*.

**MC_SPELL_LOG**
: Il file su cui scrive il correttore ortografico. Non ha un interruttore
proprio: il registro viene scritto quando la variabile nomina un file.

Per conservare il registro di una connessione ftp che non riesce:

```
MC_FTP_LOG_ENABLE=1 MC_FTP_LOG_FILE=/tmp/ftp.log mcommander
```

# FILE <a id="files"></a>

Il programma recupera tutte le informazioni relative al proprio funzionamento
dalla variabile ambiente
**MC_DATADIR**,
e se la variabile non è impostata, passerà alla directory {{pkgdatadir}} .

*{{pkgdatadir}}/help/mcommander.md*
: Il file di aiuto per il programma.

*{{pkgdatadir}}/extensions.ini*
: Il file delle estensioni di sistema predefinito.

*~/.config/mc6/extensions.ini*
: Le estensioni dell'utente, la configurazione del visualizzatore e
dell'editor di file. Se presenti, questi file si sovrappongono ai file di
sistema.

*{{pkgdatadir}}/mc.ini*
: La configurazione di sistema predefinita per il M-Commander, usata solo
se l'utente non possiede il proprio file ~/.config/mc6/ini.

*{{pkgdatadir}}/defaults.ini*
: Le impostazioni globali per il M-Commander. La modifica di questo
file influisce su tutti gli utenti, che abbiano o no il file ~/.config/mc6/ini .
Attualmente vengono caricate solo le
[impostazioni del terminale](#terminal-databases)
da defaults.ini.

*~/.config/mc6/ini*
: La configurazione dell'utente. Se questo file è presente, la configurazione
viene caricata da qui invece che dal file di sistema.

*{{pkgdatadir}}/hints/hint*
: Questo file contiene i suggerimenti (dritte) mostrate dal programma.

*~/.config/mc6/menu.ini*
: Il menu utente che si modifica da sé, un gruppo per voce. Dove questo file
esiste, è quello che F2 apre, e il .mc6menu della directory corrente viene
mostrato insieme a esso.
*~/.config/mc6/menu*
: Il menu utente per le applicazioni. Se presente viene usato al posto
del menu delle applicazioni di sistema.

*~/.cache/mc6/Tree*
: L'elenco di directory per l'albero directory e per la vista ad albero.

*./.usermenu*
: Menu locale definito dall'utente. Se questo file è presente viene usato
al posto del menu delle applicazioni utente o di sistema.

To change default home directory of M-Commander, you can use
**MC_PROFILE_ROOT**
environment variable. The value of MC_PROFILE_ROOT must be an absolute path.
If MC_PROFILE_ROOT is unset or empty, HOME variable is used. If HOME is unset
or empty, M-Commander directories are get from GLib library.

# LICENZA <!-- help:skip -->

Questo programma è distribuito sotto i termini della Licenza Generale
GNU come pubblicata dalla Free Software Foundation. Vedere l'aiuto integrato
per i dettagli sulla licenza e sulla mancanza di garanzie.

# REPERIBILITA' <a id="availability"></a>

L'ultima versione di questo programma si trova su
<https://github.com/blue-panels/mcommander/releases> .

# VEDERE ANCHE <a id="see-also"></a>

ed(1), gpm(1), terminfo(1), view(1), sh(1), bash(1),
tcsh(1), zsh(1).

```
La pagina Web di M-Commander:
	https://github.com/blue-panels/mcommander
```

# AUTORI <a id="authors"></a>

Miguel de Icaza (miguel@ximian.com), Janne Kukonlehto
(jtklehto@paju.oulu.fi), Radek Doulik (rodo@ucw.cz), Fred
Leeflang (fredl@nebula.ow.org), Dugan Porter (dugan@b011.eunet.es),
Jakub Jelinek (jj@sunsite.mff.cuni.cz), Ching Hui
(mr854307@cs.nthu.edu.tw), Andrej Borsenkow (borsenkow.msk@sni.de),
Norbert Warmuth (nwarmuth@privat.circular.de),
Mauricio Plaza (mok@roxanne.nuclecu.unam.mx), Paul Sheer
(psheer@icon.co.za), Pavel Machek (pavel@ucw.cz) e Pavel Roskin
(proski@gnu.org) sono gli sviluppatori di questo pacchetto.
Alessandro Rubini (rubini@ipvvis.unipv.it) ha dato un notevole
contribuito nella correzione e nel miglioramento del supporto del
mouse nel programma, John Davis (davis@space.mit.edu) ha reso
disponibile la sua libreria S-lang sotto la licenza GPL e ha risposto
alle mie domande su di essa; le seguenti persone hanno contribuito
al codice e in molte correzioni (in ordine alfabetico):

Adam Tla/lka (atlka@sunrise.pg.gda.pl),
alex@bcs.zp.ua (Alex I. Tkachenko), Antonio Palama,
DOS port (palama@posso.dm.unipi.it), Erwin van Eijk
(wabbit@corner.iaf.nl), Gerd Knorr (kraxel@cs.tu-berlin.de),
Jean-Daniel Luiset (luiset@cih.hcuge.ch), Jon Stevens
(root@dolphin.csudh.edu), Juan Francisco Grigera, port su piattaforma Win32
(j-grigera@usa.net), Juan Jose Ciarlante (jjciarla@raiz.uncu.edu.ar),
Ilya Rybkin (rybkin@rouge.phys.lsu.edu), Marcelo Roccasalva
(mfroccas@raiz.uncu.edu.ar), Massimo Fontanelli (MC8737@mclink.it),
Sergey Ya. Korshunoff (seyko2@gmail.com), Thomas Pundt
(pundtt@math.uni-muenster.de), Timur Bakeyev
(timur@goff.comtat.kazan.su), Tomasz Cholewo
(tjchol01@mecca.spd.louisville.edu), Torben Fjerdingstad
(torben.fjerdingstad@uni-c.dk), Vadim Sinolitis (vvs@nsrd.npi.msu.su)
e Wim Osterholt (wim@djo.wtm.tudelft.nl).

# BACHI <a id="bugs"></a>

Se si vuole fare un rapporto di un problema nel programma, si prega di
aprire una segnalazione a questo indirizzo:
<https://github.com/blue-panels/mcommander/issues> .

Nel rapporto è necessario fornire una descrizione dettagliata del baco,
la versione del programma (mcommander -v mostra quest'informazione), il sistema
operativo su cui si sta facendo girare il programma e, se il programma
va in crash, è gradita una traccia dello stack.
