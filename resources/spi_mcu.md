# MCU-FPGA SPI Communication Protocol Reference

This document serves as the complete technical reference for the SPI communication interface between the RP2040 MCU and the FPGA Core, as implemented in [mcu_spi.v](file:///x:/ATARI/FPGABuddy/resources/FPGACoreFiles/mcu_spi.v).

---

## 1. Interface Specifications & Ports

The module `mcu_spi` interfaces the MCU's SPI lines (in the SPI clock domain) with the internal FPGA core modules (in the main FPGA system clock domain `clk`).

### Port Definition Table

| Port Name | Direction | Width | Domain | Description |
| :--- | :--- | :---: | :--- | :--- |
| `clk` | Input | 1 bit | FPGA System | Main FPGA system clock. |
| `reset` | Input | 1 bit | FPGA System | System Reset (declared but unused in the core logic). |
| `spi_io_ss` | Input | 1 bit | SPI | Slave Select (Chip Select). Active-low line from the MCU. A high level resets all SPI counters and drives MISO low. |
| `spi_io_clk` | Input | 1 bit | SPI | SPI Serial Clock driven by the MCU. |
| `spi_io_din` | Input | 1 bit | SPI | MOSI (Master Out Slave In) - Serial data from MCU to FPGA. |
| `spi_io_dout` | Output | 1 bit | SPI | MISO (Master In Slave Out) - Serial data from FPGA to MCU. |
| `mcu_sys_strobe` | Output | 1 bit | FPGA System | Pulsed high for 1 `clk` cycle when a data byte is received for **System Control** (Target `0x00`). |
| `mcu_hid_strobe` | Output | 1 bit | FPGA System | Pulsed high for 1 `clk` cycle when a data byte is received for **HID/Input Emulation** (Target `0x01`). |
| `mcu_osd_strobe` | Output | 1 bit | FPGA System | Pulsed high for 1 `clk` cycle when a data byte is received for **OSD (On-Screen Display)** (Target `0x02`). |
| `mcu_sdc_strobe` | Output | 1 bit | FPGA System | Pulsed high for 1 `clk` cycle when a data byte is received for **SD Card Emulation/Control** (Target `0x03`). |
| `mcu_start` | Output | 1 bit | FPGA System | Protocol indicator. Set high when exactly 2 bytes (Target ID + 1st data byte) have been processed. |
| `mcu_sys_din` | Input | 8 bits | FPGA System | Parallel data from **System Control** to be shifted out on MISO. |
| `mcu_hid_din` | Input | 8 bits | FPGA System | Parallel data from **HID** to be shifted out on MISO. |
| `mcu_osd_din` | Input | 8 bits | FPGA System | Parallel data from **OSD** to be shifted out on MISO. |
| `mcu_sdc_din` | Input | 8 bits | FPGA System | Parallel data from **SD Card** to be shifted out on MISO. |
| `mcu_dout` | Output | 8 bits | FPGA System | Parallel output byte containing the last received data byte from the MCU. |

---

## 2. SPI Physical Layer & Clocking Parameters

* **SPI Mode:** **Mode 1** (CPOL = 0, CPHA = 1 or CPOL = 1, CPHA = 0).
  * **MOSI Sampling:** The FPGA samples `spi_io_din` on the **falling edge** of `spi_io_clk`.
  * **MISO Setup:** The FPGA updates `spi_io_dout` on the **rising edge** of `spi_io_clk`.
  * **Idle State:** Clock idle state does not prevent correct operation, but SPI data lines are pulled/driven low when idle.
* **Bit Order:** **MSB First** (Most Significant Bit is transmitted first).
  * MOSI: Bits are shifted left into the shift register (`spi_sr_in`).
  * MISO: Indexed by `in_byte[~spi_cnt[2:0]]`, where `spi_cnt[2:0]` counts from `0` to `7`. This evaluates to index `7` (MSB) first, down to `0` (LSB) last.

---

## 3. Frame Protocol & Transaction Structure

Every SPI transaction is framed by the `spi_io_ss` line:
1. MCU pulls `spi_io_ss` **Low** to start a transaction.
2. MCU transmits **Byte 0** (Target ID) to select which internal core component will receive/transmit data.
3. MCU transmits **Byte 1 and subsequent bytes** (Data Bytes).
4. MCU pulls `spi_io_ss` **High** to terminate the transaction.

### Target ID Mappings

| Target ID (Byte 0) | Associated Target Strobe | MISO Return Data Input | Purpose |
| :---: | :--- | :--- | :--- |
| `8'd0` (`0x00`) | `mcu_sys_strobe` | `mcu_sys_din` | System control registers / Core configuration |
| `8'd1` (`0x01`) | `mcu_hid_strobe` | `mcu_hid_din` | Keyboard, joystick, mouse inputs (HID) |
| `8'd2` (`0x02`) | `mcu_osd_strobe` | `mcu_osd_din` | On-Screen Display rendering and text buffers |
| `8'd3` (`0x03`) | `mcu_sdc_strobe` | `mcu_sdc_din` | SD card interface emulation / Sector transfer |
| *Others* | None | `8'h00` | Reserved / Ignored |

### Strobe & Data Output Logic

* **Target ID Latching:** The first byte (when byte index `spi_in_cnt == 0`) is saved into the register `spi_target`. No strobe is fired during this byte.
* **Data Byte Latching:** For every subsequent byte (when `spi_in_cnt > 0`):
  * The received byte is updated in `spi_in_data` and instantly reflected on `mcu_dout`.
  * The target strobe corresponding to `spi_target` (e.g. `mcu_sys_strobe`) is pulsed High for exactly **1 cycle** of `clk`.
* **Saturation of Byte Counter:** `spi_in_cnt` increments on each byte received, but saturates at `4'd15`. It remains at `15` if more than 15 bytes are sent within a single SS frame. It is reset to `0` only when `spi_io_ss` goes High.

---

## 4. Clock Domain Crossing (CDC) & Synchronization

Since the SPI interface runs on `spi_io_clk` and the FPGA core logic runs on `clk`, a clock domain crossing mechanism is implemented:
1. In the `spi_io_clk` falling-edge domain, when `spi_cnt[2:0] == 3'd7` (end of a byte):
   * The complete byte is latched into `spi_data_in`.
   * A 1-bit toggle flag `spi_data_in_ready` is inverted: `spi_data_in_ready <= !spi_data_in_ready`.
2. In the `clk` rising-edge domain, a 2-stage shift register (`spi_data_in_readyD`) synchronizes and detects transitions of `spi_data_in_ready`:
   ```verilog
   spi_data_in_readyD <= { spi_data_in_readyD[0], spi_data_in_ready };
   ```
3. A byte is processed in the system domain when a change is detected:
   ```verilog
   if(spi_data_in_readyD[1] ^ spi_data_in_readyD[0]) begin
       // Processing and strobe generation
   end
   ```
* **SPI Clock Frequency Constraint:** The toggle-synchronizer works up to an SPI clock speed equal to **2x** the FPGA clock frequency (e.g., up to 64 MHz SPI clock with a 32 MHz system clock), though a lower ratio is recommended to prevent setup/hold violations on the static `spi_data_in` bus.

---

## 5. MISO (Readback) Timing Analysis

* **MISO Multiplexer:** The byte sent back to the MCU is determined by the selected `spi_target`:
  ```verilog
  wire [7:0] in_byte = 
      (spi_target == 8'd0)? mcu_sys_din :
      (spi_target == 8'd1)? mcu_hid_din :
      (spi_target == 8'd2)? mcu_osd_din :
      (spi_target == 8'd3)? mcu_sdc_din : 8'h00;
  ```
* **Byte 0 (Target ID phase) Readback:**
  * When `spi_io_ss` is pulled Low, the MCU starts sending Byte 0 (the Target ID).
  * Since `spi_target` is not yet updated for this transaction, MISO shifts out the data belonging to the **previously selected** target.
  * The MCU must discard the byte received on MISO during the transmission of Byte 0.
* **Byte 1+ (Data phase) Readback:**
  * After Byte 0 is completed, `spi_target` updates in the `clk` domain (taking 2-3 `clk` cycles).
  * Provided the FPGA clock `clk` is fast enough relative to the SPI clock, `spi_target` changes before the first rising edge of Byte 1's clock.
  * Thus, during Byte 1 and all subsequent bytes, MISO successfully shifts out the data from the newly selected target (`mcu_sys_din`, `mcu_hid_din`, etc.).

---

## 6. Control and Status Signals

### `mcu_start`
* **Definition:** `assign mcu_start = spi_in_cnt == 2;`
* **Timing:** 
  * Initially `spi_in_cnt = 0` (`mcu_start = 0`).
  * Byte 0 processed -> `spi_in_cnt` becomes `1` (`mcu_start = 0`).
  * Byte 1 (first data byte) processed -> `spi_in_cnt` becomes `2` (`mcu_start = 1`).
  * Byte 2 (second data byte) processed -> `spi_in_cnt` becomes `3` (`mcu_start = 0`).
* **Purpose:** Acts as a pulse/status signal that remains high during the entire period where exactly 1 data byte has been successfully received and processed, and transitions back to low when the second data byte starts/completes. This is typically used by the core to detect the start of a multi-byte command block.

---

## 7. Hardware-Based Cartridge & Banking Auto-Detection (`detect2600.sv`)

The FPGA core incorporates an on-the-fly hardware auto-detection module [detect2600.sv](file:///x:/ATARI/FPGABuddy/resources/FPGACoreFiles/detect2600.sv) to identify the cartridge size, banking model, and Superchip RAM configuration.

### A. Core Detection Mechanism
* **No Software Overhead:** The MCU does **not** need to send banking commands or configuration headers to the FPGA.
* **On-The-Fly Sniffing:** As the ROM bytes are streamed via SPI to Target `0x00` (System Control), the FPGA routes the stream (the data bytes, active strobes, and sequential addresses) to the `detect2600` module.
* **Signature Detectors:** The module runs parallel pattern matchers (`match_bytes`) checking for specific 6502 assembly instruction sequences that identify particular banking formats:
  * **3F:** Checks for `STA $3F` (`0x85, 0x3F`).
  * **3E:** Checks for `STA $3E` (`0x85, 0x3E`) and `STA $3F`.
  * **FE:** Checks for JSR sequences (`JSR $D000; DEC $C5` or similar) used by Activision.
  * **E0:** Checks for absolute non-indexed accesses to `$FE0` to `$FF9`.
  * **UA:** Checks for Brazilian/Digivision banking style accesses (`STA $240`, `LDA $240`, `STA $2C0`, etc.).
  * **CV, WD, SB, E7, CDF, DPCP, CTY:** Each has dedicated byte pattern matching.

### B. Size-Based Selection & Counter
* **Final Latching:** When the transaction ends (indicated by `spi_io_ss` returning to High), the FPGA latches the final value of its write address counter into the `cart_size` register.
* **Counting Exclusion:** The write counter increments only when `mcu_sys_strobe` pulses. Because the strobe is gated to `spi_in_cnt > 0`, the first byte (Target ID `0x00`) is excluded from the counter. Hence, `cart_size` receives the exact size of the ROM file (e.g. `2048`, `4096`, `8192`, etc.).
* **Heuristics:** If no signature matches, the model defaults to standard sizes:
  * $\le$ 2KB $\rightarrow$ `BANK2K` (Standard 2KB)
  * $\le$ 4KB $\rightarrow$ `BANK00` (Standard 4KB)
  * $\le$ 8KB $\rightarrow$ `BANKF8` (F8 8KB)
  * $\le$ 16KB $\rightarrow$ `BANKF6` (F6 16KB)
  * $\le$ 32KB $\rightarrow$ `BANKF4` (F4 32KB)

### C. Superchip (SC) RAM Detection
* **Dual CRC32 Comparison:** Superchip cartridges feature 128 bytes of RAM at the beginning of each 4KB bank. The FPGA checks if the first 128 bytes of a bank are duplicated in the subsequent 128 bytes:
  1. Computes the CRC32 of bytes `0` to `127` (`sc_crc0`).
  2. Computes the CRC32 of bytes `128` to `255` (`sc_crc1`).
  3. When `addr == 256` (`16'h100`), it asserts `has_sc <= (sc_crc0 == sc_crc1)`.
  4. Repeat the validation for the second 4KB bank (offset `16'h1000` to `16'h1100`).
* **Output:** If `has_sc` remains high, the core sets `sc <= 1` (enabling Superchip write-RAM mirrors).

