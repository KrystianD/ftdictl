INC=/home/krystiand/prog/kdlibs/
KDLIBS=/home/krystiand/prog/kdlibs/kdstring.cpp
ftdictl: ftdictl.cpp ftdi/ftdi.c $(KDLIBS)
	gcc -c ftdi/ftdi.c -I /usr/include/libusb-1.0/ -o ftdi.o
	g++ -fpermissive $(KDLIBS) -I$(INC) ftdictl.cpp ftdi.o -I /usr/include/libusb-1.0/ -lftdi -lusb-1.0 -o ftdictl -Wno-write-strings
