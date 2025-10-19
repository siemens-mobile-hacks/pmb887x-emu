#!/bin/bash
set -e

SRC=$(realpath $(dirname "$(realpath $0)")/..)

cd $SRC
docker compose up --build pmb887x-emu-win64 --remove-orphans
