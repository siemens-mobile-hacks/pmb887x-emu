# BTS

Osmocom GSM/GPRS network: MCC/MNC 001-01, GSM900, ARFCN 5.

```sh
docker compose up -d
docker compose logs -f bts
docker compose down
```

- Emulators connect to `trxmux` on UDP 5750, any number at once.
- Without `trxmux` (`docker compose stop trxmux`) one emulator can act as the TRX on UDP 5700–5702.
- Subscribers are created on first attach; authentication and ciphering are off.
- GPRS: APN `internet`, subnet 172.16.32.0/24, host 172.16.32.1 on `apn0`, no NAT.
- osmo-sgsn is patched to send LLC UI frames with C/R=1 (3GPP TS 44.064 6.2.2).
