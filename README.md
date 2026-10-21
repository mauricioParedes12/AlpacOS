# AlpacOS

AlpacOS es un mini sistema operativo para el proyecto final del curso de Sistemas Operativos. Está hecho en C y Assembly.

En la versión **0.1.0** se implementó el arranque del kernel, una consola de texto y un shell con comandos básicos. Esta versión servirá como base para ir agregando las demás funciones faltantes del proyecto.

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

## Comandos disponibles

| Comando | Función |
|---|---|
| `help` | Muestra los comandos disponibles. |
| `clear` | Limpia la consola. |
| `echo hola` | Muestra el texto ingresado. |
| `version` | Muestra la versión del sistema. |
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

Todavía falta implementar interrupciones, gestión de procesos, planificación de CPU, threads, sincronización, memoria con paginación, sistema de archivos virtual y detección de deadlocks. Estas funciones se agregarán en los siguientes avances.