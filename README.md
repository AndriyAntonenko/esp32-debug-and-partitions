# ESP32-S3 Crash Debugging

Homework project: deliberately crash the firmware, capture the panic output, decode the
backtrace, and identify the exact line of code that caused the fault.

The point is not the crash itself — it is the workflow used in production to answer
_why_ a board rebooted, using the `.elf` file that holds the symbols and addresses
needed to decode a panic.

---

## 1. Environment

| Item                | Value                                         |
| ------------------- | --------------------------------------------- |
| Board               | ESP32-S3-DevKitC-1 (N8, 8 MB flash, no PSRAM) |
| MCU                 | ESP32-S3, Xtensa LX7, 240 MHz, 320 KB RAM     |
| Framework           | Arduino                                       |
| PlatformIO platform | `espressif32` 7.0.1                           |
| Arduino core        | `framework-arduinoespressif32` 3.20017        |
| Toolchain           | `toolchain-xtensa-esp32s3` 8.4.0              |

### `platformio.ini`

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
build_type = debug
```

Two settings matter for this task:

- **`monitor_filters = esp32_exception_decoder`** — PlatformIO pipes the panic output
  through `addr2line` against the build's `.elf`, so the raw backtrace addresses are
  printed as `function() at file:line`.
- **`build_type = debug`** — builds with `-Og -g`, which keeps line numbers accurate and
  prevents the compiler from optimising the call chain away.

---

## 2. Crash scenarios

The scenario is selected at compile time with the `CRASH_SCENARIO` macro in
[`src/main.cpp`](src/main.cpp):

```cpp
#define CRASH_NONE           0
#define CRASH_NULL_POINTER   1
#define CRASH_DIVIDE_BY_ZERO 2

#define CRASH_SCENARIO CRASH_NULL_POINTER
```

| Scenario                 | Macro value            | Expected exception    | `EXCCAUSE`  |
| ------------------------ | ---------------------- | --------------------- | ----------- |
| Null pointer write       | `CRASH_NULL_POINTER`   | `StoreProhibited`     | `0x1d` (29) |
| Integer division by zero | `CRASH_DIVIDE_BY_ZERO` | `IntegerDivideByZero` | `0x06` (6)  |
| No crash                 | `CRASH_NONE`           | —                     | —           |

### Implementation notes

- Every crash function is marked `__attribute__((noinline))`. Without it the compiler is
  free to inline the `level1 → level2 → level3` chain into `setup()`, which collapses the
  stack frames and destroys the backtrace — exactly the information this task is about.
- `Serial.flush()` is called before the fault so the buffered UART output is actually
  transmitted before the CPU panics.
- The divisor in the division scenario is `volatile int`. With a literal `0` the
  compiler warns (`-Wdiv-by-zero`) and is free to fold the undefined operation into
  something other than a real division instruction. `volatile` forces the CPU to execute
  the `quos` instruction, guaranteeing the hardware exception.

---

## 3. Build, flash and monitor

```bash
pio run                 # build
pio run -t upload       # flash
pio device monitor      # open serial monitor with the exception decoder
```

Or in one step:

```bash
pio run -t upload -t monitor
```

---

## 4. Scenario 1 — Null pointer write (`StoreProhibited`)

### 4.1 Code

```cpp
void __attribute__((noinline)) triggerNullPointerIssue_level3()
{
  int *p = nullptr;
  *p = 42;                 // <-- crash here
}
```

Call chain: `loopTask` → `setup()` → `triggerNullPointerIssue()` →
`_level1()` → `_level2()` → `_level3()`.

### 4.2 Serial Monitor log

<!-- TODO: paste the full raw log here, from the boot header down to the reboot line -->

```
Hello! I am useless program, that will crash. Pick a scenario via CRASH_SCENARIO to read that trace, and know something about debugging.
Scenario: null pointer write (StoreProhibited)
Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x420218bf  PS      : 0x00060830  A0      : 0x820016f2  A1      : 0x3fcebba0
  #0  0x420218bf in triggerNullPointerIssue_level3() at src/main.cpp:13

