# BTS

Osmocom GSM/GPRS network for the emulator: MCC/MNC 001-01, GSM900, ARFCN 5.
The emulator acts as the TRX of `osmo-bts-trx` over UDP on 127.0.0.1 (emulator 5700–5702, BTS 5800–5802).

```sh
docker compose up -d
docker compose logs -f bts
docker compose down
```

- Subscribers are created on first attach, authentication and ciphering are off.
- GPRS: APN `internet`, subscribers get 172.16.32.0/24, the host is 172.16.32.1 on `apn0`; no NAT to the internet.
