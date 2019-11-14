# paper-io-genesis
Paper-io 1 Demake for Sega Genesis / Sega Mega Drive written in C using SGDK.

## Playing
Load the ROM into a Genesis / Mega Drive emulator, or put it on a flash-cart and play on hardware.
 - Get the built ROM here: 

## Building from Source
To build this game I have a TinyCoreLinux VM with gendev and SGDK set up.
 - Get gendev here: https://github.com/kubilus1/gendev
 
Once gendev is installed, I use `make -f $GENDEV/sgdk/mkfiles/makefile.gen clean all` to build the rom file. The code is mostly unoptimized so I set `-O3` flags in the makefile.
