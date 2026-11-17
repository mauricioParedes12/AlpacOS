# AlpacOS

AlpacOS es el mini sistema operativo que estamos desarrollando para el curso de Sistemas Operativos. El proyecto nos permite llevar a la práctica el arranque de un kernel, la planificación de CPU, los procesos, la sincronización, la memoria y los archivos.

Está escrito en C11 y Assembly de GNU, para x86 de 32 bits (i686). Se ejecuta en QEMU y se puede compilar desde Linux o desde Windows usando Ubuntu en WSL2.

## Versión actual: 0.10.0

En esta versión separamos el sistema de archivos en dos partes. El **VFS** resuelve las rutas y dirige las operaciones al sistema de archivos correspondiente. **RAMFS** se encarga de guardar los directorios y el contenido de los archivos en memoria.

Al arrancar, RAMFS queda montado en `/`. Ahora también podemos montar otras instancias en directorios existentes, consultar los montajes y desmontarlos desde la shell. Cada instancia tiene sus propios archivos. Los datos siguen siendo temporales: se pierden al desmontar esa instancia o al reiniciar AlpacOS.

El sistema cuenta con:

- Arranque Multiboot v1, GDT, pila e inicialización del kernel.
- Shell con comandos propios, salida VGA y consola por puerto serie.
- Interrupciones, temporizador PIT, teclado PS/2 y entrada COM1 con búferes.
- Procesos con PCB, hilos con TCB y cambio de contexto.
- Planificación Round Robin por hilos, con un quantum de 10 ticks, aproximadamente 100 ms.
- Mutex, semáforos, colas de espera, suspensión de hilos e hilo inactivo (`idle`).
- Gestión de marcos físicos de 4 KiB y paginación con un directorio de páginas por proceso.
- VFS, RAMFS y hasta cuatro montajes simultáneos, contando la raíz.
- Detección de ciclos de espera entre mutex y una demostración de recuperación de deadlocks.

## Compilar y ejecutar

En Ubuntu, Linux Mint o Ubuntu en WSL2, instala las herramientas:

```bash
sudo apt update
sudo apt install build-essential binutils qemu-system-x86
```

Desde la raíz del repositorio:

```bash
make
make run
```

`make` genera `build/alpacos.elf`. Con `make run`, AlpacOS se usa desde la misma terminal mediante el puerto serie COM1. Para salir de QEMU, pulsa `Ctrl+A`, suelta las teclas y después pulsa `X`.

Para abrir una ventana con pantalla VGA y teclado PS/2:

```bash
make run-gui
```

Esta opción necesita un entorno gráfico disponible. El teclado PS/2 usa la distribución estadounidense (US). La configuración del proyecto ejecuta QEMU con una CPU y 64 MiB de RAM.

### Arrancar desde una ISO

La imagen usa GRUB y arranque BIOS:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La ISO se genera en `build/alpacos.iso`. Para consultar las tareas disponibles, usa `make help`. `make clean` elimina únicamente la carpeta `build/`.

## Comandos de la shell

| Comando | Función |
|---|---|
| `help` | Lista los comandos disponibles. |
| `clear` | Limpia la consola. |
| `echo texto` | Muestra el texto indicado. |
| `version` | Muestra la versión y la arquitectura. |
| `uptime` | Muestra los segundos transcurridos desde el arranque del temporizador. |
| `modules` | Resume los módulos implementados. |
| `ioinfo` | Muestra los contadores de entrada recibida y descartada de COM1 y PS/2. |
| `meminfo` | Muestra la memoria administrada, los marcos libres y el estado de la paginación. |
| `pagealloc` | Reserva una página de prueba y comprueba su lectura y escritura. |
| `pagefree DIRECCION` | Libera una página reservada con `pagealloc`. |
| `procmap PID` | Muestra el directorio de páginas y el mapeo de la página de datos del proceso. |
| `faultdemo` | Crea un proceso que provoca un fallo de página. |
| `ps` | Lista procesos, estados, cantidad de hilos y contadores de CPU. |
| `threads` | Lista los hilos y muestra los contadores compartidos de la demostración. |
| `spawn nombre` | Crea un proceso de prueba con su primer hilo. |
| `thread PID nombre` | Agrega un hilo al proceso de prueba indicado. |
| `kill PID` | Termina los hilos del proceso, cancela sus esperas y libera sus recursos. |
| `deadlock demo` | Inicia la demostración de espera circular entre dos hilos. |
| `deadlock check` | Muestra las dependencias de espera y comprueba si hay ciclos. |
| `deadlock resolve` | Cancela una espera del ciclo para permitir la recuperación. |
| `ls [/ruta]` | Lista un directorio. Sin argumento, lista `/`. |
| `mkdir /ruta` | Crea un directorio dentro de otro que ya existe. |
| `touch /ruta` | Crea un archivo vacío. |
| `write /ruta texto` | Reemplaza el contenido de un archivo existente. |
| `cat /ruta` | Muestra el contenido de un archivo. |
| `rm /ruta` | Elimina un archivo. |
| `rmdir /ruta` | Elimina un directorio vacío que no esté ocupado por montajes. |
| `mount ramfs /directorio` | Monta una instancia vacía de RAMFS en un directorio existente. |
| `umount /directorio` | Desmonta esa instancia y descarta sus datos. |
| `mounts` | Lista los puntos de montaje y sus sistemas de archivos. |
| `halt` | Detiene AlpacOS. QEMU permanece abierto. |

