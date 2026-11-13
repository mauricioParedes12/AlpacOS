# AlpacOS

AlpacOS es el mini sistema operativo que estamos desarrollando para el curso de Sistemas Operativos. La idea es llevar a la práctica lo que vemos en clase: desde el arranque del kernel hasta el manejo de procesos, memoria y archivos.

Está escrito en C11 y Assembly (GNU as), para x86 de 32 bits. Lo ejecutamos en QEMU y se puede compilar desde Linux o desde Windows con Ubuntu en WSL2.

## Avance actual: v0.5.0

El sistema ya arranca con Multiboot y tiene una consola con comandos propios. Cuenta con salida VGA y por puerto serie, teclado PS/2 por interrupciones, un temporizador PIT y un sistema de archivos en RAM. El arranque configura la GDT y la pila; el kernel instala la IDT y el PIC para atender excepciones e interrupciones.

En esta versión agregamos el planificador Round Robin. Los procesos reciben turnos de CPU automáticamente y el shell puede seguir atendiendo comandos mientras trabajan. Usamos el temporizador PIT, que genera unas 100 interrupciones por segundo, con un quantum de 10 ticks: aproximadamente 100 ms por turno.

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
| `ps` | Lista los procesos, sus ticks de CPU y los turnos que han recibido. |
| `spawn nombre` | Crea y pone en marcha una tarea de prueba de unos cinco segundos de CPU. |
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

## Procesos y Round Robin

Cada proceso tiene un PCB con su PID, nombre, estado y contexto guardado en una pila de 8 KiB. El shell sigue siendo el proceso con PID 0 y podemos tener hasta siete procesos más. Todos son tareas del kernel en ring 0 y comparten memoria; el aislamiento y la paginación siguen pendientes.

El planificador usa una cola FIFO. Al crear un proceso, lo agrega al final de la cola de listos. Cuando se acaba el quantum, el proceso que estaba ejecutándose vuelve al final y el primero de la cola recibe la CPU. El shell participa en los mismos turnos. Si no hay otro proceso listo, el actual continúa.

El cambio se hace al salir de la interrupción del temporizador. `kernel/interrupts/isr.S` guarda los registros generales, los segmentos y el estado de retorno. El planificador elige qué contexto restaurar y la instrucción `iret` reanuda la tarea en el punto donde fue interrumpida. La IRQ se confirma al PIC antes de pasar al siguiente proceso.

También conservamos `process_yield()` para ceder un turno voluntariamente. Ahora usa una interrupción de software desde `kernel/context.S`, pero las tareas de prueba no la llaman: el temporizador tiene que interrumpirlas para que el resto pueda avanzar. Cuando una tarea termina su función, sale de la planificación automáticamente.

Las actualizaciones de los PCB, la cola y la salida de consola se protegen con secciones cortas en las que se desactivan las interrupciones. Seguimos trabajando con una sola CPU virtual y sin mutex ni semáforos.

### Probar la planificación

Después de arrancar AlpacOS, ejecuta:

```text
spawn tareaA
spawn tareaB
ps
uptime
```

Espera un par de segundos y vuelve a escribir `ps`. Los valores de TICKS y TURNOS de ambas tareas deberían haber aumentado, aunque no hayas ejecutado ningún comando para darles CPU. El comando `run` de la versión anterior ya no se usa y se retiró del shell.

En la salida de `ps`:

- **TICKS** cuenta las interrupciones del PIT recibidas mientras ese proceso tenía la CPU.
- **TURNOS** cuenta cuántas veces el planificador le entregó la CPU.
- **READY** indica que espera su turno, **RUNNING** que está ejecutándose y **TERMINATED** que ya terminó.

La tarea de prueba hace trabajo continuo, sin `yield` ni `hlt`, hasta acumular unos 500 ticks de CPU. Después termina sola. Con varios procesos, tarda más de cinco segundos de reloj porque comparte los turnos con los demás. El progreso se consulta con `ps`; las tareas no imprimen mensajes en segundo plano para no cortar lo que estás escribiendo.

Estos contadores sirven para observar el reparto de turnos. En el caso del shell, sus ticks también incluyen el tiempo que pasa esperando entrada con `hlt`, así que no representan un porcentaje exacto de uso de CPU.

Mientras las tareas trabajan, puedes probar los comandos de archivos o terminar una con `kill`, usando el PID que te haya devuelto `spawn`:

```text
kill 1
ps
spawn tareaC
```

Los procesos terminados permanecen en la lista hasta que se reutiliza su plaza, con un PID nuevo. Si ya llenaste las siete plazas, tendrás que esperar a que termine una tarea o usar `kill` antes de crear otra. Los nombres siguen admitiendo de 1 a 23 caracteres.

`spawn` crea instancias de la misma función de prueba, cada una con su propia pila y contexto. Todavía no carga programas desde archivos. Como todo se ejecuta en ring 0, una tarea que desactive las interrupciones de forma permanente puede detener la planificación; aún no hay protección frente a ese caso.

## Pruebas

Las pruebas del sistema de archivos se ejecutan con:

```bash
make test
```

Revisan la creación de archivos y directorios, lectura, escritura, eliminación, límites y errores de rutas. Corren en Linux sin abrir QEMU. Hay más detalles en [docs/pruebas_vfs.md](docs/pruebas_vfs.md).

Para revisar Round Robin, usa el ejemplo anterior dentro de QEMU y comprueba que los contadores aumenten sin intervención manual. Prueba también llenar las siete plazas, terminar una tarea y crear otra; la nueva debe recibir un PID distinto y llevar sus propios contadores. `kill 0`, `kill -1`, `kill 999` y `kill 4294967296` deben mostrar un error sin bloquear el sistema.

`make test` solo comprueba el VFS; las pruebas del arranque y de la planificación se hacen en QEMU. El shell y los comandos de archivos deben seguir funcionando mientras las tareas reciben sus turnos.

## Crear una ISO

Si quieres arrancar desde una imagen con GRUB:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La imagen queda en `build/alpacos.iso` y usa arranque BIOS. La carpeta `build/` contiene los archivos generados al compilar y no se sube al repositorio.
