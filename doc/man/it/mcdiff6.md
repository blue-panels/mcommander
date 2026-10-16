---
date: settembre 2026
---

<!-- help:topics "Indice:" -->
# NOME <!-- help:skip -->

mcdiff6 - Visualizzatore di differenze interno.

# USO <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] file1 file2

**mcdiff6**
[-bcCdfhstVx?] [-o uscita] locale base remoto

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

**Shift-F6**
: Unire i due file rispetto alla loro base comune: la vista chiede il file
base e mostra l'unione di tre file. Lì F2 chiede prima dove salvare il
risultato.

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

# Unione di tre file <a id="merge"></a>

Con tre file, il visualizzatore mostra tre colonne: il file locale a
sinistra, il risultato dell'unione al centro e il file remoto a destra. I
file ai lati non vengono mai modificati; si salva solo la colonna centrale.

All'avvio la colonna centrale è il file base con già incluse tutte le
modifiche fatte da un solo lato. Dove entrambi i lati hanno cambiato le
stesse righe, ognuno a modo suo, il centro mantiene le righe della base: è
un conflitto, mostrato con il colore dei conflitti e contato dopo "Cnf" nella
riga di stato della colonna centrale.

Le differenze vengono ricalcolate dopo ogni modifica della colonna centrale.
Uno schermo più stretto di 120 colonne mostra il centro e un solo lato.

**F5**
: Aggiungere le righe del file di sinistra al blocco corrente della colonna
centrale, dopo ciò che già contiene. Il blocco corrente è quello sulla riga
in alto, oppure il primo sullo schermo.

**Shift-F5**
: Togliere di nuovo dal blocco le righe del file di sinistra. Se non resta
nessun lato, il blocco ha le righe della base: un conflitto torna a essere
un conflitto.

**F6**
: Aggiungere al blocco le righe del file di destra. Con entrambi i lati
aggiunti, le righe seguono l'ordine in cui sono state aggiunte.

**Shift-F6**
: Togliere di nuovo le righe del file di destra.

**], [**
: Andare al conflitto successivo o precedente.

**Enter, Space, n; Backspace, p**
: Andare al blocco successivo o precedente.

**F4**
: Modificare la colonna centrale nell'editor. Un blocco modificato lì conta
come risolto.

**F2**
: Salvare la colonna centrale. Se restano conflitti, prima chiede.

**C-u**
: Su uno schermo stretto, mostrare l'altro lato.

**F10**
: Uscire. Se la colonna centrale è stata cambiata e non salvata, chiede se
salvarla. Uscire senza modifiche lascia i file come sono.

Il programma termina con lo stato 0 se l'unione è stata salvata senza
conflitti rimasti, e 1 altrimenti, quindi può fare da strumento di unione
per git:

```
[merge]
    tool = mcdiff
[mergetool "mcdiff"]
    cmd = mcdiff6 "$LOCAL" "$BASE" "$REMOTE" -o "$MERGED"
    trustExitCode = true
```

Senza l'opzione
*-o*
l'unione viene salvata nel file base.

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
Normale confronta i file come sono. Il più veloce usa l'algoritmo histogram,
che resta veloce anche quando i file differiscono molto. Minimo impiega più
tempo per trovare un insieme più piccolo di differenze.

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
