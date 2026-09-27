---
date: settembre 2026
---

<!-- help:topics "Indice:" -->
# NOME <!-- help:skip -->

mcdiff6 - Visualizzatore di differenze interno.

# USO <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] file1 file2

# DESCRIZIONE

mcdiff6 è un collegamento a
**mcommander**,
il programma principale del gestore di file. Eseguito con questo nome, apre
il visualizzatore di differenze interno, che confronta
*file1*
con
*file2*,
indicati sulla riga di comando.

# Visualizzatore di differenze interno <a id="diff-viewer"></a>

mcdiff6 è uno strumento visuale di confronto. Permette di confrontare due file
e di modificarli sul posto, e la differenza viene ricalcolata dopo ogni
modifica. Lo apre anche il componente di pannello git, con il file come lo
tiene HEAD da una parte e il file della copia di lavoro dall'altra.

Nel visualizzatore di differenze interno sono disponibili i tasti seguenti:

**F1**
: Invoca il visualizzatore ipertestuale dell'aiuto.

**F2**
: Salva i file modificati.

**F4**
: Modifica il file del pannello sinistro nell'editor interno.

**F14**
: Modifica il file del pannello destro nell'editor interno.

**F5**
: Porta la differenza corrente nel file di destra. Viene unita solo la
differenza corrente, e il confronto viene rifatto.

**F15**
: Porta la differenza corrente nel verso opposto, nel file di sinistra.

**F7**
: Inizia una ricerca.

**F17**
: Continua la ricerca.

**F9**
: Apre le
[opzioni del confronto](#diff-options).

**Alt-e**
: Sceglie il set di caratteri con cui i due file vengono letti.

**F10, Esc, q, Q**
: Esce dal visualizzatore di differenze.

**Alt-s, s**
: Mostra o nasconde lo stato delle differenze.

**Alt-n, l**
: Mostra o nasconde i numeri di riga.

**Ctrl-s**
: Attiva o disattiva l'evidenziazione della sintassi. Il testo di ogni riga
viene colorato con le regole di sintassi, come nell'editor interno, e lo
stato della riga resta allo sfondo e alla colonna dei segni. Dove uno skin
distingue una parola cambiata dal resto della riga solo con il colore del
testo, la parola viene invece sottolineata. L'impostazione viene ricordata
separatamente da quella dell'editor.

**f**
: Ingrandisce al massimo il pannello sinistro.

**=**
: Rende i pannelli di uguale larghezza.

**>**
: Restringe il pannello destro.

**<**
: Restringe il pannello sinistro.

**2, 3, 4, 8**
: Imposta la dimensione della tabulazione.

**C-u**
: Scambia il contenuto dei due pannelli.

**C-r**
: Rilegge entrambi i file e ricalcola la differenza.

**C-o**
: Mostra lo schermo dei comandi.

**Invio, spazio, n**
: Va alla differenza successiva.

**Backspace, p**
: Va alla differenza precedente.

**g, G**
: Va alla riga indicata.

**Giù**
: Scorre di una riga in avanti.

**Su**
: Scorre di una riga indietro.

**PagSu**
: Indietreggia di una pagina.

**PagGiù**
: Avanza di una pagina.

**Sinistra, Destra**
: Spostano il testo di una colonna di lato.

**C-Sinistra, C-Destra**
: Spostano il testo di otto colonne di lato.

**Inizio**
: Torna alla prima colonna.

**C-Inizio**
: Va all'inizio del file.

**C-Fine**
: Va alla fine del file.

# Opzioni del confronto <a id="diff-options"></a>

Le impostazioni del
[visualizzatore di differenze](#diff-viewer),
aperte lì da
**F9**
e, nel gestore di file, dalla voce
**Opzioni del confronto**
del menu Opzioni. Il visualizzatore le prende quando parte, quindi un
confronto già sullo schermo mantiene quelle con cui è stato aperto.

*Algoritmo di confronto.*
Normale confronta i file come sono. Il più veloce presume file grandi e si
accontenta di un risultato più grossolano. Minimo impiega più tempo per
trovare un insieme più piccolo di differenze.

*Ignora maiuscole.*
Maiuscole e minuscole contano come lo stesso carattere.

*Ignora l'espansione delle tabulazioni.*
Le righe che differiscono solo per come è scritto lo stesso rientro, con
tabulazioni o con spazi, contano come uguali.

*Ignora i cambiamenti di spaziatura.*
Una sequenza di spazi vale quanto qualsiasi altra sequenza di spazi.

*Ignora tutti gli spazi.*
Gli spazi restano fuori dal confronto.

*Togli il ritorno a capo finale.*
Toglie il carattere di ritorno carrello a fine riga, così un file con fine
riga DOS si confronta con uno con fine riga Unix.

# LICENZA <!-- help:skip -->

Questo programma è distribuito secondo i termini della GNU General Public
License pubblicata dalla Free Software Foundation. Per i dettagli sulla
licenza e sull'assenza di garanzia si veda l'aiuto interno.

# DISPONIBILITÀ

L'ultima versione di questo programma si trova all'indirizzo
<https://github.com/blue-panels/mcommander/releases> .

# VEDERE ANCHE

mcommander(1), mview(1), mcedit6(1), diff(1).

# BACHI

I bachi vanno segnalati all'indirizzo
<https://github.com/blue-panels/mcommander/issues> .
