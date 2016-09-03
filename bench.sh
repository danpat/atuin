#!/usr/bin/env bash

ab -k -n 1000 -c 4 "http://localhost:8080/tile/657/1582/12.mvt"
#ab -n 1000 -c 4 "http://localhost:8080/bad/657/1582/12.mvt"
