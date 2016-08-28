ftdictl: ftdictl.cpp ftdi/ftdi.c
	gcc -c ftdi/ftdi.c -I /usr/include/libusb-1.0/ -o ftdi.o
	g++ -fpermissive ftdictl.cpp ftdi.o -I /usr/include/libusb-1.0/ -lftdi -lusb-1.0 -o ftdictl -Wno-write-strings
