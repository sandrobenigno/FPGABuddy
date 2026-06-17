# FPGA-Companion SPI Protocol Specification

This document acts as the reference specification for the SPI communication protocol between the FPGA Core (Atari / MiSTeryNano) and its MCU Companion (such as RP2040, BL616, or Raspberry Pi Pico). It is created from the companion's original headers and documentation files.

---

## 1. Physical Interface & Bus Connections

The connection between the MCU (SPI Master) and the FPGA (SPI Slave) is composed of 5 signals:

| Signal | Direction | Active Level | Description |
| :--- | :---: | :---: | :--- |
| **CSN** | MCU $\rightarrow$ FPGA | Active Low | SPI Chip Select. Initiates and frames transactions. |
| **SCK** | MCU $\rightarrow$ FPGA | - | SPI Clock. Idle Low (runs up to 20 MHz). |
| **MOSI** | MCU $\rightarrow$ FPGA | - | SPI Data from MCU to FPGA. |
| **MISO** | FPGA $\rightarrow$ MCU | - | SPI Data from FPGA to MCU. |
| **IRQN** | FPGA $\rightarrow$ MCU | Active Low | Hardware Interrupt Request. Allows the FPGA to request MCU attention without polling. |

### SPI Parameters
* **SPI Mode:** **Mode 1** (CPOL = 0, CPHA = 1 or CPOL = 1, CPHA = 0). Clock is idle Low, data is sampled on the **falling edge** of SCK, and shifted on the **rising edge** of SCK.
* **Bit Order:** **MSB First** (Most Significant Bit first).

---

## 2. Hardware Interrupts (IRQN)

The **IRQN** line is asserted Low by the FPGA whenever it requires cooperation or processing from the MCU. The MCU detects this transition via a GPIO interrupt and reads/clears the source. 

The three active interrupt bits are:

| Interrupt Bit | Target | Trigger Condition |
| :---: | :---: | :--- |
| **Bit 0** | **SYS** | FPGA has been cold booted / re-initialized, or serial port data is ready. |
| **Bit 1** | **HID** | DB9 joystick physical event detected by the FPGA core. |
| **Bit 3** | **SDC** | FPGA requests an SD Card sector translation/read/write operation. |

---

## 3. Protocol Frame Structure

SPI transactions are framed by **CSN** going Low.
* **Byte 0 (First Byte):** **Target ID** inside the FPGA.
* **Byte 1 (Second Byte):** **Command ID** for the selected target. (The `mcu_start` strobe pulses High in the FPGA during the reception of this byte).
* **Byte 2+:** **Payload / Parameter Bytes** (Subcommands, offsets, data packets).

### Target ID Table

| Target ID | Name | Description | Purpose |
| :---: | :---: | :--- | :--- |
| `0` | **SYS** | Generic System Control | System status, LEDs, physical buttons, IRQ management, serial ports, XML config. |
| `1` | **HID** | Human Interface Devices | USB/physical keyboard, mouse, and joysticks. |
| `2` | **OSD** | On-Screen Display | Showing/hiding and rendering the 128x64 OSD text/graphics menu. |
| `3` | **SDC** | SD Card Control | Sector-level read/write translation for emulated floppy/HDD drives. |
| `4` | **AUDIO** | Audio Output | Fake floppy drive sound synthesis and buzzer feedback (planned). |

---

## 4. SYS Target (0) Commands

Addresses general FPGA system control.

| Cmd ID | Command Constant | Description |
| :---: | :--- | :--- |
| `0` | `SPI_SYS_STATUS` | Request system identification status. Typically returns `0x5C` and `0x42` (confirming core type). |
| `1` | `SPI_SYS_LEDS` | Send LED status byte. Controls Tang Nano onboard LEDs 4 & 5. |
| `2` | `SPI_SYS_RGB` | Send 24-bit RGB values (3 bytes) to drive the onboard WS2812 status LED. |
| `3` | `SPI_SYS_BUTTONS` | Request button states of Tang Nano buttons S0/S1 (Bit 0: reset, Bit 1: user button). |
| `4` | `SPI_SYS_SETVAL` | Set a chipset/system variable. Expects 2 bytes: ASCII Variable ID + 8-bit value (e.g. ID `'C'` = Chipset; values `0`=ST, `1`=MegaST, `2`=STE). |
| `5` | `SPI_SYS_IRQ_CTRL` | Interrupt control. Byte 2: Acknowledges (clears) interrupt sources. Returns pending interrupts. |
| `6` | `SPI_SYS_IRQ_SRC` | Request system IRQ source. Returns bit field (Bit 0: cold boot, Bit 1: serial port data ready, Bit 2: FPGA button state change). |
| `7` | `SPI_SYS_PORT` | Handle serial port (RS232) redirection over Telnet/Wi-Fi. |
| `8` | `SPI_SYS_READ_CFG` | Read core-specific XML configuration to dynamically draw the OSD menus. |
| `9` | `SPI_SYS_JTAGSEL` | Select JTAG line configurations (if supported). |

