# Smart Home Control System — FreeRTOS on ATmega2560

A layered, RTOS-based embedded firmware for a smart home controller, built from scratch on the ATmega2560. The project demonstrates clean firmware architecture (MCAL → HAL → APP → RTOS), a hierarchical state machine, and the full set of FreeRTOS synchronization primitives — tasks, queues, mutexes, and semaphores.

> Simulated on Proteus. Written in Microchip Studio (AVR-GCC).

---

## Highlights

- **Four-layer architecture** with a strict dependency direction and a single central configuration file.
- **Business logic fully decoupled from the RTOS** — the entire decision-making lives in a pure, testable `APP` layer that calls no FreeRTOS function and touches no register.
- **Hierarchical state machine**: three top-level modes (Auto / Manual / Security) with a nested security sub-machine (locked → entering → unlocked, plus recovery and intruder lockout).
- **Full FreeRTOS toolkit**: 7 tasks, an event queue, a log queue, four mutexes, and an ISR-driven binary semaphore.
- **Reliability features**: a watchdog fed by a heartbeat task (auto-reset on hang), persisted settings in EEPROM, and stack-overflow / malloc-failure hooks.
- **Real fault detection**: distinguishes a genuinely hot room from a disconnected sensor by flagging physically impossible readings.
- **18 hand-written drivers**, each with a `config` struct, a `DEFAULT_CONFIG`, public APIs in the header, and line-by-line comments.

---

## Architecture

The firmware is organized into four layers. Each layer only depends on the layer beneath it, and everything is parameterized from a single `SystemConfig.h`.

![Architecture](docs/diagram1_architecture.png)

| Layer | Responsibility |
|-------|----------------|
| **RTOS** | FreeRTOS tasks, queues, mutexes, semaphores. Scheduling only. |
| **APP** | Pure business logic: state machine, control rules, security, faults. No RTOS, no registers. |
| **HAL** | Drivers for external devices (LCD, keypad, sensors, relay, motor, SD, …). |
| **MCAL** | On-chip peripheral drivers (DIO, ADC, UART, SPI, timers, ext-int, ICU, EEPROM, WDT). |

The key design decision: **tasks call APP functions**. A task reads an input, packages it as an event, asks the APP state machine to process it, and then drives the actuators from the resulting state. This keeps all product logic in one place that can be reasoned about and tested independently of the kernel.

---

## System Behaviour

### Modes

![State machine](docs/diagram2_statemachine.png)

**Auto mode** — the system runs itself:
- Light: lamp turns on when the LDR reads below the brightness threshold.
- Fan: off below 25 °C, 50 % from 25–35 °C, 100 % above 35 °C.
- Over 45 °C: buzzer alarm and an `OVER TEMP` warning.

**Manual mode** — direct control from the keypad:
- `1` Fan on · `2` Fan off · `3` Light on · `4` Light off · `5` enter Security.

**Security mode** — password-protected access:
- Prompts for a 4-digit password.
- Three wrong attempts trigger an `INTRUDER ALERT` with a 60-second lockout shown as a live countdown.
- A "forgot password" button starts a recovery flow (secret question); a correct answer lets the user set a new password.
- The lockout state is persisted in EEPROM, so cutting power does not bypass it.

### Logging

Every significant event is timestamped (uptime clock) and written to both the SD card and the UART terminal, e.g.:

```
00:00:12  TEMP HIGH
00:00:15  DOOR OPEN
00:00:18  ALARM ON
00:00:25  USER LOGIN
```

Anything shown on the LCD is mirrored to the terminal. From the keypad the user can request the latest event on the LCD or dump the full log to the terminal. Faults are shown on the LCD and the terminal on demand.

---

## RTOS Design

![RTOS flow](docs/diagram3_rtosflow.png)

| Task | Priority | Role |
|------|----------|------|
| Control | 3 | Runs the state machine, drives actuators |
| Door | 3 | Waits on the door ISR semaphore |
| Sensor | 2 | Reads LM35 + LDR once per second |
| Keypad | 2 | Debounced keypad scan |
| Display | 1 | Renders state to LCD + UART |
| Logger | 1 | Drains the log queue to SD + UART |
| Heartbeat | 1 | Blinks the LED and kicks the watchdog |

Synchronization:
- `eventQueue` carries keypad / sensor / ISR events to the control logic.
- `logQueue` carries log lines to the logger task.
- `lcdMutex`, `spiMutex`, `uartMutex` protect shared peripherals from concurrent access.
- `stateMutex` guards the single `SystemState` struct — no task ever touches it directly.
- `doorSem` is a binary semaphore given by the door ISR to wake the door task.

---

## Hardware

- **MCU**: ATmega2560 @ 16 MHz
- **Display**: 20×4 character LCD (HD44780, 4-bit)
- **Input**: 4×4 matrix keypad, push buttons (door reed switch, recovery)
- **Sensors**: LM35 (temperature), LDR (light) via ADC
- **Actuators**: relay (light), DC motor + H-bridge (fan), buzzer, status LED
- **Storage**: microSD card over SPI; internal EEPROM for settings
- **Comms**: UART terminal for logging and faults

Pin assignments live in `CONFIG/SystemConfig.h`. To re-wire the board, edit only that file.

> Timer allocation: FreeRTOS uses Timer1 for the system tick, the ICU driver uses Timer5, and the fan PWM uses Timer0 — chosen so nothing conflicts.

---

## Repository Layout

```
SmartHome/
├── CONFIG/
│   ├── SystemConfig.h        # all pins, thresholds, modes, EEPROM map
│   └── FreeRTOSConfig.h      # FreeRTOS tuned for ATmega2560
├── MCAL/                     # DIO, ADC, UART, SPI, TIMER, EXT_INT, ICU, EEPROM, WDT
├── HAL/                      # LCD, KEYPAD, LM35, LDR, LED, BUTTON, BUZZER, RELAY, MOTOR, SD_CARD
├── APP/                      # App_Types, App_SM, App_Control, App_Security, App_Fault, main.c
├── RTOS/                     # RTOS_Shared, RTOS_Tasks
└── docs/                     # architecture / state-machine / RTOS-flow diagrams
```

---

## Build & Run

### Microchip Studio

1. Create a GCC C Executable Project for **ATmega2560**.
2. Add the FreeRTOS source (`tasks.c`, `queue.c`, `list.c`, the AVR port, `heap_4.c`) and all project files.
3. Add include paths for `CONFIG/`, `MCAL/*`, `HAL/*`, `APP/`, `RTOS/`, and the FreeRTOS `include` folder.
4. Define the symbol `F_CPU=16000000UL`.
5. Build to produce the `.hex`.

### Proteus

1. Place an ATMEGA2560 and load the `.hex` into it.
2. Set the clock frequency to 16 MHz.
3. Wire the devices per `SystemConfig.h` (see the pin map there).
4. Run the simulation.

---

## What This Project Demonstrates

- Layered embedded architecture with clean separation of concerns.
- Decoupling business logic from an RTOS so the logic is portable and testable.
- Practical use of FreeRTOS tasks, queues, mutexes, and ISR-to-task semaphores.
- A hierarchical state machine driving a real device set.
- Defensive firmware: watchdog, persisted security state, fault detection, and diagnostic hooks.

---

## License

MIT — free to use and adapt.
