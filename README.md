ARM_FreeRTOS_Base
=====================

FreeRTOS starter project for ARM microcontrollers.

## Prerequisites
1. GNU Make
2. [gcc-arm-embedded](https://launchpad.net/gcc-arm-embedded)

## Creating a new project:
1. Create (if necessary) board and CPU definitions (see existing examples for details).
2. Create an application folder in the apps directory
3. Create a config.mk file for your application (see existing examples)
4. Store your application code in the newly created directory and edit your config.mk file appropriately

## Building
1. 'cd' to the root of base project directory
2. type 'make APP=your\_app\_name'

## Debugging
1. Install openocd with support for your JTAG dongle
2. Add an openocd.cfg to your project (see existing examples)
3. Install DDD (data display debugger) (optional)
4. Type: sudo make APP=your\_app\_name openocd (this starts the
   debugger)
5. Attach your favorite debugger (gdb/ddd/insight) to the openocd
   session (a make command for ddd is included in the makefile)

## ITM Trace output
1. Download, build, and install
   [swo-tracer](https://github.com/yurovsky/swo-tracer)
2. Compile and flash your firmware
3. Start openocd with trace support (see example openocd.cfg files)
4. Start gdb and enable semihosting (make APP=your\_app gdb)
5. Launch swo-tracer and point it at the swo.log file that is
   generated by openocd (swo-tracer -t swo.log). You may need to use
   sudo
6. Use GDB to reset your firmware and any printf output should be
   displayed in the swo-tracer screen

## Licensing

All of the makefiles and device drivers contained herin are licensed
under the MIT license, HOWEVER, any application code is property of
its respective owners. Any vendor provided code (ST SPL for example)
are subject to their included licenses.
