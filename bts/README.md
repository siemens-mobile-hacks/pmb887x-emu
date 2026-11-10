# BTS

Osmocom GSM/GPRS network: MCC/MNC 001-01, GSM900, ARFCN 5.

```sh
docker compose up -d
docker compose logs -f bts
docker compose down
```

- Emulators connect to `trxmux` on `127.0.0.1:5750/udp`, any number at once.
- The stack runs in its own network namespace; the host network is not changed.
- Subscribers are created on first attach; authentication and ciphering are off.
- GPRS: any APN, subnet 172.16.32.0/24, NAT to the internet, DNS 1.1.1.1 and 8.8.8.8.
- osmo-sgsn is patched to send LLC UI frames with C/R=1 (3GPP TS 44.064 6.2.2).
