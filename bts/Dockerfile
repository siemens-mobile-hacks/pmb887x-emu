FROM debian:13 AS osmocom

RUN apt-get update \
	&& apt-get install --no-install-recommends -y ca-certificates extrepo \
	&& extrepo enable osmocom-latest \
	&& sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/extrepo_osmocom-latest.sources \
	&& apt-get update

FROM osmocom AS osmo-sgsn

WORKDIR /build
COPY patches/osmo-sgsn-llc-ui-command.patch .
RUN apt-get install --no-install-recommends -y dpkg-dev \
	&& apt-get build-dep -y osmo-sgsn \
	&& apt-get source osmo-sgsn \
	&& cd osmo-sgsn-*/ \
	&& patch -p1 < ../osmo-sgsn-llc-ui-command.patch \
	&& DEB_BUILD_OPTIONS="nocheck nodoc" dpkg-buildpackage -b -uc -us

FROM osmocom

COPY --from=osmo-sgsn /build/osmo-sgsn_*.deb /tmp/
RUN apt-get install --no-install-recommends -y osmo-bsc osmo-bts-trx osmo-cbc osmo-ggsn osmo-hlr osmo-mgw osmo-msc osmo-pcu osmo-stp libwbxml2-utils nftables python3 /tmp/osmo-sgsn_*.deb \
	&& rm -rf /tmp/*.deb /var/lib/apt/lists/*
