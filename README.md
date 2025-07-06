![image](./png/DRACO-DS.png)

# DracoDS
DracoDS is a Tandy Color Computer (CoCo 2) and Dragon 32/64 emulator for your DS/DSi/XL/LL handheld.

The Dragon and Tandy machines were quite similar - both using the same Motorola reference design and, when you 
really look under the hood, it's pretty clear that the Dragon reverse-engineered some of the Tandy
hardware such that it was virtually identical in some spots (e.g. Cartridge pinouts). There are some
differences - mainly in the keyboard matrix handling and in some of the disk controller handling. 

To that end, this emulator is primarily a Tandy CoCo 2 emulator with 32K standard and partial 64K 
support but it also works for the Dragon 32/64 and can be configured as such.

![image](./png/splash.png)

Features :
-----------------------
* Tandy CoCo 2 support with 32K of RAM (64K of RAM partially implemented... the ALL-RAM mode works but the paging does not). Running at 60Hz NTSC.
* Dragon 32/64 support with 32K or 64K of RAM running at the 50Hz PAL speed.
* Cassette (.cas) support for both the Dragon and Tandy emulated machines.
* Cartridge (.ccc) support for the Tandy emulated machine.
* Disk (.dsk) support for the Tandy emulated machine. Standard single-sided 160K (35 track) or 180K (40 track) disks only.
* Save/Load Game State (one slot).
* Artifacting support to 4-color high-rez mode.
* LCD Screen Swap (press and hold L+R+X during gameplay).
* LCD Screen snapshot - (press and hold L+R+Y during gameplay and the .png file will be written to the SD card).
* Virtual keyboard stylized to the machine you've picked (there is a default global machine and you can override on a per-game basis)
* Full speed, full sound and full frame-rate even on older hardware.

![image](./png/dragon_kbd.png)
![image](./png/coco_kbd.png)

Copyright :  
-----------------------
DracoDS is Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)

This is a derivitive work of Dragon 32 Emu Copyright (c) 2018 Eyal Abraham (eyalabraham)
and can be found on github here:

https://github.com/eyalabraham/dragon32-emu

The dragon32-emu graciously allows modification and use via the lenient MIT Licence.

As far as I'm concerned: as long as there is no commercial use (i.e. no profit is made),
copying and distribution of this emulator, it's source code and associated readme files,
with or without modification, are permitted in any medium without royalty provided this 
copyright notice is used and wavemotion-dave and eyalabraham are thanked profusely.

The DracoDS emulator is offered as-is, without any warranty.

Credits :
-----------------------
Thanks to eyalabraham who provided the baseline code - without that work, this derivation doesn't happen.

BIOS/BASIC Files Needed :
-----------------------
```
* e3879310 dragon32.rom - Dragon BASIC 16K
* 54368805 bas12.rom - Tandy CoCo BASIC 8K
* a82a6254 extbas11.rom - Tando CoCo Extended BASIC 8K
* 0b9c5415 disk11.rom - Tandy CoCo Disk Extended BASIC 8K
```

The disk ROM is optional - but if you don't have it, then .dsk files will be hidden from the loader menu.

Place the BIOS/BASIC files in /roms/bios or else in the same directory as the emulator itself.

Loading Games :
-----------------------
There are three kinds of files supported: Cartridges, Cassettes and Disks.

Cartridges are the easiest... simply pick the .ccc filename from the Load Game menu and as soon as you start the emulation, the cartridge should auto-boot.

Cassettes work differently... you must load them up from the BASIC intepreter. So picking a .cas filename from the Load Game menu and starting the emulation
running should place you into the BASIC of your choice (Tandy CoCo BASIC or Dragon BASIC). From there, you type one of the following pairs of commands depending
on whether you are loading a Machine Code game (95% of the time this is true) or a BASIC game:
```
CLOADM
EXEC

CLOAD
RUN
```

You can press the START button to automatically issue the 'CLOADM' command. Note that you must wait for the Cassette to load after the CLOAD/CLOADM commands - the tape icon 
should go from green (reading) to white and the OK prompt should appear on BASIC again before you type the EXEC/RUN command.

