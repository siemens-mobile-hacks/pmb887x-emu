# Debug output

`--trace`, `--trace-io` and `--trace-log` take a comma-separated list of channels. Names are case insensitive, may contain `*` and `?`, and a leading `-` turns a channel off.

```
pmb887x-emu --fullflash EL71.bin --trace 'USART*,LCD0'
pmb887x-emu --fullflash EL71.bin --trace-io '*,-DSP.*,-DMAC' --trace-time
```

A register access line:

```
[USART0] WRITE[4] F1000000: 00000108 (USART0_CLC): EDIS | RMC(0x01) (PC: 00401238, LR: 00400738)
```

A board may lack some of the channels below.

**CPU modules**

| Channel | Description |
|---|---|
| `USART0` | Serial port 0 |
| `USART1` | Serial port 1 |
| `USIF` | Universal serial interface |
| `SSC` | Synchronous serial port |
| `I2C` | I2C bus |
| `SIM` | SIM card interface |
| `MMCI` | Memory card interface |
| `USB` | USB |
| `DMAC` | DMA controller |
| `DIF` | Display interface |
| `EBU` | External bus |
| `STM` | System timer |
| `GPTU0` | General purpose timer 0 |
| `GPTU1` | General purpose timer 1 |
| `CAPCOM0` | Capture/compare unit 0 |
| `CAPCOM1` | Capture/compare unit 1 |
| `CGU` | Clock generation |
| `SCU` | System control and watchdog |
| `SCCU` | Sleep control |
| `RTC` | Real-time clock |
| `TPU` | GSM timing unit |
| `ADC` | Analog inputs |
| `AFC` | Frequency correction |
| `GPIO` | GPIO pins |
| `VIC` | Interrupt controller |
| `KEYPAD` | Keypad |
| `GPRSCU` | GPRS ciphering |
| `MMICIF` | Multimedia controller interface |
| `DSP` | DSP interface |
| `TCM` | Tightly coupled memory |
| `UNKNOWN` | Accesses outside any module |

**External devices**

| Channel | Description |
|---|---|
| `LCD0` | Display |
| `GIMMICK` | Graphics controller |
| `PMIC` | Power management |
| `RF` | RF transceiver |
| `FM` | FM radio |
| `AMP` | Audio amplifier |
| `SACC` | Audio codec |
| `SCROLL` | Scroll wheel |
| `SIM_SLOT` | SIM card |
| `FLASH0` | NOR flash 0 |
| `FLASH1` | NOR flash 1 |
| `FLASH2` | NOR flash 2 |
| `FULLFLASH` | Fullflash image |

**DSP**

| Channel | Description |
|---|---|
| `DSP.INT` | Interrupt controller |
| `DSP.DSP` | Control block |
| `DSP.MCS` | DSP/MCU semaphores |
| `DSP.TMR1` | Timer 1 |
| `DSP.TMR2` | Timer 2 |
| `DSP.CIPH` | GSM ciphering |
| `DSP.EQ` | Equalizer |
| `DSP.CHDEC` | Channel decoder |
| `DSP.MOD` | Modulator |
| `DSP.BB` | Baseband receiver |
| `DSP.AFE` | Audio front end |
| `DSP.SSC` | Serial port |
| `DSP.I2S1` | I2S port 1 |
| `DSP.I2S2` | I2S port 2 |
| `DSP.I2S3` | I2S port 3 |
| `DSP.UNKNOWN` | Accesses outside DSP peripherals |
| `DSP.TCG` | DSP code execution |
