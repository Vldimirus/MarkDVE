#!/usr/bin/env bash

# Скрипт конфигурирует и собирает проект в релизном режиме.
set -euo pipefail

cmake --preset release
cmake --build --preset release

