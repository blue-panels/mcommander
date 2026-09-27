---
date: septiembre de 2026
---

<!-- help:topics "Índice de Contenidos:" -->
# NOMBRE <!-- help:skip -->

mview - Visor de Archivos Interno.

# SINOPSIS <!-- help:skip -->

**mview**
[-bcCdfhstVx?] arch

# Visor de Archivos Interno <a id="internal-file-viewer"></a>

El visor de archivos interno ofrece tres modos de presentación: ASCII,
hexadecimal y estructurado (árbol). Para alternar entre ASCII y
hexadecimal se emplea la tecla F4. Para entrar en el modo estructurado,
Alt-s o t. Dentro del modo ASCII, F9 recorre tres formas de mostrar los
mismos bytes: texto sin más, los colores ANSI que lleva el archivo, y un
emulador de terminal alimentado con el archivo.

El visor intenta usar el mejor método disponible en el sistema, según el
tipo de archivo, para mostrar la información.
En el modo nroff, que el
[archivo de extensiones](mcommander.md#edit-extension-file)
activa para las páginas del manual, las secuencias de sobreimpresión de una
página preformateada se muestran en negrita y subrayado en lugar de
imprimirse tal cual.

En modo ASCII las teclas del cursor desplazan la vista hasta que se activa
el cursor de lectura. Intro lo activa y lo vuelve a desactivar; una
pulsación del ratón y las teclas que seleccionan también lo activan. Con el
cursor activo, las teclas del cursor recorren el texto y Mayús-cursor,
Mayús-Inicio, Mayús-Fin, Mayús-RePág y Mayús-AvPág lo seleccionan.

Arrastrando con el botón izquierdo del ratón se selecciona texto. Un doble
clic selecciona una palabra y un triple clic una línea visual. Un clic
simple solo coloca el cursor sobre un carácter: marca el punto desde el que
las teclas con Mayús extienden la selección. Cuando el texto se desplaza
(AvPág, la rueda del ratón), el cursor se queda en la pantalla, en la misma
fila y columna. Ctrl-Ins o Intro copian la selección al archivo de
intercambio y de ahí al portapapeles del sistema; C-u quita la selección y
apaga el cursor, y copiar hace lo mismo. Donde no hay cursor que activar, en
los modos hexadecimal y árbol, Intro funciona como Abajo. Cualquier
movimiento del cursor sin Mayús pierde la selección y conserva el cursor. Lo
que se copia es el texto mostrado: el formato ANSI y nroff se quita, las
tabulaciones se convierten en espacios y, en modo filtro, solo se copian las
líneas visibles.

Como el botón izquierdo selecciona texto, el desplazamiento pulsando en el
tercio superior o inferior de la vista (véase
*mouse_move_pages*
en la sección [Viewer]) se hace en modo ASCII con el botón derecho o el
central. La rueda del ratón desplaza dos líneas cada vez, en todos los modos.

F6 filtra la vista como hace grep: solo se muestran las líneas que cumplen un
patrón, y la línea de estado las cuenta. El diálogo pide el patrón y los
mismos ajustes de tipo de búsqueda, mayúsculas y palabra completa que la
búsqueda; el botón Comprobar lista las líneas que el patrón escoge antes de
aplicar el filtro, y F1 allí abre la
[referencia rápida de expresiones regulares](mcommander.md#regex-quick-reference).
Un patrón vacío quita el filtro, y mientras nada coincide la vista lo dice y
se queda donde está. Las teclas ] y [ recorren las coincidencias, y C-t
activa el modo de seguimiento, en el que la vista se queda en la última
coincidencia mientras el archivo crece, como hace tail -f. Un archivo grande
se lee en segundo plano, y la línea de estado lo indica mientras se lee.

En modo hexadecimal, la función de búsqueda admite texto entre comillas y
números. El texto entrecomillado se busca tal cual, retiradas las comillas.
Cada número corresponde a un byte. Unos y otros se pueden entremezclar así:

```
"Cadena" 34 0xBB 012 "otro texto"
```

Los números se interpretan siempre en hexadecimal. En el ejemplo, "34" es
0x34. El prefijo "0x" no hace falta: se puede escribir "BB" en lugar de
"0xBB". Y "012" es 0x12, no un número octal.

El modo estructurado muestra los archivos JSON, YAML y XML como un árbol que
se despliega; un archivo HTML lo lee el mismo analizador que el XML. El
formato se deduce del nombre del archivo, de lo que dice de él la orden file
y del propio texto. Alt-s (o t) sobre un archivo admitido entra en el modo;
si el archivo no se puede analizar, o es mayor de lo que admite la vista de
árbol, se muestra un diagnóstico y el visor se queda en modo ASCII. El árbol
solo funciona con archivos locales, y al entrar en él quedan aparte el
filtro de líneas y el modo terminal. El YAML lo trata un analizador propio
que cubre el subconjunto de uso corriente (asignaciones y secuencias en
bloque, escalares en bloque, escalares entrecomillados, anclas y alias). Los
alias se expanden copiando; un alias cíclico, o una expansión que pasa del
presupuesto interno de nodos, se muestra como una referencia \*nombre en vez
de una copia. Las etiquetas y las colecciones en línea ([] y {}) no se
analizan y aparecen como valores de texto; los documentos fuera de ese
subconjunto (escalares sin comillas de varias líneas, tabulaciones en la
sangría) se avisan y se muestran como texto. Dentro del árbol, F4, Alt-s y t
vuelven al modo ASCII. La línea de estado muestra el tipo de contenido y la
ruta al estilo de jq del nodo actual (por ejemplo
**.spec.containers[0].image**).
Un clic mueve el cursor del árbol, y un clic en la fila donde está el cursor
despliega o pliega ese nodo.
El modo funciona también en el panel de vista rápida (C-x q), donde el árbol
sigue al cursor del panel. Con la opción
*structured_auto*
de la sección [Viewer] activada, los archivos admitidos se abren directamente
en el árbol. Teclas disponibles dentro del árbol:

```
Intro        desplegar/plegar el nodo actual; en una hoja,
             mostrar el valor completo
Derecha/Izq  desplegar / plegar (en un nodo plegado, Izquierda
             salta al padre)
*            desplegar el subárbol actual por completo
+ / -        desplegar / plegar todo el árbol
1 .. 9       desplegar el documento hasta la profundidad dada
Alt-Intro    copiar la ruta del nodo actual al portapapeles
F7, /        buscar en todo el documento, incluidos los nodos
             plegados; la ruta hasta la coincidencia se despliega
F17, n       continuar la búsqueda
F6           filtrar el árbol por un patrón
] / [        ir a la coincidencia siguiente / anterior
```

El filtro (F6) pide el patrón en el mismo diálogo que el del modo ASCII, con
los mismos ajustes de tipo, mayúsculas y palabra completa, y deja solo los
nodos cuya clave o valor coinciden. La ruta hasta cada coincidencia sigue
visible, y también lo que hay dentro de una coincidencia, de modo que un
nodo coincidente se puede abrir y recorrer. La línea de estado cuenta las
coincidencias; ] y [ las recorren. Un patrón vacío quita el filtro, igual
que salir del árbol. Un nodo coincide por el texto que muestra su fila, así
que un valor largo solo coincide hasta la longitud que el árbol guarda para
la vista previa (160 caracteres).

Las teclas que mueven por el texto mueven también por el árbol: los
cursores y h, j, k y l, las teclas de página y las que van al principio y al
final. Alt-e elige el juego de caracteres y C-o muestra la pantalla de
órdenes, igual que en el texto.

He aquí una lista de las acciones asociadas a cada tecla que M-Commander
gestiona en el visor interno de archivos.

**F1**
: Invoca el visor de ayuda de hipertexto interno.

**F2**
: Cambia el modo de ajuste de líneas. En modo hexadecimal pasa a editar los
bytes y vuelve; F6 escribe los cambios en el archivo.

**F4**
: Cambia entre el modo hexadecimal y el ASCII.

**Alt-s, t**
: Activa el modo estructurado (árbol) para archivos JSON, YAML y XML.

**F14**
: Muestra el archivo en el visor de estructuras, el complemento mcstruct, en
el byte donde está el cursor. Solo funciona con archivos locales.

**F6**
: Filtra la vista por un patrón. En modo hexadecimal, guarda los cambios
hechos en los bytes.

**C-t**
: Activa o desactiva el modo de seguimiento del filtro.

**], [**
: Va a la coincidencia siguiente o anterior del filtro.

**F5**
: Ir a. El diálogo admite un número de línea, un porcentaje del tamaño del
archivo o una posición en decimal o en hexadecimal, según cuál de las cuatro
se elija en él.

**F7, /, ?**
: Comienza una búsqueda. Estas teclas abren el diálogo con las opciones de
búsqueda. Con la tecla ? la opción "Hacia atrás" queda activada.

**C-s**
: Continúa la búsqueda hacia adelante.

**C-r**
: Continúa la búsqueda hacia atrás.

**F17, n**
: Continúa la búsqueda en el sentido elegido.

**N**
: Invierte el sentido de la búsqueda por una vez: hacia atrás si está elegida
la búsqueda hacia adelante, y al revés.

**Mayús-F8**
: Activa o desactiva el realce de sintaxis: el texto se colorea según las
reglas de sintaxis, las mismas y de la misma forma que en el editor interno.
Por encima de 4 MB el archivo recibe las reglas locales de línea, que
colorean números, cadenas entrecomilladas y puntuación sin tener que leer
todo lo que hay por encima de la línea mostrada. El texto conserva sus
propios colores mientras está activo el modo ANSI, el hexadecimal o el
terminal, y allí las reglas de sintaxis se apartan.

**Mayús-F9**
: Activa o desactiva la interpretación de las secuencias de color ANSI que
haya en el texto.

**F8**
: Alterna entre el modo crudo y el procesado: muestra el archivo tal como
está en disco o, si se ha indicado un filtro de visualización en el archivo
extensions.ini, la salida de ese filtro. El modo actual es siempre el
contrario al que dice la etiqueta del botón, ya que el botón muestra el modo
al que se entra con esa tecla.

**F9**
: Recorre las formas de mostrar el texto: sin más, luego con los colores ANSI
del archivo, y luego el modo terminal, en el que el archivo se pasa a un
emulador de terminal y lo que el visor muestra es la pantalla que este
dibuja. La etiqueta del botón nombra el modo al que lleva la tecla, no aquel
en el que está el visor. El modo terminal se construye a partir del flujo de
bytes y no de las líneas del archivo, así que allí el ajuste de líneas, el
salto a una línea, el filtro y la búsqueda no tienen sobre qué trabajar, y
la barra de botones lo indica.

**F3, F10, Esc, q**
: Sale del visor interno.

**AvPág, espacio, f, C-v**
: Avanza una página.

**RePág, b, Alt-v, Retroceso**
: Retrocede una página.

**d, u**
: Avanza o retrocede media página.

**C-a, C-e**
: Va al comienzo o al final de la línea.

**Inicio, C-Inicio, C-RePág, g**
: Va al comienzo del archivo.

**Fin, C-Fin, C-AvPág, G**
: Va al final del archivo.

**Abajo, Arriba**
: Mueven el cursor de texto una fila abajo o arriba; en el borde de la vista
el texto se desplaza una línea. En modo hexadecimal desplazan una línea.

**Izquierda, Derecha**
: Mueven el cursor de texto un carácter mostrado; al final de la fila sigue
por la siguiente. En modo hexadecimal mueven el cursor hexadecimal.

**h, j, k, l**
: Mueven a izquierda, abajo, arriba y derecha, como las teclas del cursor.
Las teclas Ins y Supr, C-p y C-n, e y, e también mueven una fila arriba y
abajo.

**Tab**
: En modo hexadecimal, cambia entre la columna hexadecimal y la de texto.

**C-Izquierda, C-Derecha**
: Mueven el cursor de texto ocho caracteres mostrados. Con el cursor apagado
y las líneas sin ajustar, desplazan la vista diez columnas a los lados.

**Mayús-Izquierda, Mayús-Derecha, Mayús-Arriba, Mayús-Abajo**
: Extienden la selección un carácter mostrado o una fila.

**Mayús-Inicio, Mayús-Fin**
: Extienden la selección hasta el principio o el final de la fila visual.

**Mayús-RePág, Mayús-AvPág**
: Extienden la selección hasta la primera o la última fila visible.

**Ctrl-Ins, Intro**
: Copian el texto seleccionado al archivo de intercambio y al portapapeles
del sistema. Sin selección, Intro funciona como Abajo.

**C-u**
: Quita la selección de texto.

**C-l**
: Redibuja el contenido de la pantalla.

**C-o**
: Alterna con la pantalla de órdenes del sistema.

**[n] m**
: Coloca la marca n.

**[n] r**
: Salta hasta la marca n.

**C-f**
: Salta al archivo siguiente. No en el panel de vista rápida, que sigue al
cursor del otro panel.

**C-b**
: Salta al archivo anterior. Tampoco en el panel de vista rápida.

**Alt-r**
: Recorre los modos de regla: arriba de la vista, abajo, y desactivada.

**Alt-Mayús-e**
: Abre la lista de los archivos vistos antes y muestra el que se elija.

**Alt-e**
: Cambia el juego de caracteres del texto mostrado. La recodificación va del
juego elegido al del sistema. Para anularla, elegir "\<No translation>" en
el diálogo de juegos de caracteres.

Es posible adiestrar al visor de archivos sobre cómo mostrar un archivo,
mírese la sección
[Editar Archivo de Extensiones](mcommander.md#edit-extension-file).


# Opciones del visor <a id="viewer-options"></a>

Los ajustes del
[visor interno](#internal-file-viewer)
que valen para todos los archivos que abre.

*Ajustar líneas largas.*
Si está activo, una línea más ancha que la pantalla sigue en la línea
siguiente; si no, se corta y la vista se desplaza a los lados. Activo de forma
predeterminada.

*Resaltado de sintaxis.*
Si está activo, el visor colorea el texto con las reglas de sintaxis del
editor. Un archivo abierto en un modo que trae sus propios colores, como una
página de manual o un Markdown compuesto, conserva esos colores. Inactivo de
forma predeterminada.

*Desplazamiento del ratón por páginas.*
Cuánto desplaza una pulsación en el tercio superior o inferior de la vista:
media pantalla si está activo, una línea si no. La rueda no se ve afectada,
siempre desplaza dos líneas. Activo de forma predeterminada.

*Recordar la posición en el archivo.*
Si está activo, el archivo se abre donde se dejó la última vez. Inactivo de
forma predeterminada.

*Vista de árbol de JSON, YAML y XML.*
Si está activo, un archivo de esos formatos se abre como un árbol plegable en
lugar de texto. La misma vista está siempre disponible con la tecla que cambia
el modo. Inactivo de forma predeterminada.

*Marca de fin de archivo.*
El texto que se imprime tras la última línea del archivo. Vacío de forma
predeterminada.

*Redibujados que se pueden saltar.*
Mientras se lee el archivo, el visor se salta redibujados para no quedarse
atrás. Aquí se dice cuántos seguidos puede saltarse antes de dibujar de todos
modos. 10 de forma predeterminada.

*Límite de tamaño para la vista de árbol, MB.*
El archivo más grande que la vista de árbol analiza. Uno mayor se rechaza
antes de leerlo. 64 de forma predeterminada.

*Límite de nodos de la vista de árbol.*
El árbol más grande que construye ese modo, contado en nodos. 10000000 de
forma predeterminada.

# VÉASE TAMBIÉN <a id="see-also"></a>

mcommander(1), mcedit6(1), mcdiff6(1).
