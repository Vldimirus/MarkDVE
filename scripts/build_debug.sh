#!/usr/bin/env bash

# Скрипт конфигурирует и собирает проект в отладочном режиме.
set -euo pipefail

cmake --preset debug
cmake --build --preset debug

