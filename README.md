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

Each scenario lives in its own module; [`src/main.cpp`](src/main.cpp) only selects and
calls one of them:

```
src/main.cpp            # scenario selection, setup() / loop()
src/nullptr.cpp/.h      # nullptr_issue::triggerNullPointerIssue()
src/zero_division.cpp/.h# zero_division::triggerZeroDivisionIssue()
```

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

[`src/nullptr.cpp`](src/nullptr.cpp) — the three inner levels live in an anonymous
namespace, only the entry point is exported through `nullptr_issue`:

```cpp
namespace
{
  void __attribute__((noinline)) triggerNullPointerIssue_level3()
  {
    int *p = nullptr;
    *p = 42;               // <-- src/nullptr.cpp:9, crash here
  }
  // ... _level2() -> _level3(), _level1() -> _level2()
}

void __attribute__((noinline)) nullptr_issue::triggerNullPointerIssue()
{
  Serial.flush();
  triggerNullPointerIssue_level1();
}
```

Call chain: `loopTask` → `setup()` → `nullptr_issue::triggerNullPointerIssue()` →
`_level1()` → `_level2()` → `_level3()`.

### 4.2 Serial Monitor log

```
Hello! I am useless program, that will crash. Pick a scenario via CRASH_SCENARIO to read that trace, and know something about debugging.
Scenario: null pointer write (StoreProhibited)
Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x420218c7  PS      : 0x00060830  A0      : 0x8200172e  A1      : 0x3fcebba0
  #0  0x420218c7 in (anonymous namespace)::triggerNullPointerIssue_level3() at src/nullptr.cpp:9

A2      : 0x3fc91558  A3      : 0x00000001  A4      : 0xffffffff  A5      : 0x0000ff00
  #0  0x3fc91558 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/esp32-hal-uart.c:73

A6      : 0x00ff0000  A7      : 0xff000000  A8      : 0x00000000  A9      : 0x0000002a
A10     : 0x00000001  A11     : 0x00000000  A12     : 0x00000000  A13     : 0x00000000
A14     : 0x3fcebf84  A15     : 0x00000000  SAR     : 0x0000001d  EXCCAUSE: 0x0000001d
EXCVADDR: 0x00000000  LBEG    : 0x400556d5  LEND    : 0x400556e5  LCOUNT  : 0xffffffff


Backtrace: 0x420218c4:0x3fcebba0 0x4200172b:0x3fcebbc0 0x42001733:0x3fcebbe0 0x42001741:0x3fcebc00 0x42001723:0x3fcebc20 0x42003456:0x3fcebc50
  #0  0x420218c4 in (anonymous namespace)::triggerNullPointerIssue_level3() at src/nullptr.cpp:9
  #1  0x4200172b in (anonymous namespace)::triggerNullPointerIssue_level2() at src/nullptr.cpp:14
  #2  0x42001733 in (anonymous namespace)::triggerNullPointerIssue_level1() at src/nullptr.cpp:19
  #3  0x42001741 in nullptr_issue::triggerNullPointerIssue() at src/nullptr.cpp:27
  #4  0x42001723 in setup() at src/main.cpp:20
  #5  0x42003456 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:42


ELF file SHA256: cba34904776c0af2

Rebooting...
```

### 4.3 Decoded backtrace

```
Backtrace: 0x420218c4:0x3fcebba0 0x4200172b:0x3fcebbc0 0x42001733:0x3fcebbe0 0x42001741:0x3fcebc00 0x42001723:0x3fcebc20 0x42003456:0x3fcebc50
  #0  0x420218c4 in (anonymous namespace)::triggerNullPointerIssue_level3() at src/nullptr.cpp:9
  #1  0x4200172b in (anonymous namespace)::triggerNullPointerIssue_level2() at src/nullptr.cpp:14
  #2  0x42001733 in (anonymous namespace)::triggerNullPointerIssue_level1() at src/nullptr.cpp:19
  #3  0x42001741 in nullptr_issue::triggerNullPointerIssue() at src/nullptr.cpp:27
  #4  0x42001723 in setup() at src/main.cpp:20
  #5  0x42003456 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:42
```

### 4.4 Analysis

| Question                      | Answer                           |
| ----------------------------- | -------------------------------- |
| Cause of the error            | `StoreProhibited`                |
| `EXCCAUSE`                    | `0x0000001d`                     |
| `EXCVADDR` (faulting address) | `0x00000000`                     |
| File                          | `src/nullptr.cpp`                |
| Line number                   | `9`                              |
| Function                      | `(anonymous namespace)::triggerNullPointerIssue_level3` |

**Register interpretation**

| Register   | Value      | Address (window mask applied) | Meaning                                                         |
| ---------- | ---------- | ----------------------------- | --------------------------------------------------------------- |
| `PC`       | 0x420218c7 | —                             | Address of the instruction that faulted; maps to the crash line |
| `EXCCAUSE` | 0x0000001d | —                             | Exception code — identifies the type of fault                   |
| `EXCVADDR` | 0x00000000 | —                             | The memory address the code tried to access                     |
| `A1`       | 0x3fcebba0 | —                             | Stack pointer; matches the first backtrace frame                |
| `A0`       | 0x8200172e | 0x4200172e                    | Return address — where execution would have continued           |
| `A8`       | 0x00000000 | —                             | Destination pointer `p` (`nullptr`); matches `EXCVADDR`         |
| `A9`       | 0x0000002a | —                             | Value being stored: 0x2a = 42                                   |

**Explanation**

