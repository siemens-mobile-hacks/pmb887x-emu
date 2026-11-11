# BTS

Local GSM/GPRS network for the emulator: MCC/MNC 001-01.

```sh
docker compose up -d
docker compose down
```

- Run the emulator with `--gsm-trxmux 127.0.0.1:5750`; up to four emulators at once.
- New phones get Internet, browser and MMS settings by SMS automatically.
- GPRS: any APN, internet access via NAT.
- SMS center: `+1234`.
