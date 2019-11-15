# paper-io-genesis
Paper-io 1 Demake for Sega Genesis / Sega Mega Drive written in C using SGDK.

## Playing
Load the ROM into a Genesis / Mega Drive emulator.
 - Get the built ROM here: [ROM File](https://github.com/TomAwezome/paper-io-genesis/releases/latest)

## Building from Source
To build this game I have a TinyCoreLinux VM with gendev and SGDK set up.
 - Get gendev here: [gendev](https://github.com/kubilus1/gendev)
 
Once gendev is installed, I use `make -f $GENDEV/sgdk/mkfiles/makefile.gen clean all` to build the rom file. The code is mostly unoptimized so I set `-O3` flags in the makefile.

## Screenshots

![Screenshot 1](https://github.com/TomAwezome/paper-io-genesis/blob/master/screenshot1.png "Screenshot 1")
![Screenshot 2](https://github.com/TomAwezome/paper-io-genesis/blob/master/screenshot2.png "Screenshot 2")
