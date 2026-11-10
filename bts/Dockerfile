FROM debian:13

RUN apt-get update \
	&& apt-get install --no-install-recommends -y ca-certificates extrepo \
	&& extrepo enable osmocom-latest \
	&& apt-get update \
	&& apt-get install --no-install-recommends -y osmo-bsc osmo-bts-trx osmo-cbc osmo-ggsn osmo-hlr osmo-mgw osmo-msc osmo-pcu osmo-sgsn osmo-stp python3 \
	&& rm -rf /var/lib/apt/lists/*
