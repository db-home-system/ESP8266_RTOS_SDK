#!/bin/bash

make

cp build/radiolog_app.bin .
openssl s_server -WWW -key server_certs/ca_key.pem -cert server_certs/ca_cert.pem -port 8070
