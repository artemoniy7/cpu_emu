# 16-bit CPU Emulator

Console C++ project that emulates a small 16-bit CPU and a separate 10 MB byte-addressable storage area.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/cpu_emu
```

Every entered command is executed immediately and the emulator prints the result.

## Commands

- `HELP` — show command list.
- `REGS` — show all registers and flags.
- `RESET` — clear CPU registers, flags, and storage.
- `MOV R V` — copy a 16-bit value or another register into register `R`.
- `ADD R V`, `SUB R V`, `CMP R V` — arithmetic and comparison.
- `INC R`, `DEC R` — increment or decrement a register.
- `LOAD R ADDR` — load a 16-bit word from storage into `R`.
- `STORE R ADDR` — store register `R` as a 16-bit word.
- `PEEK ADDR`, `POKE ADDR V` — read or write one byte.
- `DUMP ADDR LEN` — print a memory range.
- `EXIT` — quit.

Registers: `AX`, `BX`, `CX`, `DX`, `SP`, `BP`, `SI`, `DI`.
Numbers can be decimal or hexadecimal (`0x10`).

## Example

```text
> MOV AX 0x1234
OK ZF=0 CF=0 SF=0
> STORE AX 0x100
OK
> DUMP 0x100 4
0x000100: 34 12 00 00
```