La shell acepta hasta 127 caracteres por línea y 16 palabras, contando el comando. Separa los argumentos por espacios o tabulaciones; no interpreta comillas, redirecciones ni tuberías.

## Archivos y montajes

Para crear y leer un archivo en el sistema raíz:

```text
mkdir /docs
touch /docs/nota.txt
write /docs/nota.txt Hola desde AlpacOS
cat /docs/nota.txt
ls /docs
```

`write` necesita que el archivo ya exista. Para eliminarlo y borrar el directorio:

```text
rm /docs/nota.txt
rmdir /docs
```

### Probar un montaje

Este ejemplo permite ver la diferencia entre el directorio original y la instancia que se monta sobre él:

```text
mkdir /mnt
touch /mnt/original.txt
write /mnt/original.txt Archivo del sistema raiz
mount ramfs /mnt
ls /mnt
touch /mnt/nuevo.txt
write /mnt/nuevo.txt Archivo del montaje
cat /mnt/nuevo.txt
mounts
umount /mnt
ls /mnt
cat /mnt/original.txt
```

El primer `ls /mnt`, después de montar RAMFS, muestra un directorio vacío. El archivo `original.txt` queda oculto mientras el montaje está activo. Al desmontarlo, vuelve a aparecer con su contenido; `nuevo.txt` se descarta junto con la instancia desmontada.

También se pueden crear montajes dentro de otros montajes. Para desmontar el padre, primero hay que desmontar sus hijos. El VFS impide borrar un punto de montaje activo, borrar un directorio que lo contiene o desmontar `/`.

Cada instancia de RAMFS admite 32 entradas, contando su raíz. Cada archivo guarda hasta 255 bytes de texto; los nombres admiten hasta 23 caracteres y las rutas completas hasta 127. El límite de línea de la shell puede reducir el texto que se escribe en un solo comando.

Las rutas son absolutas, como `/docs/nota.txt`. Los nombres admiten letras ASCII, números, `_`, `-` y `.`. No se aceptan espacios, rutas relativas, los segmentos `.` y `..`, barras consecutivas ni una barra final, salvo en `/`.

## Procesos, hilos y sincronización

Cada proceso tiene un PCB con su PID, estado, contadores y espacio de direcciones. Cada hilo tiene su propio TID, estado, contexto y pila de 8 KiB. Los hilos de un proceso comparten su espacio de direcciones.

El planificador reparte la CPU entre los hilos listos usando Round Robin. El temporizador permite interrumpir un hilo cuando termina su quantum. Los hilos también pueden ceder la CPU, dormir o bloquearse al esperar un recurso. Cuando no hay trabajo disponible, se ejecuta `idle`.

Para probarlo, crea un proceso:

```text
spawn grupo
```

Si el comando devuelve el PID `1`, puedes continuar así. Si devuelve otro número, reemplaza `1` en los comandos:

```text
thread 1 hiloB
thread 1 hiloC
ps
threads
procmap 1
kill 1
```

Los hilos de esta demostración incrementan un contador compartido protegido por un mutex. Se cede la CPU dentro de la sección crítica para que se pueda observar a otros hilos en `BLOCKED` mientras esperan. El contador sigue aumentando hasta terminar el proceso.

El módulo de sincronización también incluye semáforos con colas de espera. Se usan mediante la API del kernel; esta versión no tiene un comando específico para una demostración productor–consumidor.

Hay espacio para ocho procesos en total: el proceso del sistema con PID 0 y hasta siete procesos de trabajo. El límite es de 24 hilos, incluidos los del shell y `idle`, por lo que quedan hasta 22 hilos de trabajo. El PID 0 está protegido frente a `kill`.

## Memoria y paginación

