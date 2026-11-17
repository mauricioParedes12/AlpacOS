# AlpacOS

AlpacOS es el mini sistema operativo que estamos desarrollando para el curso de Sistemas Operativos. La idea es llevar a la práctica lo que vemos en clase: desde el arranque del kernel hasta el manejo de procesos, memoria y archivos.

Está escrito en C11 y Assembly (GNU as), para x86 de 32 bits. Lo ejecutamos en QEMU y se puede compilar desde Linux o desde Windows con Ubuntu en WSL2.

## Avance actual: v0.8.0

El sistema ya arranca con Multiboot y tiene una consola con comandos propios. Cuenta con salida VGA y por puerto serie, teclado PS/2 por interrupciones, un temporizador PIT y un sistema de archivos en RAM. El arranque configura la GDT y la pila; el kernel instala la IDT y el PIC para atender excepciones e interrupciones.

En esta versión agregamos hilos del kernel y sincronización. Un proceso puede tener varios hilos, cada uno con su TID, pila y contexto. El planificador Round Robin ahora reparte la CPU entre hilos, no solamente entre procesos.

También incorporamos mutex. Cuando un hilo intenta tomar uno que ya está ocupado, queda en estado `BLOCKED`; al liberarlo, se despierta al siguiente hilo que estaba esperando. El shell incluye comandos para crear hilos y revisar sus estados.

En esta versión agregamos un gestor sencillo de marcos físicos de 4 KiB y activamos la paginación de x86. El kernel mantiene un mapeo identidad de los primeros 64 MiB y puede reservar marcos, mapearlos en direcciones virtuales de prueba y liberarlos. `meminfo` muestra ahora los marcos totales, usados y libres; `pagealloc` y `pagefree` permiten comprobar la reserva y liberación desde el shell.

El mapa de memoria es compartido por todo el kernel. Los procesos e hilos todavía no tienen directorios de páginas propios ni aislamiento entre sí. Tampoco hay memoria virtual bajo demanda ni swapping. La detección de deadlocks se agregó en la v0.8.0.

## Compilar y ejecutar

En Ubuntu o WSL2, instala las herramientas y ejecuta estos comandos desde la carpeta del proyecto:

```bash
sudo apt update
sudo apt install build-essential binutils qemu-system-x86
make
make run
```

Con `make run`, AlpacOS aparece en la misma terminal a través del puerto serie COM1. Para salir de QEMU, pulsa `Ctrl+A`, suelta las teclas y luego pulsa `X`.

Si prefieres usar una ventana con pantalla VGA y teclado PS/2:

```bash
make run-gui
```

El teclado está configurado con la distribución estadounidense (US).

## Comandos

| Comando | Para qué sirve |
|---|---|
| `help` | Muestra los comandos disponibles. |
| `clear` | Limpia la pantalla. |
| `echo hola` | Escribe el texto indicado. |
| `version` | Muestra la versión y la arquitectura. |
| `uptime` | Indica cuántos segundos lleva encendido el sistema. |
| `meminfo` | Muestra la RAM reportada, los marcos físicos usados/libres y si la paginación está activa. |
| `pagealloc` | Reserva un marco físico y lo mapea en una dirección virtual de prueba. |
| `pagefree 0xD0000000` | Desmapea y libera una página reservada; usa la dirección que mostró `pagealloc`. |
| `modules` | Muestra qué partes están implementadas y cuáles faltan. |
| `ps` | Lista los procesos, cuántos hilos tienen y sus contadores de CPU. |
| `threads` | Muestra TID, PID, estado, ticks y turnos de cada hilo, además del contador compartido de la prueba. |
| `spawn nombre` | Crea un proceso de prueba con su primer hilo. |
| `thread PID nombre` | Agrega otro hilo al proceso indicado. |
| `kill PID` | Solicita que terminen los hilos de demostración del proceso. |
| `ls [/ruta]` | Lista el contenido de un directorio. Sin ruta, muestra `/`. |
| `mkdir /ruta` | Crea un directorio dentro de otro que ya exista. |
| `touch /ruta` | Crea un archivo vacío. |
| `write /ruta texto` | Reemplaza el contenido de un archivo existente. |
| `cat /ruta` | Muestra el contenido de un archivo. |
| `rm /ruta` | Elimina un archivo. |
| `rmdir /ruta` | Elimina un directorio vacío. |
| `halt` | Detiene AlpacOS, aunque la ventana de QEMU sigue abierta. |

