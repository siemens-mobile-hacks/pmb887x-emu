# BTS

Local GSM/GPRS network for the emulator: MCC/MNC 001-01.

```sh
docker compose up -d
docker compose down
```

- Run the emulator with `--gsm-trxmux 127.0.0.1:5750`; one emulator per cell, up to 10 cells.
- Settings are in `.env.default`: trxmux address and port, operator code, number of cells, GPRS ciphering, SIM key, phone number prefix, settings by SMS.
  Override them in `.env`.
- SIM IMSIs start with the operator code; the emulator uses `001010000001000` by default, give other emulators `--sim-imsi 001010000001001` and so on.
- New phones get the last 9 IMSI digits without leading zeros as their number (`1000`), and the settings of `config/provisioning.xml` by SMS: Internet, browser, MMS, streaming and device management.
- GPRS: any APN, internet access via NAT, GEA3 ciphering when the phone supports it.
- SMS center: `+1234`.