A2      : 0x3fc91558  A3      : 0x00000001  A4      : 0xffffffff  A5      : 0x0000ff00
  #0  0x3fc91558 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c:73

A6      : 0x00ff0000  A7      : 0xff000000  A8      : 0x00000000  A9      : 0x0000002a
A10     : 0x00000001  A11     : 0x00000000  A12     : 0x00000000  A13     : 0x00000000
A14     : 0x3fcebf84  A15     : 0x00000000  SAR     : 0x0000001d  EXCCAUSE: 0x0000001d
EXCVADDR: 0x00000000  LBEG    : 0x400556d5  LEND    : 0x400556e5  LCOUNT  : 0xffffffff


Backtrace: 0x420218bc:0x3fcebba0 0x420016ef:0x3fcebbc0 0x420016f7:0x3fcebbe0 0x42001705:0x3fcebc00 0x42001743:0x3fcebc20 0x42003456:0x3fcebc50
  #0  0x420218bc in triggerNullPointerIssue_level3() at src/main.cpp:13
  #1  0x420016ef in triggerNullPointerIssue_level2() at src/main.cpp:18
  #2  0x420016f7 in triggerNullPointerIssue_level1() at src/main.cpp:23
  #3  0x42001705 in triggerNullPointerIssue() at src/main.cpp:29
  #4  0x42001743 in setup() at src/main.cpp:56
  #5  0x42003456 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:42


ELF file SHA256: 855530d5a10ead85

Rebooting...
```

### 4.3 Decoded backtrace

```
Backtrace: 0x420218bc:0x3fcebba0 0x420016ef:0x3fcebbc0 0x420016f7:0x3fcebbe0 0x42001705:0x3fcebc00 0x42001743:0x3fcebc20 0x42003456:0x3fcebc50
  #0  0x420218bc in triggerNullPointerIssue_level3() at src/main.cpp:13
  #1  0x420016ef in triggerNullPointerIssue_level2() at src/main.cpp:18
  #2  0x420016f7 in triggerNullPointerIssue_level1() at src/main.cpp:23
  #3  0x42001705 in triggerNullPointerIssue() at src/main.cpp:29
  #4  0x42001743 in setup() at src/main.cpp:56
  #5  0x42003456 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:42
```

### 4.4 Analysis

| Question                      | Answer                           |
| ----------------------------- | -------------------------------- |
| Cause of the error            | `StoreProhibited`                |
| `EXCCAUSE`                    | `0x0000001d`                     |
| `EXCVADDR` (faulting address) | `0x00000000`                     |
| File                          | `src/main.cpp`                   |
| Line number                   | `13`                             |
| Function                      | `triggerNullPointerIssue_level3` |

**Register interpretation**

| Register   | Value      | Address (window mask applied) | Meaning                                                         |
| ---------- | ---------- | ----------------------------- | --------------------------------------------------------------- |
| `PC`       | 0x420218bf | —                             | Address of the instruction that faulted; maps to the crash line |
| `EXCCAUSE` | 0x0000001d | —                             | Exception code — identifies the type of fault                   |
| `EXCVADDR` | 0x00000000 | —                             | The memory address the code tried to access                     |
| `A1`       | 0x3fcebba0 | —                             | Stack pointer; matches the first backtrace frame                |
| `A0`       | 0x820016f2 | 0x420016f2                    | Return address — where execution would have continued           |
| `A8`       | 0x00000000 | —                             | Destination pointer `p` (`nullptr`); matches `EXCVADDR`         |
| `A9`       | 0x0000002a | —                             | Value being stored: 0x2a = 42                                   |

**Explanation**

- The `.elf` is the artifact that makes a panic readable. Without it the backtrace is a list of hex addresses; with it, every frame resolves to `file:line`. It must be archived for every shipped build, and it must match the flashed binary — the `ELF file SHA256` line exists to verify that.
- The register dump and the backtrace answer different questions. `EXCCAUSE` and `EXCVADDR` identify the fault (a write to address `0x0`), `A8` and `A9` show the operands involved (`nullptr` and `42`), and the backtrace shows the path that got there. Neither is sufficient alone.

---

## 5. Scenario 2 — Integer division by zero (`IntegerDivideByZero`)

### 5.1 Code

```cpp
volatile int divisor = 0;
int divisionIteration = 0;