Disks are the most complicated.  After loading you should do a DIR to see the contents of the disk. Then you issue a pair of commands as follows:

```
LOADM "FILENAME"
EXEC

LOAD "FILENAME"
RUN
```

Where FILENAME is the desired file as shown in the DIR command.

Dragon vs Tandy Mode :
-----------------------
The default mode is Tandy CoCo. You can change the default in Global Settings. You can also override on a per-game basis. Be sure to run games intended for the Dragon in Dragon mode (you should see the Dragon logo on the keyboard) and be sure to run Tandy CoCo games in Tandy mode (the Tandy Color Computer logo should be shown on the main keyboard graphic).


Blue or Orange Screen? :
-----------------------
Because the artifacting on an NTSC CoCo was a bit hit-or miss (that is, from one setup to another it might reverse the Orange/Blue colors), some games took to 
putting up an artifacting wall of color on the opening screen to allow the user to press RESET so that the color might change... if you see this in any 
game, just press ENTER (some games might use SPACE) to bypass it.  DracoDS should always present the standard artifacting color set and always be 'right'.

64K Support :
-----------------------
The emulators does emulate a full 64K machine but there are some caveats. If the game utilizes just the RAM/ROM and ALL-RAM memory maps, the game should work fine.
Many such 64K work this way. But if the game utilizes the Paging Register to map the upper RAM into the lower half of the address space - then it will not work 
with the current emulation. Unfortunately Sailor Man falls into this latter category.

Joystick Options :
-----------------------
The Tandy CoCo and Dragon machines used an analog joystick that is difficult to emulate properly on the DS/DSi. To that end, the default 'Joystick' is digital - that 
is, pressing the D-Pad will emualte extreme analog positions of a real joystick. This works fine for many games... but some games really did take advantage of the
multi-position of a real analog stick. So you can change the default 'DIGITAL' controller to 3 speeds of Analog - both self-centering and none. Games like POLARIS, for
example, play fine with the 'SLOW ANALOG' setting.  Experiment and figure out what works best for any given game. Your joystick settings are saved on a per-game basis.

The default joystick is the RIGHT port joystick but every game seems to be different... if the game isn't responding to the RIGHT joystick port, switch it in the 
game options to the LEFT port (you don't have to exit the game to make this change - the DS MINI menu has the Game Options available from the little Cassette Icon)

Keyboards :
-----------------------
Both the Tandy and Dragon keyboards behave the same - there is only the cosmetic difference of the company banner/logo at the top.  Note that due to the limitations
of the DS touch-screen where only one key can be pressed at a time, the SHIFT key works like a temporary toggle. Press it and then the next key you press will be SHIFT-ed.

Compile Instructions :
-----------------------
gcc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0
libnds 1.8.2-1
I use Ubuntu and the Pacman repositories (devkitpro-pacman version 6.0.1-7).  I'm told it should also build under 
Windows but I've never done it and don't know how.

If you've got the nds libraries above setup correctly it should be a matter of typing:
* _make clean_
* _make_

To build the emulator. The output of this is ColecoDS.nds with a version as set in the MAKEFILE.
I use the following standard environment variables that are SET on Ubuntu:
* DEVKITARM=/opt/devkitpro/devkitARM
* DEVKITPPC=/opt/devkitpro/devkitPPC
* DEVKITPRO=/opt/devkitpro

To create the soundbank.bin and soundbank.h (sound effects) file in the data directory:

mmutil -osoundbank.bin -hsoundbank.h -d *.wav

And then move the soundbank.h file to the arm9/sources directory

Versions :
-----------------------
V0.6: 06-July-2025 by wavemotion-dave
* Disk sizes supported now includes 160K (35 track) and 180K (40 track).
* Improved disk read/write handling (still not backed to SD)
* Proper disk icon when a .dsk is loaded and floppy loading sounds added.
* Improved PIA handling to eliminate small sound glitches on some games.
* Mirrors for PIA now handled.
* Macros for Dungeons of Daggorath added!
* Added first pass at Save/Load State

V0.5: 05-July-2025 by wavemotion-dave
* First public beta!