## Archivos en RAM

Para probar los archivos, puedes crear una carpeta, guardar una nota y leerla:

```text
mkdir /docs
touch /docs/nota.txt
write /docs/nota.txt Hola desde AlpacOS
cat /docs/nota.txt
ls /docs
```

`cat` debe mostrar `Hola desde AlpacOS`, y `ls` debe listar `nota.txt`. Para borrar todo lo creado:

```text
rm /docs/nota.txt
rmdir /docs
```

Los archivos se guardan en RAM, así que se pierden al reiniciar. Por ahora tenemos espacio para 32 entradas, contando la raíz, y hasta 255 caracteres de texto por archivo. Los nombres admiten hasta 23 caracteres y las rutas hasta 127.

Usamos rutas absolutas, como `/docs/nota.txt`. Los nombres pueden llevar letras, números, `_`, `-` y `.`, pero no espacios; tampoco se aceptan los segmentos `.` y `..`. Primero hay que crear el archivo con `touch`, ya que `write` solo cambia su contenido. El shell separa las palabras por espacios y no interpreta comillas.

Este sistema de archivos todavía no tiene permisos, usuarios, montajes ni almacenamiento en disco.

## Memoria y paginación

El gestor toma la cantidad de memoria básica reportada por Multiboot y administra marcos físicos de 4 KiB. Para evitar pisar el kernel, la pila, las tablas iniciales y los datos de arranque, reserva los primeros 4 MiB (o más si el kernel termina después de esa dirección). Por ahora trabaja con un máximo de 64 MiB.

Al iniciar, AlpacOS crea un directorio de páginas y tablas que mapean de forma identidad los primeros 64 MiB: una dirección virtual apunta al mismo número físico. Después activa el bit de paginación del registro `CR0`. Las tablas necesarias para direcciones virtuales adicionales se crean cuando se mapea una página y se liberan cuando quedan vacías.

Para probarlo desde el shell:

```text
meminfo
pagealloc
meminfo
pagefree 0xD0000000
meminfo
```

`pagealloc` muestra la dirección virtual y el marco físico asignado. También escribe y lee un valor de prueba en esa página. La dirección de ejemplo `0xD0000000` es la primera del área de prueba; usa exactamente la dirección que te devuelva el comando. `pagefree` quita el mapeo y devuelve el marco al allocator.

Este avance usa un único directorio de páginas compartido por el kernel, procesos e hilos. El mapeo identidad permite seguir ejecutando el código y las pilas actuales después de activar paginación, pero no proporciona aislamiento entre procesos. El allocator se basa en los campos de memoria básica de Multiboot y limita su administración a los primeros 64 MiB; todavía no interpreta el mapa E820 completo, no implementa espacios de direcciones independientes ni memoria virtual bajo demanda.

## Procesos, hilos y Round Robin

Cada proceso tiene un PCB con PID, nombre, estado, contadores y cantidad de hilos. El shell sigue siendo el proceso con PID 0. Se pueden tener hasta siete procesos de prueba activos.

Cada hilo tiene su propio TCB, TID, estado, contexto y pila de 8 KiB. El límite actual es de 24 hilos en total, incluido el hilo del shell. Los hilos de un mismo proceso comparten los datos del kernel; todavía no hay espacios de direcciones separados ni protección entre procesos.

El planificador usa una cola FIFO de hilos. El quantum sigue siendo de 10 ticks del PIT, aproximadamente 100 ms. Cuando termina el turno, el hilo que estaba ejecutándose vuelve al final de la cola y el siguiente recibe la CPU. El cambio de contexto guarda y restaura los registros desde la interrupción del temporizador.

El módulo de sincronización incluye un mutex no recursivo. Si un hilo intenta bloquear un mutex que pertenece a otro, se agrega a la cola de espera y pasa a `BLOCKED`. Cuando el propietario lo libera, la propiedad se transfiere al siguiente hilo y este vuelve a `READY`. Si un hilo intenta bloquear dos veces el mismo mutex, la operación falla en vez de dejarlo esperando por sí mismo. La detección de ciclos de espera se agregó en la v0.8.0.

