# STM32 Smart Device Controller (Blue Pill, FreeRTOS, Renode-Simulated)

A bare-metal-style embedded firmware project for the STM32F103C8T6 ("Blue Pill"),
built entirely without physical hardware and validated end-to-end using the
[Renode](https://renode.io) hardware simulation framework.

The firmware implements a simple smart device controller: it simulates a sensor
reading on a periodic hardware timer interrupt, runs an automatic threshold-based
control loop driving simulated actuator outputs, and exposes a UART command
interface for manual override and status reporting — all running on FreeRTOS
with two cooperating tasks.

## Why simulation?

I don't currently own a physical Blue Pill board. Rather than wait on hardware,
I built and validated this firmware entirely in [Renode](https://renode.io),
an open-source hardware simulation framework used in real embedded engineering
teams for hardware-in-the-loop-free firmware testing and CI. The `.elf` built
here is standard STM32 HAL/CMSIS code — it would run unmodified on real Blue
Pill hardware.

## Architecture

- **MCU target:** STM32F103C8T6 (64KB flash / 20KB RAM, Cortex-M3 @ 8MHz)
- **RTOS:** FreeRTOS (CMSIS_V2 interface), two tasks + one queue
  - `SensorTask` — woken by a 1Hz TIM2 hardware interrupt; updates a simulated
    temperature reading, runs the AUTO-mode control logic (fan/heater GPIO
    outputs, critical-temp alarm LED), and pushes a status snapshot into
    `sensorQueue`
  - `UartTask` — woken by USART1 RX interrupts; parses line-based commands and
    responds over UART; reads the latest snapshot from `sensorQueue` to answer
    `STATUS`
- **Peripherals used:** GPIO (output actuators + onboard LED), USART1
  (interrupt-driven RX/TX), TIM2 (periodic interrupt)

### Command interface (115200 8N1 over USART1)

| Command | Effect |
|---|---|
| `STATUS` | Reports current temperature, mode, and actuator states |
| `MODE AUTO` | Switches to automatic threshold-based control |
| `MODE MANUAL` | Switches to manual control (required before `SET PAx`) |
| `SET THRESHOLD n` | Sets the fan-trigger temperature threshold |
| `SET PA0 ON`/`OFF` | Manually drives the fan output (manual mode only) |
| `SET PA1 ON`/`OFF` | Manually drives the heater output (manual mode only) |
| `SET PA2 ON`/`OFF` | Manually drives the auxiliary alarm output (manual mode only) |

## Repository layout

```
firmware/     STM32CubeIDE project (Core/, Drivers/, Middlewares/, .ioc file)
sim/          Renode simulation script (simulate.resc)
```

## Building

1. Open `firmware/` as an existing project in STM32CubeIDE
2. Build (Project → Build Project) — produces `firmware/Debug/STM32_Smart_Device_Controller_C.elf`

## Running in simulation (no hardware required)

1. Install [Renode](https://renode.io)
2. Edit the `$bin` path in `sim/simulate.resc` to point at your built `.elf`
3. From the `sim/` folder: `renode simulate.resc`
4. Connect a raw TCP terminal (e.g. [PuTTY](https://putty.org) in **Raw** mode,
   not Telnet) to `localhost:3456` to interact with the UART command interface

## Known limitations / simulation notes

- **HSE/PLL clock path is not usable in Renode**: Renode's STM32F103 platform
  model does not implement the RCC peripheral's oscillator-ready behavior, which
  causes the standard 72MHz HSE+PLL clock configuration to hang indefinitely
  waiting on a status flag that never legitimately clears. This firmware runs
  on the internal 8MHz HSI oscillator instead (`PLLState = RCC_PLL_NONE`) —
  functionally correct for this project, but a difference from how a
  performance-oriented real deployment would typically be clocked.
- **UART command parsing uses a single shared line buffer** rather than a
  proper per-line queue. Rapid back-to-back command bursts (e.g. pasting
  multiple commands at once) can occasionally cause a command to be dropped
  or overwritten before `UartTask` processes it. Commands sent with normal
  typing/one-at-a-time timing are unaffected. A follow-up improvement would
  replace the shared buffer with an `osMessageQueue` of complete lines.
- **HAL timebase uses SysTick** (rather than a dedicated timer) despite running
  FreeRTOS, which normally also wants SysTick for its own scheduler tick. This
  is the standard, most common STM32+FreeRTOS configuration and works safely
  here since CubeMX's generated `SysTick_Handler` correctly services both
  HAL and the RTOS tick — a dedicated-timer alternative was tested but found
  to be unreliable under Renode's Blue Pill timer model.

## Next steps

- Replace the shared UART line buffer with a proper FreeRTOS queue of
  complete command lines
- Add a hardware abstraction seam so the same application logic could
  target real HSE/PLL clocking on physical hardware while still simulating
  cleanly on HSI in Renode
