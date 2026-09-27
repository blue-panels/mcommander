---
date: septiembre de 2026
---

<!-- help:topics "Índice de Contenidos:" -->
# NOMBRE <!-- help:skip -->

mcdiff6 - Comparador de Archivos Interno.

# SINOPSIS <!-- help:skip -->

**mcdiff6**
[-bcCdfhstVx?] arch

# Comparador de Archivos Interno <a id="diff-viewer"></a>

mcdiff6 es una herramienta visual de comparación. Permite comparar dos
archivos y editarlos en el sitio, y la diferencia se calcula de nuevo tras
cada cambio. El complemento de panel git también lo abre, con el archivo tal
como lo tiene HEAD a un lado y el archivo del árbol de trabajo al otro.

El comparador ofrece los siguientes atajos de teclado:

**F1**
: Invoca el visor de ayuda y muestra esta sección.

**F2**
: Guarda los archivos modificados.

**F4**
: Edita el archivo del panel izquierdo en el editor interno.

**F14**
: Edita el archivo del panel derecho en el editor interno.

**F5**
: Lleva el fragmento actual al archivo de la derecha. Solo se combina el
fragmento actual y la diferencia se calcula de nuevo.

**F15**
: Lleva el fragmento actual en sentido contrario, al archivo de la izquierda.

**F7**
: Comenzar una búsqueda.

**F17**
: Repetir la búsqueda previa.

**F9**
: Abre las
[opciones de la comparación](#diff-options).

**Alt-e**
: Elegir el juego de caracteres con el que se leen los dos archivos.

**F10, Esc, q, Q**
: Salir del comparador.

**Alt-s, s**
: Mostrar/ocultar el estado de los fragmentos.

**Alt-n, l**
: Mostrar/ocultar números de línea.

**Ctrl-s**
: Activar o desactivar el realce de sintaxis. El texto de cada línea se
colorea según las reglas de sintaxis, igual que en el editor interno, y el
estado de la línea queda a cargo del fondo y de la columna de marcas. Donde
un skin distingue una palabra cambiada del resto de su línea solo por el
color del texto, la palabra se subraya en su lugar. El ajuste se recuerda
aparte del mismo ajuste del editor.

**f**
: Maximizar el panel izquierdo.

**=**
: Igualar el ancho de los paneles.

**>**
: Reducir el panel derecho.

**<**
: Reducir el panel izquierdo.

**2, 3, 4, 8**
: Fijar ancho de tabulaciones.

**Ctrl-u**
: Intercambia el contenido de los paneles.

**Ctrl-r**
: Vuelve a leer los dos archivos y calcula la diferencia de nuevo.

**Ctrl-o**
: Alternar con la pantalla de órdenes del sistema.

**Intro, Espacio, n**
: Avanzar al siguiente fragmento diferente.

**Backspace, p**
: Retroceder al fragmento diferente anterior.

**g, G**
: Saltar a la línea indicada.

**Abajo**
: Avanzar una línea.

**Arriba**
: Retroceder una línea.

**RePág**
: Retrocede una página.

**AvPág**
: Avanza una página.

**Izquierda, Derecha**
: Desplaza el texto una columna a los lados.

**Ctrl-Izquierda, Ctrl-Derecha**
: Desplaza el texto ocho columnas a los lados.

**Inicio**
: Vuelve a la primera columna.

**Ctrl-Inicio**
: Va al comienzo del archivo.

**Ctrl-Fin**
: Va al final del archivo.


# Opciones de la comparación <a id="diff-options"></a>

Los ajustes de la
[comparación interna](#diff-viewer),
que abre
**F9**
allí mismo y la entrada
**Opciones de la comparación**
del menú Opciones del gestor de archivos. La comparación toma sus ajustes al
arrancar, así que una ventana ya abierta conserva los que tenía.

*Algoritmo de comparación.*
El normal compara los archivos tal como son. El rápido da por supuesto que son
grandes y se conforma con un resultado más grueso. El mínimo dedica más tiempo
a encontrar un conjunto de diferencias más pequeño.

*Ignorar mayúsculas.*
Mayúsculas y minúsculas cuentan como el mismo carácter.

*Ignorar expansión de tabuladores.*
Las líneas que solo se diferencian en cómo está escrita la misma sangría, con
tabuladores o con espacios, cuentan como iguales.

*Ignorar cambios de espacios.*
Una serie de espacios cuenta como igual a cualquier otra.

*Ignorar todos los espacios.*
Los espacios quedan fuera de la comparación.

*Quitar el retorno de carro final.*
Se descarta el retorno de carro al final de la línea, de modo que un archivo
con fin de línea de DOS se compara con uno de Unix.

# VÉASE TAMBIÉN <a id="see-also"></a>

mcommander(1), mview(1), mcedit6(1).
