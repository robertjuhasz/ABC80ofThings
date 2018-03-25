ABC80ofThings
Internet of Things using ABC80 emulation! The idea is to create a platform for sensors and general embedded stuff that 
can be scripted using ABC80 or ABC800 basic. Extremely useful..... ;)

abc80em contains basic abc80 emulation for Teensy 3.2 and 3.6 microcontroller chips through the Arduino environment.
Get them here or here: 
    https://www.pjrc.com/store/teensy36.html
    https://www.electrokit.com/teensy-3-6.54524

Everything is pure c. I have used an old Z80 emulator by Marat Fayzullin and added very basic ABC80 support.
Two roms are supported: Standard ABC80 and extended (ABC800 basic, with modified prompt to read "ABC81"). 
Real ABC802 emulation is in the works. Basic ABC832 disk access from files on an SD card are also simulated enabling use
of UFD-DOS.

Switch emulation mode by out100,81 for "ABC81", out 100,80 for ABC80 and later out 100,102 for ABC802 emulation

We are able to simulate a 32K computer with 1+1 K video RAM using only ca 41K RAM!

Screen emulation is extremely crude: Data is written to the (USB) serial port of the Teensy. Whenever a write occurs to the video memory, a VT100 positioning command + the character is written to serial. Very slow, but works fine if you don't do large screen scrolls....

This may be the physically smallest implementation of ABC80 emulation ever done..... 