### Probar procesos, hilos y mutex

Arranca AlpacOS y ejecuta los comandos usando el PID que muestre `spawn` (no siempre será 1):

```text
spawn grupo
thread 4 hiloB
thread 4 hiloC
ps
threads
```

En este ejemplo, `4` representa el PID devuelto por `spawn`; reemplázalo por el tuyo. El proceso empieza con un hilo y `thread` agrega los demás. Los hilos incrementan continuamente un contador compartido protegido por un mutex. Para que se pueda observar la sincronización, cada hilo cede la CPU mientras mantiene tomado el mutex. Cuando otro hilo intenta entrar, queda en estado `BLOCKED` hasta que el propietario libera el recurso.

El proceso de prueba permanece activo hasta que ejecutes `kill PID`. Ese comando solicita el cierre: cada hilo termina después de salir de su sección crítica y liberar el mutex. Mientras tanto, `ps` muestra el estado del proceso y `threads` permite revisar los TID, los estados y el contador compartido. El valor del contador sigue aumentando mientras los hilos están activos; no hay un resultado fijo como 600.

Los procesos y sus hilos siguen ejecutándose en ring 0 y comparten el espacio de memoria del kernel. El ejemplo usa un contador global asociado al PID; esto sirve para probar la exclusión mutua, pero todavía no equivale a aislamiento de memoria entre procesos.

## Pruebas

Las pruebas del sistema de archivos se ejecutan con:

```bash
make test
```

Revisan la creación de archivos y directorios, lectura, escritura, eliminación, límites y errores de rutas. Corren en Linux sin abrir QEMU. Hay más detalles en [docs/pruebas_vfs.md](docs/pruebas_vfs.md).

Para revisar la planificación y los mutex, usa el ejemplo de la sección anterior dentro de QEMU. Comprueba que los tres hilos queden asociados al mismo PID, que cada uno tenga un TID distinto y que el contador compartido aumente. Con varios hilos activos, `threads` debe permitir observar hilos en `BLOCKED` mientras esperan el mutex. Para cerrar la prueba, ejecuta `kill` con el PID del proceso y vuelve a consultar `ps` y `threads`. Para la memoria, ejecuta el ejemplo de `meminfo`, `pagealloc` y `pagefree` de la sección anterior; el número de marcos libres debe bajar al reservar la página y recuperarse al liberarla (la tabla de páginas vacía también se devuelve al allocator).

`make test` solo comprueba el VFS; las pruebas de procesos, hilos, sincronización y paginación se hacen al arrancar el kernel en QEMU. El shell y los comandos de archivos deben seguir funcionando mientras los hilos reciben sus turnos.

## Deadlocks (v0.8.0)

La detección usa el estado real de los mutex: registra qué hilo espera cada recurso y quién lo posee. Como cada mutex tiene un propietario, se puede recorrer la cadena de esperas para encontrar ciclos.

Para probarlo desde el shell:

```text
deadlock demo
deadlock check
threads
deadlock resolve
deadlock check
threads
```

`deadlock demo` crea un proceso con dos hilos. Cada uno toma un mutex distinto y después intenta tomar el del otro. `deadlock check` debe mostrar las dos dependencias circulares. `deadlock resolve` despierta a uno de los hilos para que cancele su segunda solicitud, libere el recurso que tenía y permita que el otro continúe.

La demostración necesita algunas conmutaciones del planificador. Si consultas el estado inmediatamente, repite `deadlock check` después de unos instantes.

Fuera de la demostración, `mutex_lock` rechaza la solicitud que formaría un ciclo: devuelve `false` y la rutina llamadora es responsable de liberar los mutex que ya tenía. El mecanismo detecta ciclos entre mutex de una sola instancia; no implementa un algoritmo general para recursos con múltiples instancias ni recupera automáticamente cualquier aplicación.

## Crear una ISO

Si quieres arrancar desde una imagen con GRUB:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La imagen queda en `build/alpacos.iso` y usa arranque BIOS. La carpeta `build/` contiene los archivos generados al compilar y no se sube al repositorio.