- The `.elf` is the artifact that makes a panic readable. Without it the backtrace is a list of hex addresses; with it, every frame resolves to `file:line`. It must be archived for every shipped build, and it must match the flashed binary — the `ELF file SHA256` line exists to verify that.
- The register dump and the backtrace answer different questions. `EXCCAUSE` and `EXCVADDR` identify the fault (a write to address `0x0`), `A8` and `A9` show the operands involved (`nullptr` and `42`), and the backtrace shows the path that got there. Neither is sufficient alone.

---

## 5. Scenario 2 — Integer division by zero (`IntegerDivideByZero`)

### 5.1 Code

[`src/zero_division.cpp`](src/zero_division.cpp) — state lives in an anonymous
namespace, the trigger is exported through `zero_division`:

```cpp
namespace
{
  const int TRIGGER_DIVISION_BY_ZERO_AT = 10;
  const int DIVISION_ITERATION_DELAY = 500;
  volatile int divisor = 0;
  int divisionIteration = 0;
}

void __attribute__((noinline)) zero_division::triggerZeroDivisionIssue()
{
  if (divisionIteration >= TRIGGER_DIVISION_BY_ZERO_AT)
  {
    Serial.flush();
    Serial.println(divisionIteration / divisor);   // <-- src/zero_division.cpp:18
  }
  delay(DIVISION_ITERATION_DELAY);
  divisionIteration++;
}
```

The board runs normally for ~5 seconds (10 iterations × 500 ms) and only then faults,
which shows that the panic is produced by running code rather than by a boot failure.

### 5.2 Serial Monitor log

```
Guru Meditation Error: Core  1 panic'ed (IntegerDivideByZero). Exception was unhandled.

Core  1 register dump:
PC      : 0x42001770  PS      : 0x00060830  A0      : 0x82001736  A1      : 0x3fcebc10
  #0  0x42001770 in zero_division::triggerZeroDivisionIssue() at src/zero_division.cpp:18

A2      : 0x3fc949c0  A3      : 0x00000038  A4      : 0x00000078  A5      : 0x0000e100
  #0  0x3fc949c0 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/HardwareSerial.cpp:39

A6      : 0x0000002b  A7      : 0x3fc91558  A8      : 0x00000000  A9      : 0x3fc94920
A10     : 0x3fc949c0  A11     : 0x0000000a  A12     : 0x0000000a  A13     : 0x00000000
  #0  0x3fc949c0 in ?? at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/HardwareSerial.cpp:39

A14     : 0x3fcf19e0  A15     : 0x00000000  SAR     : 0x0000001d  EXCCAUSE: 0x00000006
EXCVADDR: 0x00000000  LBEG    : 0x400556d5  LEND    : 0x400556e5  LCOUNT  : 0xffffffff


Backtrace: 0x4200176d:0x3fcebc10 0x42001733:0x3fcebc30 0x42003549:0x3fcebc50
  #0  0x4200176d in zero_division::triggerZeroDivisionIssue() at src/zero_division.cpp:18
  #1  0x42001733 in loop() at src/main.cpp:31
  #2  0x42003549 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50


ELF file SHA256: 8de75e44d8199328

Rebooting...
```

### 5.3 Decoded backtrace

```
Backtrace: 0x4200176d:0x3fcebc10 0x42001733:0x3fcebc30 0x42003549:0x3fcebc50
  #0  0x4200176d in zero_division::triggerZeroDivisionIssue() at src/zero_division.cpp:18
  #1  0x42001733 in loop() at src/main.cpp:31
  #2  0x42003549 in loopTask(void*) at /Users/insomnia.exe/.platformio/packages/framework-arduinoespressif32/cores/esp32/main.cpp:50
```

### 5.4 Analysis

| Question           | Answer                                     |
| ------------------ | ------------------------------------------ |
| Cause of the error | `IntegerDivideByZero`                      |
| `EXCCAUSE`         | `0x00000006`                               |
| File               | `src/zero_division.cpp`                    |
| Line number        | `18`                                       |
| Function           | `zero_division::triggerZeroDivisionIssue`  |

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

### 6.1 Partition list (`esp_partition` API)

Enumerated with `esp_partition_find()` / `esp_partition_get()`; the running partition is
resolved with `esp_ota_get_running_partition()`.

```
[INFO]: Searching for partitions...
Label            Type  SubType     Address           Size
------------------------------------------------------------
nvs              data  nvs         0x00009000     20 KB
otadata          data  ota         0x0000e000      8 KB
app0             app   ota_0       0x00010000   3264 KB  <-- running
app1             app   ota_1       0x00340000   3264 KB
spiffs           data  spiffs      0x00670000   1536 KB
coredump         data  coredump    0x007f0000     64 KB
------------------------------------------------------------
```

### 6.2 Raw partition table read (`esp_flash_read` at 0x8000)

Read straight from flash with `esp_flash_read()`, one 32-byte slot at a time, parsed by hand
into `RawPartitionEntry` (magic, type, subtype, offset, size, label, flags). Parsing stops at
the MD5 entry (magic `0xEBEB`).

```
Label            Type  SubType     Address           Size
------------------------------------------------------------
nvs              data  nvs         0x00009000     20 KB
otadata          data  ota         0x0000e000      8 KB
app0             app   ota_0       0x00010000   3264 KB
app1             app   ota_1       0x00340000   3264 KB
spiffs           data  spiffs      0x00670000   1536 KB
coredump         data  coredump    0x007f0000     64 KB
------------------------------------------------------------
```

The output is identical to 6.1 except for the `<-- running` marker: that information does not
exist in the raw bytes, it comes from `esp_ota_get_running_partition()`.
