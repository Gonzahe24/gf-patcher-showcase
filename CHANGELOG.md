# About This Repository

[English](#english) | [Español](#español)

## English

### What is this?

This is a **portfolio showcase** of a real runtime binary patcher for Linux game servers. The full project is private and actively used to extend and fix legacy game server binaries without source code.

### Why a showcase?

The complete codebase cannot be made public because:
- It contains real memory addresses and offsets specific to the target binaries
- Publishing the full code would allow unauthorized modification of game servers
- It includes server-specific configurations and gameplay balance parameters

Instead, this repository presents **simplified, educational versions** of the core patching techniques, with all memory addresses replaced by placeholders.

### What's real and what's simplified?

- **The techniques are real** — ptrace attachment, remote mmap via syscall injection, hand-assembled x86-64 code caves, and JMP hooks all work in production
- **The code is simplified** — focused on the patching mechanics, stripped of game-specific logic
- **All addresses are redacted** — real memory offsets replaced with `ADDR_XXX` placeholders

### About the project

- **40+ modular patches** covering combat balance, item protection, PvP mechanics, and QoL fixes
- **Hot-reloadable config** — patches can be toggled without restarting the daemon or the game servers
- **Non-invasive** — all modifications are in-memory only; original binaries are never modified on disk
- Written in **C (POSIX)** targeting Linux x86-64

### Technologies used in the full project

C (POSIX C99), ptrace, mmap, /proc filesystem, x86-64 assembly (hand-assembled machine code), code caves, JMP trampolines, syscall injection, signal handling, hot-reload configuration

---

## Español

### ¿Qué es esto?

Este es un **portfolio showcase** de un patcher de binarios en runtime real para servidores de juego Linux. El proyecto completo es privado y se utiliza activamente para extender y corregir binarios de servidores de juego legacy sin código fuente.

### ¿Por qué un showcase?

El código completo no puede ser público porque:
- Contiene direcciones de memoria y offsets reales específicos de los binarios objetivo
- Publicar el código completo permitiría la modificación no autorizada de servidores de juego
- Incluye configuraciones específicas del servidor y parámetros de balance de gameplay

En su lugar, este repositorio presenta **versiones simplificadas y educativas** de las técnicas centrales de patching, con todas las direcciones de memoria reemplazadas por placeholders.

### ¿Qué es real y qué está simplificado?

- **Las técnicas son reales** — ptrace, mmap remoto vía inyección de syscall, code caves x86-64 ensamblados a mano, y JMP hooks funcionan en producción
- **El código está simplificado** — enfocado en las mecánicas de patching, sin lógica específica del juego
- **Todas las direcciones están redactadas** — offsets reales reemplazados con placeholders `ADDR_XXX`

### Sobre el proyecto

- **40+ parches modulares** cubriendo balance de combate, protección de ítems, mecánicas PvP, y mejoras de calidad de vida
- **Configuración recargable en caliente** — los parches pueden activarse/desactivarse sin reiniciar el daemon ni los servidores
- **No invasivo** — todas las modificaciones son solo en memoria; los binarios originales nunca se modifican en disco
- Escrito en **C (POSIX)** para Linux x86-64

### Tecnologías utilizadas en el proyecto completo

C (POSIX C99), ptrace, mmap, sistema de archivos /proc, ensamblador x86-64 (código máquina ensamblado a mano), code caves, trampolines JMP, inyección de syscall, manejo de señales, configuración con recarga en caliente
