Bus schedule for the eZ430 Chronos
==================================

This is a module for the [openchronos-ng-elf](https://github.com/BenjaminSoelberg/openchronos-ng-elf) operating system on the eZ430 Chronos. It will show you when the next bus will arrive at a station.

How to build
------------

You will need the openchronos-ng-elf repository in order to build this.

Place your own bus schedules into the folder tables and run `compile_plan.py` to generate `busplan_data.h`.

Copy `busplan.c`, `busplan.cfg` and `busplan_data.h` into the modules folder of openchronos-ng-elf.

Run `make config` to activate the module, followed by `make`.
