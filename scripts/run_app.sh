#!/usr/bin/env bash

# Скрипт собирает отладочную версию проекта и запускает приложение.
set -euo pipefail

cmake --preset debug
cmake --build --preset debug
./build/debug/bin/MarkDVE
