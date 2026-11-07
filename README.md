# AlpacOS

AlpacOS es un mini sistema operativo educativo para el proyecto final de Sistemas Operativos. Usa **C11 y Assembly** (GNU as), arquitectura x86 de 32 bits y arranque Multiboot v1 con QEMU.

## Estado del proyecto (0.3.0)

- **Arranque y kernel:** entrada Multiboot, GDT, pila y kernel freestanding.
- **Interrupciones:** IDT, manejo de excepciones y controlador PIC para IRQ0..IRQ15.
- **E/S:** consola VGA, puerto serie COM1 por sondeo, teclado PS/2 con IRQ1, temporizador PIT a unos 100 Hz.
- **Shell:** comandos propios, entrada por VGA/teclado o puerto serie.
- **Sistema de archivos virtual en RAM:** jerarquia de directorios, creacion, escritura, lectura, listado y eliminacion de archivos/directorios vacios.

**Pendiente:** procesos/PCB y cambio de contexto, planificacion de CPU, threads y sincronizacion, memoria/paginacion, gestion de deadlocks. La E/S existente es basica (no hay drivers de disco).

## Compilar y ejecutar

El entorno recomendado es Windows + Ubuntu en WSL2, o Ubuntu Linux.

```bash
sudo apt update
sudo apt install build-essential binutils qemu-system-x86
make
make run
```

`make run` ejecuta QEMU en terminal usando COM1. Para salir de QEMU: `Ctrl+A`, soltar, y `X`.

Para la pantalla VGA y el teclado PS/2 en una ventana, usar `make run-gui`. El teclado usa la distribucion US. Si no hay entrada de teclado, la CPU espera una interrupcion con `HLT`; la entrada COM1 se revisa al despertar.

### Comandos del shell

| Comando | Descripcion |
|---|---|
| `help` | Lista los comandos. |
| `clear` | Limpia la consola. |
| `echo hola` | Muestra argumentos. |
| `version` | Version y arquitectura. |
| `uptime` | Segundos desde el inicio del temporizador. |
| `meminfo` | Memoria reportada por el cargador y rango del kernel (no es RAM libre). |
| `modules` | Estado real de los modulos. |
| `ls [/ruta]` | Lista archivos y directorios (por defecto `/`). |
| `mkdir /ruta` | Crea un directorio si su padre ya existe. |
| `touch /ruta` | Crea un archivo vacio. |
| `write /ruta texto` | Sobrescribe el contenido del archivo existente. |
| `cat /ruta` | Lee el contenido del archivo. |
| `rm /ruta` | Elimina un archivo. |
| `rmdir /ruta` | Elimina un directorio vacio. |
| `halt` | Detiene AlpacOS (QEMU permanece abierto). |

Ejemplo de demostracion:

```text
alpacos> mkdir /docs
alpacos> touch /docs/nota.txt
alpacos> write /docs/nota.txt Hola desde AlpacOS
alpacos> cat /docs/nota.txt
Hola desde AlpacOS
alpacos> ls /docs
  [FILE] nota.txt (18 bytes)
alpacos> rm /docs/nota.txt
alpacos> rmdir /docs
alpacos> ls /
  (directorio vacio)
```

**Restricciones del VFS:** hasta 32 nodos (incluida la raiz), nombres de 23 caracteres, contenido de hasta 255 caracteres por archivo, rutas absolutas de hasta 127 caracteres. Se admiten letras, digitos, `_`, `-` y `.` en nombres; no se admiten los segmentos `.` o `..`, espacios en nombres ni rutas relativas. No hay permisos, usuarios, montajes, paginacion ni persistencia: **todos los archivos desaparecen al reiniciar**. `write` sobreescribe un archivo ya creado con `touch`; no lo crea automaticamente. El shell separa argumentos por espacios, sin comillas especiales.

## Pruebas

```bash
make test
```

Este objetivo ejecuta en Linux las pruebas unitarias de la capa VFS sin depender de QEMU; comprueba creacion, jerarquia, lectura, eliminacion, errores, limites y reinicio. `make` compila y enlaza el kernel de 32 bits con advertencias tratadas como errores. **No sustituye la prueba funcional con `make run` en QEMU.** Ver `docs/pruebas_vfs.md`.

## Generar ISO booteable

```bash
sudo apt install grub-common grub-pc-bin xorriso
make iso
make run-iso
```

La imagen `build/alpacos.iso` utiliza BIOS y GRUB. Los archivos de `build/` son generados y se excluyen del repositorio.
