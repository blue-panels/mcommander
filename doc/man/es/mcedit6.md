---
date: septiembre de 2026
---

<!-- help:topics "Índice de Contenidos:" -->
# NOMBRE <!-- help:skip -->

mcedit6 - Editor de Archivos Interno.

# SINOPSIS <!-- help:skip -->

**mcedit6**
[-bcCdfhstVx?] arch

# Editor de Archivos Interno <a id="internal-file-editor"></a>

El editor de archivos interno es un editor a pantalla completa de avanzadas
prestaciones. Un archivo mayor que
*editor_filesize_threshold*
(64 MB por omisión) se abre tras una pregunta. También permite modificar
archivos binarios.
Se inicia pulsando
**F4**,
supuesto que la variable
*use_internal_edit*
esté presente en el archivo de inicialización.

Las características soportadas actualmente son: copia, desplazamiento,
borrado, corte y pegado de bloques; deshacer paso a paso; menús
desplegables; inserción de archivos; definición de macros; buscar y
reemplazar usando expresiones regulares; selección de texto con
mayúsculas-cursor (si el terminal lo soporta); alternancia
insertar-sobreescribir; plegado de líneas; sangrado automático; tamaño
de tabulación configurable; realce de sintaxis para varios tipos de
archivos; y la opción de pasar bloques de texto por filtros externos
como indent o ispell.