void __attribute__((noinline)) triggerZeroDivisionIssue()
{
  if (divisionIteration >= TRIGGER_DIVISION_BY_ZERO_AT)
  {
    Serial.flush();
    Serial.println(divisionIteration / divisor);   // <-- crash here
  }
  delay(DIVISION_ITERATION_DELAY);
  divisionIteration++;
}
```

The board runs normally for ~5 seconds (10 iterations × 500 ms) and only then faults,
which shows that the panic is produced by running code rather than by a boot failure.

### 5.2 Serial Monitor log

```
PC      : 0x42001768  PS      : 0x00060830  A0      : 0x8200177a  A1      : 0x3fcebc10
  #0  0x42001768 in triggerZeroDivisionIssue() at src/main.cpp:42

A2      : 0x3fc949c0  A3      : 0x00000038  A4      : 0x00000078  A5      : 0x0000e100
  #0  0x3fc949c0 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/HardwareSerial.cpp:39

A6      : 0x0000002b  A7      : 0x3fc91558  A8      : 0x00000000  A9      : 0x3fc94920
A10     : 0x3fc949c0  A11     : 0x0000000a  A12     : 0x0000000a  A13     : 0x00000000
  #0  0x3fc949c0 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/HardwareSerial.cpp:39

A14     : 0x3fcf19e0  A15     : 0x00000000  SAR     : 0x0000001d  EXCCAUSE: 0x00000006
EXCVADDR: 0x00000000  LBEG    : 0x400556d5  LEND    : 0x400556e5  LCOUNT  : 0xffffffff


Backtrace: 0x42001765:0x3fcebc10 0x42001777:0x3fcebc30 0x42003549:0x3fcebc50
  #0  0x42001765 in triggerZeroDivisionIssue() at src/main.cpp:42
  #1  0x42001777 in loop() at src/main.cpp:67
  #2  0x42003549 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50


ELF file SHA256: 0204372a1bf2aa38

Rebooting...
```

### 5.3 Decoded backtrace

```
Backtrace: 0x42001765:0x3fcebc10 0x42001777:0x3fcebc30 0x42003549:0x3fcebc50
  #0  0x42001765 in triggerZeroDivisionIssue() at src/main.cpp:42
  #1  0x42001777 in loop() at src/main.cpp:67
  #2  0x42003549 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50
```

### 5.4 Analysis

| Question           | Answer                     |
| ------------------ | -------------------------- |
| Cause of the error | `IntegerDivideByZero`      |
| `EXCCAUSE`         | `0x00000006`               |
| File               | `src/main.cpp`             |
| Line number        | `42`                       |
| Function           | `triggerZeroDivisionIssue` |

**Explanation**

- The divisor is `volatile`, so the compiler cannot fold the division away: the CPU
  executes a real `quos` instruction with a zero operand and raises `EXCCAUSE 0x6`.
  `EXCVADDR` is `0x00000000` here but carries no meaning — it is only populated for
  memory faults.
- The stack is three frames deep instead of six, because the fault happens directly in
  `loop()` rather than through a nested call chain. The crash only occurs after ~10
  iterations, which confirms it is a runtime fault and not a boot failure.

---

## 6. Bonus task — partition table

> Not done yet. This section covers reading the partition table / dumping the binary
> contents of a partition.

<!-- TODO -->
