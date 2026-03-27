# Runtime Binary Patcher for Game Servers

[English](#english) | [Español](#español)

---

## English

### Overview

A Linux daemon that patches running game server processes **in memory** without modifying binaries on disk. Uses `ptrace` for process attachment, `/proc/PID/mem` for memory I/O, and remote `mmap` syscall execution to allocate executable code caves inside target processes.

Built to extend and fix a legacy 32-bit x86 Linux game server (no source code available) by injecting custom logic at runtime.

### Features

- **Non-invasive**: all patches are in-memory only, fully reversible on detach or process restart
- **40+ modular gameplay patches** (combat, items, PvP, quality-of-life)
- **Hot-reloadable config**: re-reads config file every 2 seconds, toggles patches live
- **Automatic process detection** via `/proc` scanning
- **PID recycling detection** using `/proc/PID/stat` start-time comparison
- **Hand-assembled x86 machine code** for code caves (no compiler/assembler dependency at runtime)
- **Graceful degradation**: failed patches don't block others from applying

### Architecture

```
                  +------------------+
                  |   Daemon Loop    |  (runs continuously)
                  +--------+---------+
                           |
              +------------+------------+
              |            |            |
        Config Reload  Proc Scan   Apply Patches
        (every 2s)    (/proc)     (per process)
                                       |
                          +------------+------------+
                          |            |            |
                     ptrace attach  read/write   ptrace detach
                                   /proc/PID/mem
                                       |
                              +--------+--------+
                              |                 |
                        Direct writes     Code caves
                        (value patches)   (logic patches)
                              |                 |
                         overwrite bytes   mmap remote →
                         at known addrs    build shellcode →
                                           write cave →
                                           JMP hook install
```

### Technologies

- **Language**: C (POSIX / Linux-specific)
- **Process control**: `ptrace(PTRACE_ATTACH/DETACH/GETREGS/SETREGS/SINGLESTEP)`
- **Memory I/O**: `/proc/PID/mem` (read/write via `pread`/`pwrite`)
- **Code injection**: Remote `mmap` via syscall hijacking, hand-assembled x86 shellcode
- **Process discovery**: `/proc` filesystem scanning, `/proc/PID/stat` parsing
- **Configuration**: Custom key=value parser with live reload

### Code Examples

| Module | Description |
|--------|-------------|
| [ptrace-core/](ptrace-core/) | Process attach/detach via ptrace |
| [memory-io/](memory-io/) | Read/write process memory via /proc/PID/mem |
| [code-caves/](code-caves/) | Remote mmap allocation via syscall injection |
| [process-scanner/](process-scanner/) | Automatic process detection + PID recycle guard |
| [config/](config/) | Hot-reloadable key=value config parser |
| [patch-example/](patch-example/) | Complete patch example: shellcode + JMP hook |
| [daemon/](daemon/) | Main daemon loop orchestrating everything |

### How a Patch Works (End to End)

1. **Daemon** detects the target process via `/proc` scanning
2. **ptrace** attaches to the process, stopping it
3. **Remote mmap** allocates an executable memory región (code cave) inside the target
4. **Shellcode** is hand-assembled and written into the code cave via `/proc/PID/mem`
5. A **JMP hook** is installed at the original function, redirecting execution to the cave
6. The cave runs custom logic, then jumps back to the original code flow
7. **ptrace** detaches, the process resumes with the patch active

### Building

This is a showcase/portfolio repository with educational code excerpts. Each module is self-contained and demonstrates a specific technique. The code compiles on any Linux system with standard headers:

```bash
gcc -Wall -Wextra -O2 -o patcher ptrace-core/process_attach.c memory-io/memory_rw.c ...
```

### License

Educational / Portfolio use. See individual files for details.

---

## Español

### Descripción General

Un demonio Linux que parchea procesos de servidores de juegos **en memoria** sin modificar los binarios en disco. Usa `ptrace` para adjuntarse a procesos, `/proc/PID/mem` para lectura/escritura de memoria, y ejecución remota de `mmap` vía syscall para asignar cuevas de código ejecutable dentro de los procesos objetivo.

Construido para extender y corregir un servidor de juegos legacy x86 de 32 bits en Linux (sin código fuente disponible), inyectando lógica personalizada en tiempo de ejecución.

### Características

- **No invasivo**: todos los parches son solo en memoria, completamente reversibles al desconectar o reiniciar el proceso
- **40+ parches modulares de gameplay** (combate, objetos, PvP, calidad de vida)
- **Configuración recargable en caliente**: relee el archivo de configuración cada 2 segundos, activa/desactiva parches en vivo
- **Detección automática de procesos** vía escaneo de `/proc`
- **Detección de reciclaje de PID** usando comparación de tiempo de inicio en `/proc/PID/stat`
- **Código máquina x86 ensamblado a mano** para cuevas de código (sin dependencia de compilador/ensamblador en runtime)
- **Degradación elegante**: parches fallidos no bloquean a los demás

### Arquitectura

```
                  +------------------+
                  |  Bucle Demonio   |  (ejecuta continuamente)
                  +--------+---------+
                           |
              +------------+------------+
              |            |            |
        Recarga Config  Escaneo Proc  Aplicar Parches
        (cada 2s)       (/proc)       (por proceso)
                                       |
                          +------------+------------+
                          |            |            |
                     ptrace attach  leer/escribir  ptrace detach
                                   /proc/PID/mem
                                       |
                              +--------+--------+
                              |                 |
                       Escrituras directas  Cuevas de código
                       (parches de valor)   (parches de lógica)
```

### Tecnologías

- **Lenguaje**: C (POSIX / específico de Linux)
- **Control de procesos**: `ptrace(PTRACE_ATTACH/DETACH/GETREGS/SETREGS/SINGLESTEP)`
- **E/S de memoria**: `/proc/PID/mem` (lectura/escritura vía `pread`/`pwrite`)
- **Inyección de código**: `mmap` remoto vía secuestro de syscall, shellcode x86 ensamblado a mano
- **Descubrimiento de procesos**: escaneo del filesystem `/proc`, parseo de `/proc/PID/stat`
- **Configuración**: Parser key=value personalizado con recarga en vivo

### Ejemplos de Código

| Módulo | Descripción |
|--------|-------------|
| [ptrace-core/](ptrace-core/) | Attach/detach de procesos vía ptrace |
| [memory-io/](memory-io/) | Leer/escribir memoria de proceso vía /proc/PID/mem |
| [code-caves/](code-caves/) | Asignación remota de mmap vía inyección de syscall |
| [process-scanner/](process-scanner/) | Detección automática de procesos + guardia contra reciclaje de PID |
| [config/](config/) | Parser de configuración key=value recargable en caliente |
| [patch-example/](patch-example/) | Ejemplo completo de parche: shellcode + JMP hook |
| [daemon/](daemon/) | Bucle principal del demonio orquestando todo |

### Cómo Funciona un Parche (De Principio a Fin)

1. El **demonio** detecta el proceso objetivo vía escaneo de `/proc`
2. **ptrace** se adjunta al proceso, deteniéndolo
3. **mmap remoto** asigna una región de memoria ejecutable (cueva de código) dentro del objetivo
4. El **shellcode** se ensambla a mano y se escribe en la cueva vía `/proc/PID/mem`
5. Se instala un **JMP hook** en la función original, redirigiendo la ejecución a la cueva
6. La cueva ejecuta lógica personalizada, luego salta de vuelta al flujo original
7. **ptrace** se desconecta, el proceso continúa con el parche activo
