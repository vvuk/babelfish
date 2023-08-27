#!/bin/bash

set -e

cd `dirname $0`

ninja -C build
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 20000" -c "program build/babelfish.elf verify reset exit"

