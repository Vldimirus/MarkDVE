#!/usr/bin/env bash

# Скрипт собирает отладочную конфигурацию и запускает автотесты.
set -euo pipefail

cmake --preset debug
cmake --build --preset debug
ctest --preset debug

