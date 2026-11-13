# AlpacOS

AlpacOS es el mini sistema operativo que estamos desarrollando para el curso de Sistemas Operativos. La idea es llevar a la práctica lo que vemos en clase: desde el arranque del kernel hasta el manejo de procesos, memoria y archivos.

Está escrito en C11 y Assembly (GNU as), para x86 de 32 bits. Lo ejecutamos en QEMU y se puede compilar desde Linux o desde Windows con Ubuntu en WSL2.

## Avance actual: v0.4.0

El sistema ya arranca con Multiboot y tiene una consola con comandos propios. Cuenta con salida VGA y por puerto serie, teclado PS/2 por interrupciones, un temporizador PIT y un sistema de archivos en RAM. El arranque configura la GDT y la pila; el kernel instala la IDT y el PIC para atender excepciones e interrupciones.

En esta versión agregamos los PCB y el cambio de contexto. Ahora podemos crear procesos de prueba, ejecutarlos por pasos y retomarlos sin que pierdan su avance. Por el momento elegimos qué proceso ejecutar desde el shell; todavía falta el planificador automático.

También quedan pendientes los hilos y la sincronización, la gestión de memoria con paginación y el manejo de deadlocks.

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
| `ps` | Lista los procesos con su PID, estado y nombre. |
| `spawn nombre` | Crea un proceso de prueba que cuenta hasta cinco. |
| `run PID` | Inicia o continúa el proceso indicado. |
| `kill PID` | Termina un proceso que esté en estado READY. |
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

## Procesos y cambio de contexto

Cada proceso tiene un PCB con su PID, nombre, estado, función de entrada y un puntero al contexto guardado en su pila. Reservamos una pila de 8 KiB para cada uno. El cambio de contexto se hace en `kernel/context.S`, donde se guardan y restauran los registros que exige la ABI i386, EFLAGS y la dirección de retorno.

El shell es el proceso con PID 0. Además de él, podemos tener hasta siete procesos activos, con nombres de 1 a 23 caracteres. Todos se ejecutan como tareas del kernel en ring 0 y comparten memoria; todavía no hay aislamiento entre procesos ni paginación. Las pilas tienen un espacio fijo reservado desde el inicio.

La ejecución funciona así: `run PID` le entrega la CPU a un proceso. Cuando este llama a `process_yield()`, vuelve al shell. Si ejecutamos otra vez `run` con el mismo PID, continúa donde se quedó. Cuando su función termina, `process_exit()` cambia su estado a TERMINATED y devuelve el control al shell.

Por eso hablamos de un cambio de contexto **cooperativo**. El temporizador aún no reparte turnos ni hay un quantum: si una tarea no cede la CPU ni termina, el shell no puede continuar.

### Probar dos procesos

Después de arrancar AlpacOS, ejecuta:

```text
spawn tareaA
spawn tareaB
ps
run 1
run 2
run 1
run 2
run 1
run 1
run 1
ps
kill 2
ps
spawn tareaC
run 3
```

Los PID serán 1, 2 y 3 si acabas de iniciar el sistema. Si ya creaste otros procesos, usa los números que te devuelva `spawn`.

En esta prueba, A llega al paso cinco y termina. B avanza dos pasos y luego la terminamos con `kill`. C aprovecha una plaza libre y empieza desde el paso uno. Los procesos terminados siguen apareciendo en `ps` hasta que se reutiliza su plaza con un PID nuevo. El shell debe aparecer como RUNNING cuando consultes la lista.

`spawn` crea instancias de la misma función de prueba, cada una con su propio contexto. Todavía no carga programas desde archivos.

También puedes probar `run 0`, `kill 0`, `run 999`, `run -1` y `run 4294967296`. Deben devolver un error sin detener el sistema. Si llenas las siete plazas, tendrás que terminar un proceso antes de crear otro.

## Pruebas

Las pruebas del sistema de archivos se ejecutan con:

```bash
make test
```

Revisan la creación de archivos y directorios, lectura, escritura, eliminación, límites y errores de rutas. Corren en Linux sin abrir QEMU. Hay más detalles en [docs/pruebas_vfs.md](docs/pruebas_vfs.md).

Para revisar los procesos, usa el ejemplo anterior dentro de QEMU. Después comprueba que `uptime` siga avanzando y que puedas usar los comandos de archivos. `make test` solo comprueba el VFS; no prueba el arranque ni el cambio de contexto.

## Crear una ISO

Si quieres arrancar desde una imagen con GRUB:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La imagen queda en `build/alpacos.iso` y usa arranque BIOS. La carpeta `build/` contiene los archivos generados al compilar y no se sube al repositorio.
