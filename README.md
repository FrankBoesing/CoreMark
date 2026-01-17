# CoreMark - CPU Performance Benchmark

Measures the number for times per second your processor can perform a
variety of common tasks: linked list management, matrix multiply, and
executing state machines.

* RP2350 (SMP) kooperative Scheduling
* FreeRTOS preemtpive Scheduling.

The RP2350 runs bare-metal tasks on each core with no scheduler, maximizing per-core throughput for benchmarks like CoreMark.

ESP32 with FreeRTOS uses preemptive multitasking, adding context-switch overhead that slightly lowers peak performance but improves responsiveness for concurrent tasks.


| Board                  | CoreMark | GCC switches | Cores used |
| ---------------------- | :------: | :------: | :------: |
| Teensy 4.0             | 2313.57  | n/a | 1 |
| RP2350 Dual Core (276MHz overclock, -O3) | 1437.00  | n/a | 2 |
| RP2350 Dual Core (200MHz overclock, -O3) | 1041.00  | n/a | 2 |
| ESP32 WROOM 32 xtensa 240MHz | 1032.62 | -O3 -fjump-tables -ftree-switch-conversion | 2 |
| RP2350 Dual Core (150MHz) | 600.00   |  n/a  | 2 |
| Adafruit Metro M4 (200MHz overclock, 'dragons' optimization) | 536.35   |  n/a |  1 |
| ESP32 WROOM 32 xtensa 240MHz | 519.75   | -O3 -fjump-tables -ftree-switch-conversion | 1 |
| Adafruit Metro M4 (180MHz overclock, faster optimizations) | 458.19   |  n/a | 1 |
| Teensy 3.6             | 440.72   |  n/a | 1 |
| ESP32-C3 160MHz        | 409.72   | -O3 -fjump-tables -ftree-switch-conversion | 1 |
| Sparkfun ESP32 Thing   | 351.33   |  n/a | 1 |
| Adafruit HUZZAH 32     | 351.35   |  n/a | 1 |
| Teensy 3.5             | 265.50   |  n/a | 1 |
| Teensy 3.2 (96MHz overclock, faster optimizations)            | 218.26   |  n/a | 1 |
| Adafruit Metro M4 (120MHz, smaller code) | 214.85   |  n/a | 1 |
| Teensy 3.2 (72MHz)            | 168.62   |  n/a | 1 |
| Teensy 3.2 (72MHz, smaller code)            | 126.76   |  n/a | 1 |
| Arduino Due            | 94.95    |  n/a | 1 |
| Arduino Zero           | 56.86    | n/a  | 1 |
| Arduino Nano Every     | 8.20     | n/a  | 1 |
| Arduino Mega           | 7.03     | n/a  | 1 |

(larger numbers are better)