### XML Configuration Reading (`SPI_SYS_READ_CFG`)
* Allows the FPGA to describe its menu layout to the companion dynamically.
* Sent starting with the second payload byte of the read command.
* **Plain Text Format:** Must start with `<` and end with a null terminator `\x00`.
* **GZIP Compressed Format:** Starts with `0x1F` (GZIP magic header). Compressing the XML reduces storage space in the FPGA ROM by ~75%.

### Redirection Port Subcommands (`SPI_SYS_PORT`)
| Sub-Cmd ID | Subcommand Constant | Description |
| :---: | :--- | :--- |
| `0` | `SPI_SYS_PORT_STATUS` | Get serial port FIFO/bitrate status. |
| `1` | `SPI_SYS_PORT_GET` | Read incoming bytes from the core's serial port. |
| `2` | `SPI_SYS_PORT_PUT` | Write outgoing bytes into the core's serial port. |

---

## 5. HID Target (1) Commands

Handles input peripherals.

| Cmd ID | Command Constant | Description |
| :---: | :--- | :--- |
| `0` | `SPI_HID_STATUS` | Request HID requirements. Returns `0x5C` and `0x42` (confirming interface). |
| `1` | `SPI_HID_KEYBOARD`| Send a keyboard matrix byte (press/release row/col code). F12 is trapped by the MCU to toggle OSD. |
| `2` | `SPI_HID_MOUSE` | Send mouse data (3 bytes: state of mouse buttons in bits 0-1 + relative X offset + relative Y offset). |
| `3` | `SPI_HID_JOYSTICK`| Send joystick state (5 bytes: Player Address + 8-bit digital joystick buttons + proportional analog X + proportional analog Y + 8 extra digital gamepad buttons). |
| `4` | `SPI_HID_GET_DB9` | Read the state of the physical DB9 joystick ports connected directly to the FPGA (for OSD navigation). |

---

## 6. OSD Target (2) Commands

Controls the On-Screen Display. The OSD uses a 128x64 monochrome framebuffer (1024 bytes) organized in vertical tiles, matching the `u8g2` library.

| Cmd ID | Command Constant | Description |
| :---: | :--- | :--- |
| `1` | `SPI_OSD_ENABLE` | Show (1) or Hide (0) the OSD centered overlay on the video output. |
| `2` | `SPI_OSD_WRITE` | Write graphics data. Followed by a 1-byte tile column offset and $N$ data bytes to update regions. |

---

## 7. SDC Target (3) Commands

Manages the SD Card. Since the SD card is physically connected to the companion (RP2040), the FPGA requests sector data through this target, and the MCU translates image file offsets into physical sectors.

| Cmd ID | Command Constant | Description |
| :---: | :--- | :--- |
| `1` | `SPI_SDC_STATUS` | Read card status. Returns generic card initialization/type status, busy status (Bit 1), floppy sector request status (Bit 0: drive A, Bit 1: drive B), and the requested 4-byte sector number. |
| `2` | `SPI_SDC_CORE_RW` | Instruct the FPGA core to perform a direct sector read/write on the card for its own purposes. Followed by the 4-byte sector number. |
| `3` | `SPI_SDC_MCU_READ`| Request sector read for MCU filesystem usage. Followed by the 4-byte sector number. Returns busy status, then `0x00`, followed by 512 bytes of data. |
| `4` | `SPI_SDC_INSERTED`| Inform the core about disk image insertion/selection. Followed by the drive ID and 4-byte image size (in sectors). A size of 0 indicates disk ejection. |
| `5` | `SPI_SDC_MCU_WRITE`| Request sector write on behalf of the MCU filesystem. Followed by the 4-byte sector number and 512 bytes of data. |
| `6` | `SPI_SDC_DIRECT` | Tell the core that the selected image is contiguous (not fragmented) on the SD card, allowing direct core access. Followed by the LBA of the first sector. |
| `7` | `SPI_SDC_INS_LARGE`| Inform the core of a large disk image (>4GB) insertion. Protects smaller cores from corrupting data. |
| `8` | `SPI_SDC_IMAGE` | Select/mount disk image file subcommands. |

### Image Subcommands (`SPI_SDC_IMAGE`)
| Sub-Cmd ID | Constant | Description |
| :---: | :--- | :--- |
| `0` | `SPI_SDC_IMAGE_STATUS` | Read mounting status. |
| `1` | `SPI_SDC_IMAGE_SELECT` | Set active drive and mount image. |
| `2` | `SPI_SDC_IMAGE_WRITE` | Trigger sector write. |

---

## 8. AUDIO Target (4) Commands

Designed for buzzer/sound simulation (e.g. simulating floppy drive mechanical stepper sounds).

| Cmd ID | Command Constant | Description |
| :---: | :--- | :--- |
| `1` | `SPI_AUDIO_ENABLE` | Enable or disable fake floppy/buzzer sound synthesis. |
| `2` | `SPI_AUDIO_BUFFER` | Read the core's audio buffer capacity/usage. |
| `3` | `SPI_AUDIO_WRITE` | Write audio samples. |
