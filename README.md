ABC80ofThings
Internet of Things using ABC80 emulation! The idea is to create a platform for sensors and general embedded stuff that can be scripted using ABC80 or ABC800 basic. Extremely useful..... ;)

abc80em contains basic abc80 emulation for ESP32 chips through the Arduino environment.

Everything is pure c. I have used an old Z80 emulator by Marat Fayzullin and added very basic ABC80 support.
Two roms are supported: Standard ABC80 and extended (ABC800 basic, with modified prompt to read "ABC81"). 
Real ABC802 emulation is in the works. Basic ABC832 disk access from files on an SD card are also simulated enabling use
of UFD-DOS.

Switch emulation mode by out100,81 for "ABC81", out 100,80 for ABC80 and later out 100,102 for ABC802 emulation
* ABC81 mode is really Basic II for ABC80 with UFDDOS + patched prompt
* ABC81 ROM is also patched to bypass screen writes to video memory and instead write to serial. Useful for embedded applications
* disk support by ABC832 emulation. Internal flash fits two disks...
* ESP32 sleep can be invoked by out 99,<sectosleep> where <sectosleep> is the number of seconds to sleep before booting again

This may be the physically smallest implementation of ABC80 emulation ever done..... 