AlpacOS administra marcos físicos de 4 KiB. Usa el mapa de memoria que entrega Multiboot cuando está disponible; si no lo está, recurre a la información básica de memoria. Reserva las regiones necesarias para el kernel y el arranque, y administra RAM hasta el límite de 64 MiB.

El kernel comparte un mapeo identidad de los primeros 64 MiB, con la página cero ausente para detectar accesos a direcciones nulas. Cada proceso de trabajo tiene su propio directorio de páginas y una página de datos en la dirección virtual `0x40000000`. Esa dirección puede apuntar a marcos físicos distintos en procesos diferentes. `procmap PID` permite observarlo.

Para probar la reserva y liberación desde el shell:

```text
meminfo
pagealloc
meminfo
pagefree 0xD0000000
meminfo
```

Usa en `pagefree` la dirección que muestre `pagealloc`. `0xD0000000` es la primera dirección del área de prueba, que tiene espacio para 16 páginas. Al liberar la última página de una tabla, también se libera esa tabla.

`faultdemo` crea un proceso que intenta escribir en una dirección sin mapear. El manejador registra el fallo y termina ese proceso; la shell sigue funcionando. Un fallo de página en el proceso del sistema detiene el kernel.

Aunque los procesos tienen mapeos propios, todos se ejecutan en **ring 0** y comparten las regiones del kernel. Esto todavía no proporciona protección completa entre procesos ni separación entre modo usuario y núcleo. Tampoco hay paginación bajo demanda, reemplazo de páginas o swapping.

## Deadlocks

La detección sigue las dependencias entre hilos: qué mutex espera cada uno y qué hilo lo posee. Si esa cadena forma un ciclo, hay una espera circular.

Para reproducirla:

```text
deadlock demo
deadlock check
threads
deadlock resolve
deadlock check
```

La demostración crea dos hilos que toman dos mutex en orden inverso. Necesita algunas conmutaciones del planificador; si consultas demasiado pronto, repite `deadlock check` después de unos instantes.

`deadlock resolve` cancela una de las esperas. El hilo afectado puede reanudarse, liberar el mutex que tenía y permitir que el otro continúe. Si revisas el grafo inmediatamente después, puede hacer falta otra conmutación para ver la recuperación completada.

Fuera de esta demostración, `mutex_lock` rechaza una solicitud que formaría un ciclo y devuelve `false`. El código que hizo la solicitud debe manejar ese resultado y liberar los recursos que ya posee. La detección se limita a dependencias entre mutex de un solo propietario; no es un detector general de bloqueos entre todos los recursos del sistema.

## Pruebas

Las pruebas incluidas se ejecutan en el equipo anfitrión, sin abrir QEMU:

```bash
make test
```

Comprueban las operaciones básicas de archivos y directorios, las rutas inválidas, los límites y el reinicio del VFS. Este comando no ejecuta pruebas del planificador, de la paginación ni de los montajes anidados.

Para comprobar el kernel, ejecuta `make run` y usa los ejemplos anteriores. Al probar montajes, verifica que el contenido original reaparezca al desmontar. En la prueba de memoria, comprueba que los marcos libres se recuperen después de `pagefree`. En las pruebas de procesos y deadlocks, revisa los estados con `ps` y `threads` y confirma que la shell siga respondiendo.

## Organización del código

| Ruta | Contenido |
|---|---|
| `boot/` | Arranque Assembly, script del enlazador y configuración de GRUB. |
| `kernel/` | Inicialización, consola, memoria, procesos, sincronización y deadlocks. |
| `kernel/interrupts/` | Excepciones, interrupciones y rutinas de entrada Assembly. |
| `kernel/fs/vfs.c` | Resolución de rutas, nodos y tabla de montajes. |
| `kernel/fs/ramfs.c` | Directorios y archivos almacenados en RAM. |
| `kernel/shell/` | Lectura de comandos, análisis de argumentos y ejecución. |
| `drivers/` | VGA, puerto serie, teclado y temporizador. |
| `include/alpacos/` | Interfaces y constantes del kernel. |
| `lib/` | Funciones de cadenas y memoria usadas por el kernel. |
| `tests/` | Pruebas del sistema de archivos en el anfitrión. |

Los archivos generados al compilar quedan en `build/`.

## Alcance de esta versión

AlpacOS es un sistema educativo para una CPU. `spawn` ejecuta una rutina de demostración compilada dentro del kernel; todavía no carga programas externos. La GDT usa segmentación plana, y la paginación aún no recupera páginas ausentes ni elige páginas para reemplazar.

RAMFS es el único sistema de archivos incluido. El acceso se realiza por rutas y sobre archivos de texto completos: todavía no hay descriptores por proceso, permisos, usuarios ni almacenamiento persistente en disco.
