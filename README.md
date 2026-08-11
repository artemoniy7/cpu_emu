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

Every entered command is executed immediately and the emulator now prints the result directly under the prompt while keeping command history for the optional full-screen renderer. Debug output is enabled by default; use `DEBUG OFF` to hide flag/details-only responses while keeping operational output such as memory, disk, and plain `PRINT` results. Files and programs are persisted into the emulated disk images, so they can be created, read back, and run later from inside the emulator.

## Commands

- `HELP` — show command list.
- `REGS` — show all registers and flags.
- `RESET` — clear CPU registers, flags, and storage.
- `DEBUG ON`, `DEBUG OFF`, `DBG ON`, `DBG OFF` — enable or disable debug output; it is enabled by default.
- `MOV R V` — copy a 16-bit value or another register into register `R`.
- `XCHG R1 R2` — exchange two registers.
- `PUSH V`, `POP R` — use the 16-bit stack through `SP`.
- `ADD R V`, `ADC R V`, `SUB R V`, `SBB R V`, `CMP R V`, `NEG R`, `MUL V`, `IMUL V`, `DIV V`, `IDIV V` — arithmetic and comparison with carry/borrow and signed/unsigned multiply/divide.
- `INC R`, `DEC R` — increment or decrement a register.
- `CLC`, `STC`, `CMC`, `CLD`, `STD` — realistic flag-control instructions for carry and string direction.
- `NOP`, `HLT` — no-operation and simulated halt instructions.
- `LOAD R ADDR` — load a 16-bit word from storage into `R`.
- `IN R PORT`, `OUT PORT V` — read/write byte-sized I/O ports, like a simple hardware bus.
- `STORE R ADDR` — store register `R` as a 16-bit word.
- `PEEK ADDR`, `POKE ADDR V` — read or write one byte.
- `DUMP ADDR LEN` — print a memory range.
- `INPUT ADDR` — wait for keyboard input and write the entered text or number to memory at `ADDR`.
- `DIR [PATH]`, `CD [PATH]`, `MD/MKDIR PATH`, `RD/RMDIR PATH` — DOS-like directory work.
- `ECHO TEXT > FILE`, `TYPE FILE`, `COPY SRC DST`, `DEL FILE` — DOS-like file work.
- `NEW FILE`, `APPEND FILE CMD`, `RUN/EXEC FILE` — create, store on disk, load, and run emulator programs.
- Program files can use `LABEL:`, `JMP/JE/JNE/... LABEL`, `CALL LABEL`, `RET`, and `LOOP LABEL` for realistic control flow while running from disk.
- `C:`, `D:`, `E:`, `F:` — switch the active drive.
- `EXIT` — quit.

Registers: `AX`, `BX`, `CX`, `DX`, `SP`, `BP`, `SI`, `DI`, `CS`, `DS`, `ES`, `SS`. Flags include zero, carry, sign, overflow, and direction.
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
