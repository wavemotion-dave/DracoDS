
# DracoDS
DracoDS is a Tandy Color Computer (CoCo 2) and Dragon 32/64 emulator for your DS/DSi/XL/LL handheld.

![image](./png/splash.png)

The Dragon and Tandy machines were quite similar - both using the same reference design and, when you 
really look under the hood, it's pretty clear that the Dragon reverse-engineered some of the Tandy
hardware such that it was virtually identical in some spots (e.g. Cartridge pinouts). There are some
differences - mainly in the keyboard matrix handling and in some of the disk controller handling. 

To that end, this emulator is primarily a Tandy CoCo 2 emulator with 32K standard and partial 64K 
support but it also works for the Dragon 32/64 and can be configured as such.

Features :
-----------------------
* Tandy CoCo 2 support with 32K of RAM (64K of RAM partially implemented... the ALL-RAM mode works but the paging does not). Running at 60Hz NTSC.
* Dragon 32/64 support with 32K or 64K of RAM running at the 50Hz PAL speed.
* Cassette (.cas) support for both the Dragon and Tandy emulated machines.
* Cartridge (.ccc) support for the Tandy emulated machine.
* Disk (.dsk) support for the Tandy emulated machine. Standard single-sided 160K disks only.
* Save/Load Game State (one slot).
* Artifacting support to 4-color high-rez mode.
* LCD Screen Swap (press and hold L+R+X during gameplay).
* Virtual keyboard stylized to the machine you've picked (there is a default global machine and you can override on a per-game basis)
* Full speed, full sound and full frame-rate even on older hardware.

![image](./png/dragon_kbd.png)
![image](./png/coco_kbd.png)

Copyright :
-----------------------
DracoDS is Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)

As long as there is no commercial use (i.e. no profit is made),
copying and distribution of this emulator, it's source code
and associated readme files, with or without modification, 
are permitted in any medium without royalty provided this 
copyright notice is used and wavemotion-dave (Phoenix-Edition),
Alekmaul (original port) and Marat Fayzullin (ColEM core) are 
thanked profusely.

The sound drivers (sn76496, ay38910 and Konami SCC) are libraries
from FluBBa (Fredrik Ahlström) and those copyrights remain his.

In addition, since the Z80 CPU and TMS9918 are borrowed from 
ColEM, please contact Marat (https://fms.komkon.org/ColEm/)
to make sure he's okay with what you're doing with the emulator
core.

The ColecoDS emulator is offered as-is, without any warranty.

Credits :
-----------------------
Thanks to Alekmaul who provided the baseline code to work with and to lobo for the menu graphical design.

Thanks to Flubba for the SN76496, AY38910 and SCC sound cores. 
You can seek out his latest and greatest at https://github.com/FluBBaOfWard

Thanks to the C-BIOS team for the open 
source MSX BIOS (see cbios.txt)

Thanks to Andy and his amazing Memotech 
Emulator MEMO which helped me get some
preliminary and simple MTX-500 support 
included.

Thanks to Darryl Hirschler for the keyboard graphics 
for the CreatiVision keypad, the ADAM keyboard,
the MSX keyboard and the MTX keyboard.

Thanks to Marcel de Kogel who wrote the Adam-EM 
emulator which is a bit of a grandfather to the
more modern emulators. I got the final bugs worked 
out of the VDP emulation and for disk drive
block caching on the ADAM thanks to his pioneering work.

Special thanks to  Marat Fayzullin, as the 
author of ColEM which is the code for the 
core emulation (specifically TMS9918 VDP
and the CZ80 CPU core).  I think the original 
port was circa ColEM 2.1 with some fixes and 
updated Sprite/Line handling from ColEM 5.6. 
Without Marat - this emulator simply wouldn't exist.

BIOS/BASIC Files Needed :
-----------------------
```
* e3879310 dragon32.rom - Dragon BASIC 16K
* 54368805 bas12.rom - Tandy CoCo BASIC 8K
* a82a6254 extbas11.rom - Tando CoCo Extended BASIC 8K
* 0b9c5415 disk11.rom - Tandy CoCo Disk Extended BASIC 8K
```

General Compatibility:
-----------------------
TBD

Joystick Options :
-----------------------
TBD

Keyboards :
-----------------------
TBD

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
V0.5: 05-July-2025 by wavemotion-dave
* Soon...
