#!/bin/bash
set -uo pipefail

make -j8 || exit 1

cp build/radiolog_app.bin .
openssl s_server -WWW -key server_certs/ca_key.pem -cert server_certs/ca_cert.pem -port 8070
