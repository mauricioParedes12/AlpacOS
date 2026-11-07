# AlpacOS

AlpacOS es un mini sistema operativo para el proyecto final del curso de Sistemas Operativos. Está hecho en C y Assembly.

La versión **0.2.0** agrega interrupciones a la base del kernel y el shell. Se implementó la IDT, el manejo básico de excepciones, el controlador PIC y un temporizador PIT de aproximadamente 100 Hz. El teclado PS/2 ahora funciona mediante IRQ1 y un buffer de entrada.

Cuando no hay entrada, la CPU espera una interrupción usando `HLT`. El puerto serie sigue por sondeo y se revisa al despertar. Si ocurre una excepción, el kernel muestra el fallo y se detiene.

## Compilar y ejecutar

El entorno de trabajo es Windows con Ubuntu en WSL2. Desde la terminal de Ubuntu, instalar las herramientas necesarias:

```bash
sudo apt update
sudo apt install build-essential binutils qemu-system-x86
```

Luego, dentro de la carpeta del proyecto donde está el `Makefile`:

```bash
make
make run
```

Al iniciar aparece la consola de AlpacOS. Para cerrar QEMU desde este modo, presionar `Ctrl + A`, soltar las teclas y luego presionar `X`.

Para probar el teclado PS/2 y la pantalla VGA en una ventana, ejecutar `make run-gui`. El teclado utiliza distribución US.

## Comandos disponibles

| Comando | Función |
|---|---|
| `help` | Muestra los comandos disponibles. |
| `clear` | Limpia la consola. |
| `echo hola` | Muestra el texto ingresado. |
| `version` | Muestra la versión del sistema. |
| `uptime` | Muestra los segundos desde que se inició el temporizador. |
| `meminfo` | Muestra datos de memoria recibidos durante el arranque. |
| `modules` | Muestra qué partes están implementadas y cuáles faltan. |
| `halt` | Detiene AlpacOS, pero deja QEMU abierto. |

## Generar la ISO

Para crear una imagen arrancable con GRUB:

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
```

La imagen se guarda en `build/alpacos.iso` y utiliza arranque BIOS. La carpeta `build/` contiene archivos generados y no se incluye en Git.

## Trabajo pendiente

Todavía falta implementar gestión de procesos, planificación de CPU, threads, sincronización, memoria con paginación, sistema de archivos virtual y detección de deadlocks. Estas funciones se agregarán en los siguientes avances.
