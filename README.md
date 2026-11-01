[README.md](https://github.com/user-attachments/files/34238659/README.md)
# BMP180 on STM32F103C8 (Blue Pill) with Live Graph

Read temperature, pressure and altitude from a **BMP180** sensor over **I2C** using an **STM32F103C8 (Blue Pill)** and the **STM32 HAL**, print the values over **UART**, and plot them live on your PC with a small **Python** script.

```
BMP180 --I2C--> STM32F103C8 --UART--> USB-TTL --USB--> PC --> Python live graph
```

Example UART output (1 line per second):

```
T=24.3 C  P=87175 Pa  Alt=1250.91 m
```

---

## Table of contents

1. [Features](#features)
2. [What you need](#what-you-need)
3. [Repository structure](#repository-structure)
4. [Wiring](#wiring)
5. [Part 1: STM32 firmware from scratch](#part-1-stm32-firmware-from-scratch)
6. [Part 2: Check the data in a serial terminal](#part-2-check-the-data-in-a-serial-terminal)
7. [Part 3: Live graph on the PC](#part-3-live-graph-on-the-pc)
8. [Driver API](#driver-api)
9. [How the BMP180 works](#how-the-bmp180-works)
10. [Configuration](#configuration)
11. [Troubleshooting](#troubleshooting)
12. [License](#license)

---

## Features

- Small, dependency-free BMP180 driver (`bmp180.c` / `bmp180.h`) using only HAL I2C
- Chip-ID check and calibration read at start-up
- Temperature in 0.1 °C, pressure in Pa, altitude in metres
- Selectable oversampling (OSS 0 to 3)
- Integer-only UART output, so no need to enable float `printf` in newlib-nano
- Python script that plots temperature, pressure and altitude live

---

## What you need

**Hardware**

| Item | Notes |
|------|-------|
| STM32F103C8 Blue Pill | 8 MHz crystal, 72 MHz system clock |
| ST-Link V2 | For flashing and debugging over SWD |
| BMP180 module (GY-68) | I2C address `0x77`, use **3.3 V** |
| USB-TTL (UART) adapter | CH340 / CP2102 / FT232, 3.3 V logic |
| Jumper wires | |

**Software**

| Tool | Purpose |
|------|---------|
| STM32CubeIDE (CubeMX included) | Project generation, build, flash |
| Python 3.8 or newer | Live graph |
| `pyserial`, `matplotlib` | Python libraries |
| Serial terminal (ScriptCommunicator, PuTTY, etc.) | Optional, to check raw UART output |

---

## Repository structure

```
i2c_bmp180/
├── Core/
│   ├── Inc/
│   │   └── bmp180.h          # driver header
│   └── Src/
│       ├── bmp180.c          # driver implementation
│       └── main.c            # application: read sensor, print over UART
├── i2c_bmp180.ioc            # CubeMX configuration
├── live_bmp180.py            # live graph (Python)
└── README.md
```

---

## Wiring

### BMP180 to Blue Pill (I2C1)

| BMP180 (GY-68) | Blue Pill |
|----------------|-----------|
| VCC / VIN | 3.3 V |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |

The GY-68 module normally has 4.7 kΩ pull-up resistors on SCL and SDA. If you use a bare chip or a module without pull-ups, add 4.7 kΩ from SCL and SDA to 3.3 V.

### UART to USB-TTL adapter (USART1)

| Blue Pill | USB-TTL adapter |
|-----------|-----------------|
| PA9 (TX) | RX |
| PA10 (RX) | TX |
| GND | GND |

TX goes to RX and RX goes to TX. Do not connect the adapter's 5 V pin to the Blue Pill. Power the Blue Pill from the ST-Link 3.3 V or USB.

### ST-Link (SWD) to Blue Pill

| ST-Link | Blue Pill |
|---------|-----------|
| SWDIO | SWDIO (PA13) |
| SWCLK | SWCLK (PA14) |
| GND | GND |
| 3.3 V | 3.3 V |

### Overview

```
            +----------------+
 BMP180 --- | PB6 (SCL)      |
 (GY-68) -- | PB7 (SDA)      |
            |                |
            | PA9  (TX)  ----+--> USB-TTL RX
            | PA10 (RX)  <---+--- USB-TTL TX
            |                |
            | PC13 (LED)     |
            +----------------+
               STM32F103C8
```

---

## Part 1: STM32 firmware from scratch

### 1. Create the project

1. Open **STM32CubeIDE** and choose **File → New → STM32 Project**.
2. In the MCU selector, search for `STM32F103C8` and select **STM32F103C8Tx**.
3. Name the project (for example `i2c_bmp180`), choose **C**, **Executable**, then **Finish**.

### 2. Configure peripherals in the `.ioc` file

| Section | Setting |
|---------|---------|
| **System Core → SYS** | Debug: **Serial Wire** |
| **System Core → RCC** | High Speed Clock (HSE): **Crystal/Ceramic Resonator** |
| **Clock Configuration** | HSE 8 MHz, PLL source HSE, PLLMUL **x9**, SYSCLK **72 MHz**, APB1 prescaler **/2** (36 MHz) |
| **Connectivity → I2C1** | I2C: **I2C**, Speed Mode **Standard** (100 kHz). Pins PB6 (SCL) and PB7 (SDA) |
| **Connectivity → USART1** | Mode **Asynchronous**, baud rate **115200**, 8 bits, no parity, 1 stop bit. Pins PA9 (TX) and PA10 (RX) |
| **Pinout view** | Set **PC13** to **GPIO_Output** (on-board LED) |

Save the `.ioc` file and let CubeIDE generate the code.

### 3. Add the driver files

Copy the driver files into the project:

- `bmp180.h` into `Core/Inc/`
- `bmp180.c` into `Core/Src/`

### 4. Edit `main.c`

Add code only between the `USER CODE BEGIN` and `USER CODE END` markers, so CubeMX does not overwrite it when you regenerate.

**Includes**

```c
/* USER CODE BEGIN Includes */
#include "bmp180.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */
```

**Private variables**

```c
/* USER CODE BEGIN PV */
BMP180_t bmp;
/* USER CODE END PV */
```

**Before the main loop**

```c
/* USER CODE BEGIN 2 */
char msg[80];

if (BMP180_Init(&bmp, &hi2c1, BMP180_OSS_3) != HAL_OK) {
    const char *err = "BMP180 not found! Check wiring and pull-ups.\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)err, strlen(err), 100);
}
/* USER CODE END 2 */
```

**Inside the main loop**

```c
while (1)
{
  /* USER CODE BEGIN 3 */
  int32_t t10, p;

  if (BMP180_ReadTemperature(&bmp, &t10) == HAL_OK &&
      BMP180_ReadPressure(&bmp, &p)      == HAL_OK)
  {
      int32_t alt_cm = (int32_t)(BMP180_Altitude(p, 101325.0f) * 100.0f);

      int n = snprintf(msg, sizeof(msg),
               "T=%ld.%ld C  P=%ld Pa  Alt=%ld.%02ld m\r\n",
               t10 / 10, (t10 < 0 ? -t10 : t10) % 10,
               p,
               alt_cm / 100, (alt_cm < 0 ? -alt_cm : alt_cm) % 100);
      HAL_UART_Transmit(&huart1, (uint8_t *)msg, n, 100);
  }
  else
  {
      const char *e = "Read error\r\n";
      HAL_UART_Transmit(&huart1, (uint8_t *)e, strlen(e), 100);
  }

  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  HAL_Delay(1000);
  /* USER CODE END 3 */
}
```

Do not add your own `while (1) { ... }` inside `USER CODE BEGIN 3`. CubeMX already generates the loop braces.

### 5. Build and flash

1. **Project → Build Project** (Ctrl+B).
2. Connect the ST-Link and click **Run** (or **Debug**). The LED on PC13 starts blinking once per second.

---

## Part 2: Check the data in a serial terminal

Before using the graph, confirm the UART output is correct.

1. Connect the USB-TTL adapter to the PC and find its COM port (Windows: **Device Manager → Ports (COM & LPT)**).
2. Open a serial terminal (ScriptCommunicator, PuTTY, etc.) with **115200 baud, 8N1**.
3. You should see one line per second:

```
T=24.4 C  P=87184 Pa  Alt=1250.07 m
T=24.4 C  P=87175 Pa  Alt=1250.91 m
T=24.3 C  P=87175 Pa  Alt=1250.91 m
```

4. **Disconnect and close the terminal** before the next part. Only one program can use a COM port at a time.

---

## Part 3: Live graph on the PC

### 1. Install Python and the libraries

Check Python:

```
python --version
```

If it is not installed, get it from [python.org](https://www.python.org/downloads/) and tick **Add Python to PATH** during installation.

Install the libraries:

```
python -m pip install pyserial matplotlib
```

Use `python -m pip` so the libraries go to the same Python that runs the script.

### 2. Set the COM port

Open `live_bmp180.py` and edit the port near the top:

```python
PORT = "COM14"       # your USB-TTL port
BAUD = 115200
WINDOW = 120         # number of latest samples shown
```

On Linux the port looks like `/dev/ttyUSB0`, on macOS like `/dev/tty.usbserial-XXXX`.

### 3. Run it

Go to the project folder and run the script:

```
cd /d "C:\path\to\i2c_bmp180"
python live_bmp180.py
```

A window opens with three live graphs: temperature (°C), pressure (Pa) and altitude (m). The title shows the latest values. Close the window to stop.

> Make sure the file is named `live_bmp180.py` and not `live_bmp180.py.txt`. In Windows File Explorer, enable **View → Show → File name extensions** to check.

The script expects exactly this line format from the firmware:

```
T=<temp> C  P=<pressure> Pa  Alt=<altitude> m
```

If you change the `snprintf` format in `main.c`, update the regular expression `LINE_RE` in the script to match.

---

## Driver API

```c
HAL_StatusTypeDef BMP180_Init(BMP180_t *dev, I2C_HandleTypeDef *hi2c, BMP180_OSS oss);
HAL_StatusTypeDef BMP180_ReadTemperature(BMP180_t *dev, int32_t *temp_x10);
HAL_StatusTypeDef BMP180_ReadPressure(BMP180_t *dev, int32_t *press_pa);
float             BMP180_Altitude(int32_t press_pa, float sea_level_pa);
```

| Function | Description |
|----------|-------------|
| `BMP180_Init` | Checks the chip ID (`0x55`), reads the 22 calibration bytes, stores the oversampling setting. Returns `HAL_OK` on success. |
| `BMP180_ReadTemperature` | Starts a temperature conversion and returns the result in **0.1 °C** (`243` = 24.3 °C). |
| `BMP180_ReadPressure` | Reads the temperature first (needed for compensation), then the pressure. Result in **Pa** (`87175` Pa = 871.75 hPa). |
| `BMP180_Altitude` | Computes altitude in metres from pressure and a sea-level reference pressure. |

---

## How the BMP180 works

- **Interface:** I2C. 7-bit address `0x77` (HAL needs the 8-bit form `0xEE`, which the driver handles).
- **Chip ID:** register `0xD0` returns `0x55`.
- **Calibration:** 11 factory coefficients (AC1 to AC6, B1, B2, MB, MC, MD) are stored in registers `0xAA` to `0xBF`. They are read once at start-up.
- **Measurement flow:**
  1. Write `0x2E` to register `0xF4` (start temperature), wait about 4.5 ms, read the raw value `UT` from `0xF6`/`0xF7`.
  2. Write `0x34 + (OSS << 6)` to `0xF4` (start pressure), wait for the conversion, read the raw value `UP` from `0xF6` to `0xF8`.
  3. Apply the compensation algorithm from the Bosch datasheet to get temperature and pressure.
- **Altitude:**

  ```
  altitude = 44330 * (1 - (p / p0) ^ (1 / 5.255))
  ```

  where `p` is the measured pressure and `p0` is the sea-level pressure (default 101325 Pa).

---

## Configuration

### Oversampling (OSS)

| `BMP180_OSS_x` | Pressure conversion time | Noise | Use case |
|----------------|--------------------------|-------|----------|
| `BMP180_OSS_0` | about 4.5 ms | highest | Fast, low power |
| `BMP180_OSS_1` | about 7.5 ms | | |
| `BMP180_OSS_2` | about 13.5 ms | | |
| `BMP180_OSS_3` | about 25.5 ms | lowest | Best resolution (default in this project) |

### Sea-level pressure

The altitude depends on the reference pressure. The default `101325.0f` Pa is the standard atmosphere. For a more accurate absolute altitude, pass the current local sea-level pressure (from a weather service):

```c
float alt = BMP180_Altitude(p, 100800.0f);   /* example value */
```

For relative altitude (height change from a start point), the reference value matters less.

---

## Troubleshooting

| Problem | Likely cause and fix |
|---------|----------------------|
| `BMP180 not found!` | Check SCL/SDA wiring (PB6 / PB7), 3.3 V power, common GND, and 4.7 kΩ pull-ups. Confirm I2C1 is enabled in the `.ioc` file. |
| `Read error` printed | I2C connection dropped or the bus is stuck. Re-check wiring and power cycle the board. |
| LED on PC13 does not blink | PC13 is not set to `GPIO_Output` in the `.ioc`, or the GPIOC clock is not enabled. Set it in CubeMX and regenerate. |
| Build error: `expected declaration or statement at end of input` | A `}` is missing in `main.c`. Check the end of `main()` and `Error_Handler()`. Do not add an extra `while (1) { }` inside `USER CODE BEGIN 3`. |
| Undefined reference to `powf` | Link the math library: **Project → Properties → C/C++ Build → Settings → MCU GCC Linker → Libraries**, add `m`. |
| `Could not verify ST device` when debugging | Common with clone Blue Pill chips. Use the OpenOCD debug configuration instead of ST-LINK GDB server. |
| No text in the serial terminal | Check baud 115200, PA9 to adapter RX, PA10 to adapter TX, common GND. |
| Python: `ModuleNotFoundError: No module named 'serial'` | Run `python -m pip install pyserial matplotlib`. If it still fails, run `pip uninstall serial` (a different package with a clashing name), then install `pyserial` again. |
| Python: `Port COM14 khulena` / `PermissionError` | Another program (terminal, ScriptCommunicator, CubeIDE console) is using the port. Close it. |
| Python: `The directory name is invalid` | `cd` takes a folder, not a file. Use `cd /d "C:\path\to\folder"`, then run `python live_bmp180.py`. |
| Graph window opens but stays empty | No valid lines received. Check the port number, baud rate, and that the line format matches `LINE_RE`. |
| Altitude jumps by about 1 m | Normal. About 12 Pa of pressure change equals about 1 m. Use `BMP180_OSS_3` and average several samples if you need a smoother value. |
| Altitude is wrong by hundreds of metres | The sea-level reference is off. Pass the local sea-level pressure to `BMP180_Altitude`. |

---

## License

Add a `LICENSE` file to the repository (for example MIT) and state it here.
