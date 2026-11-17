# AlpacOS

AlpacOS es el mini sistema operativo que estamos desarrollando para el curso de Sistemas Operativos. La idea es llevar a la práctica lo que vemos en clase: desde el arranque del kernel hasta el manejo de procesos, memoria y archivos.

Está escrito en C11 y Assembly (GNU as), para x86 de 32 bits. Lo ejecutamos en QEMU y se puede compilar desde Linux o desde Windows con Ubuntu en WSL2.

## Avance actual: v0.6.0

El sistema ya arranca con Multiboot y tiene una consola con comandos propios. Cuenta con salida VGA y por puerto serie, teclado PS/2 por interrupciones, un temporizador PIT y un sistema de archivos en RAM. El arranque configura la GDT y la pila; el kernel instala la IDT y el PIC para atender excepciones e interrupciones.

En esta versión agregamos hilos del kernel y sincronización. Un proceso puede tener varios hilos, cada uno con su TID, pila y contexto. El planificador Round Robin ahora reparte la CPU entre hilos, no solamente entre procesos.

También incorporamos mutex. Cuando un hilo intenta tomar uno que ya está ocupado, queda en estado `BLOCKED`; al liberarlo, se despierta al siguiente hilo que estaba esperando. El shell incluye comandos para crear hilos y revisar sus estados.

Siguen pendientes la gestión de memoria con paginación y el manejo de deadlocks.

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
| `meminfo` | Muestra la memoria reportada al arrancar y el espacio reservado para el kernel. No calcula la RAM libre. |
| `modules` | Muestra qué partes están implementadas y cuáles faltan. |
| `ps` | Lista los procesos, cuántos hilos tienen y sus contadores de CPU. |
| `threads` | Muestra TID, PID, estado, ticks y turnos de cada hilo, además del contador compartido de la prueba. |
| `spawn nombre` | Crea un proceso de prueba con su primer hilo. |
| `thread PID nombre` | Agrega otro hilo al proceso indicado. |
| `kill PID` | Termina los hilos listos de un proceso, siempre que no tenga hilos bloqueados. |
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

## Procesos, hilos y Round Robin

Cada proceso tiene un PCB con PID, nombre, estado, contadores y cantidad de hilos. El shell sigue siendo el proceso con PID 0. Se pueden tener hasta siete procesos de prueba activos.

Cada hilo tiene su propio TCB, TID, estado, contexto y pila de 8 KiB. El límite actual es de 24 hilos en total, incluido el hilo del shell. Los hilos de un mismo proceso comparten los datos del kernel; todavía no hay espacios de direcciones separados ni protección entre procesos.

El planificador usa una cola FIFO de hilos. El quantum sigue siendo de 10 ticks del PIT, aproximadamente 100 ms. Cuando termina el turno, el hilo que estaba ejecutándose vuelve al final de la cola y el siguiente recibe la CPU. El cambio de contexto guarda y restaura los registros desde la interrupción del temporizador.

El módulo de sincronización incluye un mutex no recursivo. Si un hilo intenta bloquear un mutex que pertenece a otro, se agrega a la cola de espera y pasa a `BLOCKED`. Cuando el propietario lo libera, la propiedad se transfiere al siguiente hilo y este vuelve a `READY`. Si un hilo intenta bloquear dos veces el mismo mutex, la operación falla en vez de dejarlo esperando por sí mismo. La detección de deadlocks todavía no está implementada.

### Probar procesos, hilos y mutex

Arranca AlpacOS y ejecuta estos comandos:

```text
spawn grupo
thread 1 hiloB
thread 1 hiloC
ps
threads
```

`spawn` crea el proceso `grupo` con un hilo inicial. Los otros dos comandos agregan hilos al mismo PID. Los tres ejecutan una tarea de prueba que incrementa un contador compartido 200 veces. Cada hilo cede una vez la CPU mientras tiene tomado el mutex para que otro pueda intentar entrar y quedar bloqueado. Después mantiene el proceso ocupado unos cinco segundos de CPU para que puedas agregar los hilos desde el shell.

En `threads` puedes revisar los estados `READY`, `RUNNING`, `BLOCKED` y `TERMINATED`. El contador del proceso debe llegar a 600 cuando hayan terminado los tres hilos. Si ejecutas el comando antes de que acaben, vuelve a consultarlo para ver el avance. `ps` muestra el estado general del proceso y el total de hilos que siguen activos.

`kill PID` puede terminar los hilos que estén listos, pero no mata un proceso si alguno de sus hilos está bloqueado esperando un mutex. Esta restricción evita dejar referencias a TID que ya no existen en la cola de espera.

Los procesos y sus hilos siguen ejecutándose en ring 0 y comparten el espacio de memoria del kernel. El ejemplo usa un contador global asociado al PID; esto sirve para probar la exclusión mutua, pero todavía no equivale a aislamiento de memoria entre procesos.

## Pruebas

Las pruebas del sistema de archivos se ejecutan con:

```bash
make test
```

Revisan la creación de archivos y directorios, lectura, escritura, eliminación, límites y errores de rutas. Corren en Linux sin abrir QEMU. Hay más detalles en [docs/pruebas_vfs.md](docs/pruebas_vfs.md).

Para revisar la planificación y los mutex, usa el ejemplo de la sección anterior dentro de QEMU. Comprueba que los tres hilos queden asociados al mismo PID, que cada uno tenga un TID distinto y que el contador compartido termine en 600. También puedes ejecutar `ps` y `threads` varias veces para observar los estados y los turnos.

`make test` solo comprueba el VFS; las pruebas de procesos, hilos y sincronización se hacen en QEMU. El shell y los comandos de archivos deben seguir funcionando mientras los hilos reciben sus turnos.

## Crear una ISO

Si quieres arrancar desde una imagen con GRUB:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La imagen queda en `build/alpacos.iso` y usa arranque BIOS. La carpeta `build/` contiene los archivos generados al compilar y no se sube al repositorio.
