# Avance 0.3.0: sistema de archivos virtual en RAM

## Objetivo

Cubrir el requisito **8. Sistema de archivos virtual** del proyecto final de Sistemas Operativos con un sistema de archivos RAM educativo integrado en el shell y el kernel de AlpacOS.

## Diseno e implementacion

- `include/alpacos/vfs.h` define la API y los limites.
- `kernel/fs/vfs.c` implementa un arreglo estatico de nodos, con indice del padre, tipo archivo/directorio, nombre y contenido.
- El nodo 0 es la raiz `/`; las rutas se recorren por segmentos y se validan antes de realizar operaciones.
- `vfs_init()` inicializa la raiz durante el arranque de `kernel_main()`.
- El shell ofrece `ls`, `mkdir`, `touch`, `write`, `cat`, `rm` y `rmdir`.
- El contenido se almacena en memoria del kernel y **no se conserva al apagar o reiniciar**.

### Estructura logica

```text
/
└── docs/             [DIRECTORIO]
    └── nota.txt       [ARCHIVO, contenido en RAM]
```

El prototipo reserva una tabla de 32 nodos (uno es la raiz) y un buffer fijo de 256 bytes por nodo. Esta estrategia no exige un asignador dinamico, pero limita el numero y tamano de archivos. No se realiza E/S de disco, no existe aislamiento de memoria y no es una implementacion POSIX ni un sistema de archivos persistente.

## Pruebas automatizadas

Comando: `make test`.

| Prueba | Resultado esperado |
|---|---|
| Crear directorio `/docs` y archivo `/docs/hola.txt` | Ambos se crean y figuran en sus directorios |
| Escribir y leer texto | Los datos y longitud se conservan |
| Intentar borrar directorio no vacio | `VFS_ERR_NOT_EMPTY` |
| Intentar borrar directorio usando `rm` | `VFS_ERR_IS_DIR` |
| Intentar borrar archivo usando `rmdir` | `VFS_ERR_NOT_DIR` |
| Rutas invalidas (relativas, `//`, `.` o `..`, barra final) | `VFS_ERR_INVALID_PATH` |
| Crear un archivo repetido | `VFS_ERR_EXISTS` |
| Acceder a un padre inexistente | `VFS_ERR_NOT_FOUND` |
| Superar 255 caracteres de contenido | `VFS_ERR_TOO_LARGE` y datos anteriores intactos |
| Crear mas de 31 elementos adicionales | `VFS_ERR_FULL` |
| Borrar un archivo y reutilizar su entrada | Nueva creacion exitosa |
| Reinicializar el VFS | Directorio raiz vacio |

Las pruebas se implementaron en `tests/test_vfs.c`; los resultados concretos deben obtenerse ejecutando el comando en la version que se entregue.

## Demostracion manual con QEMU

Ejecutar `make run`, luego introducir en el shell:

```text
mkdir /docs
mkdir /docs/sub
ls /
ls /docs
touch /docs/sub/nota.txt
write /docs/sub/nota.txt Prueba del VFS
cat /docs/sub/nota.txt
rmdir /docs
rm /docs/sub/nota.txt
rmdir /docs/sub
rmdir /docs
ls /
```

Se espera que el primer `rmdir /docs` produzca el error *directorio no esta vacio*. El segundo debe eliminarlo una vez quitados archivo y subdirectorio. Reiniciar QEMU y ejecutar `ls /` para comprobar que el sistema de archivos es volatil.

## Alcance de este avance y trabajo futuro

Este modulo cubre una **version educativa basica** del sistema de archivos virtual, no un sistema de archivos con discos, controladores de bloques, descriptores de archivo ni montajes. Es independiente de los componentes pendientes: procesos/PCB, scheduler, hilos/sincronizacion, paginacion y deadlocks. Se recomienda implementar posteriormente los PCB y el planificador usando el temporizador ya existente.