Secciones:
: [Opciones del editor](#editor-options)

El editor es muy fácil de usar y no requiere aprendizaje alguno.
Para conocer las teclas asignadas a cada función, basta consultar los
menús correspondientes. Además, las teclas de desplazamiento con la
tecla de mayúsculas seleccionan texto. Se puede seleccionar con el ratón,
aunque podemos recuperar su funcionamiento habitual en terminales (copiar
y pegar) manteniendo pulsada la tecla mayúsculas.
**Ctrl-Ins**
copia al archivo de intercambio
**~/.local/share/mc6/mcedit/mcedit.clip**,
**Mayús-Ins**
pega desde él,
**Mayús-Supr**
corta hacia él y
**Ctrl-Supr**
elimina el texto resaltado.

Para definir una macro, pulsar
**Ctrl-r**
y entonces teclearemos las secuencias de teclas que deseamos sean
ejecutadas. Pulsaremos
**Ctrl-r**
de nuevo al finalizar y asignaremos la macro a una tecla pulsando sobre
ella. La macro se ejecuta con esa tecla y también con
**Ctrl-a**
seguido de la tecla asignada. Las macros se guardan en la sección
**[editor]**
del archivo
**~/.local/share/mc6/macros**,
y se borra una macro eliminando su línea en ese archivo; cómo una macro
llama a un guion propio se explica en la sección MACRO de
**man mcedit6**.

El juego de caracteres del texto mostrado se cambia con Alt-e (M-e). La
recodificación va del juego elegido al del sistema. Para anularla,
elegir "\<No translation>" en el diálogo de juegos de caracteres.

El botón
**Filtro**
del diálogo de búsqueda
(**F7**)
oculta todas las líneas que no contienen el texto buscado, con los mismos
ajustes de tipo, mayúsculas y palabra completa que la búsqueda. Los números
de línea conservan su valor original y la columna de estado marca cada
tramo oculto. El conjunto de líneas ocultas queda fijado en el momento de
pulsar el botón: la edición no vuelve a aplicar la condición, de modo que
una línea visible que se parte o se une sigue visible, y las líneas
tecleadas después siguen visibles aunque no cumplan la condición.
**M-s**
quita el filtro; pulsada de nuevo vuelve a poner la última búsqueda como
filtro. "Desplegar todo" del menú Utilidades también lo quita.

# Opciones del editor <a id="editor-options"></a>

Los ajustes del
[editor interno](#internal-file-editor).
Los abre el menú
**Opciones**
del propio editor y la entrada
**Opciones del editor**
del menú Opciones del gestor de archivos. Se guardan en el archivo ini con los
nombres que describe el manual en inglés.

*Modo de ajuste de línea.*
Desactivado, formateo dinámico del párrafo, o el ajuste de máquina de escribir
que corta la línea a la longitud fijada mientras se teclea.

*Simular medias tabulaciones.*
Entre el texto y el margen izquierdo el movimiento y la sangría van de media
tabulación y se rellenan con espacios; en el resto la tabulación es normal.

*Retroceso a través de tabulaciones.*
Un solo retroceso borra toda la sangría hasta el margen izquierdo cuando entre
el cursor y el margen no hay texto.

*Rellenar tabulaciones con espacios.*
En lugar del carácter de tabulación se insertan espacios hasta la siguiente
posición.

*Ancho de tabulación.*
El ancho que ocupa el carácter de tabulación. 8 de forma predeterminada.

*Auto sangría con Enter.*
La línea nueva empieza con la sangría de la línea de arriba.

*Confirmar antes de guardar.*
Preguntar antes de escribir el archivo.

*Recordar la posición en el archivo.*
Abrir el archivo donde se dejó la última vez.

*Mostrar espacios finales.*
Se marcan los espacios al final de la línea.

*Mostrar tabulaciones.*
Se marcan los caracteres de tabulación.

*Mostrar caracteres de control.*
Los caracteres de control del texto se imprimen en lugar de ocultarse.

*Resaltado de sintaxis.*
El texto se colorea con las reglas de sintaxis de su tipo de archivo.

*Cursor tras el bloque insertado.*
Después de insertar un bloque el cursor queda al final y no al principio.

*Selección persistente.*
La selección se mantiene al mover el cursor en lugar de perderse.

*Cursor más allá del fin de línea.*
El cursor puede estar después del último carácter de la línea.

*Deshacer en grupo.*
Un solo deshacer devuelve una serie de cambios del mismo tipo y no una tecla.

*Longitud de línea para el ajuste.*
La columna por la que los modos de ajuste cortan la línea. 72 de forma
predeterminada.

# Guardar como <a id="save-file-as"></a>

El nombre con el que escribir el archivo y los fines de línea con los que
escribirlo: como los tiene el archivo, Unix (LF), Windows y DOS (CR LF) o
Macintosh (CR).

# Modo de guardar <a id="edit-save-mode"></a>

Cómo se escribe el archivo:

**Guardado rápido**
: Escribir sobre el archivo en el acto. Rápido, y un fallo a mitad deja el
archivo escrito a medias.

**Guardado seguro**
: Escribir primero un archivo temporal y renombrarlo sobre el original cuando
está entero, de modo que un fallo no toca el original.

**Crear copias de seguridad con la extensión**
: Guardado seguro, y el original se conserva con su nombre más la extensión de
la línea de entrada, "~" de forma predeterminada.

**Comprobar fin de línea POSIX**
: Preguntar por el fin de línea que falta al final del archivo antes de
escribirlo.

# Explorador de macros <a id="macro-explorer"></a>

Las macros grabadas, con la tecla a la que responde cada una y lo que hace.
Los botones son

**Ejecutar**
: Reproducir la macro donde está el cursor.

**Borrar**
: Quitarla, tras una pregunta.

**Editar archivo**
: Abrir el archivo donde viven las macros.

# Archivos abiertos <a id="open-files"></a>

Los archivos que el editor tiene abiertos, uno por línea. Enter va al archivo
donde está el cursor, Esc deja el que se muestra.

# Información de complementos <a id="plugin-info"></a>

Los complementos que el editor ha cargado: el nombre, si está activo, qué
ofrece y qué hace. Es una lista para mirar; se desactiva un complemento y se
abre su configuración en el diálogo
[Administrar complementos](mcommander.md#manage-plugins)
del gestor de archivos.

# VÉASE TAMBIÉN <a id="see-also"></a>

mcommander(1), mview(1), mcdiff6(1).
