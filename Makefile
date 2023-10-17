CFLAGS = -g -O2 -Wall -cheri-bounds=subobject-safe
LFLAGS = -lusb -lssl
PURECAP = -mabi=purecap
HYBRID = -mabi=aapcs

SOURCEFILES = ./utils/timestamp.c
HEADERFILES = usb2can.h ./utils/timestamp.h ./utils/logs.h
ALLFILES= $(SOURCEFILES) $(HEADERFILES)

commonfiles := usb2can.h ./utils/timestamp.c ./utils/timestamp.h ./utils/logs.h

all: usb2can usb2can_hy test test_hy test2 test2_hy

usb2can: usb2can.c $(ALLFILES)
	cc $(CFLAGS) $(PURECAP) $(LFLAGS) usb2can.c $(SOURCEFILES) -o usb2can 
	
usb2can_hy: usb2can.c $(ALLFILES)
	cc $(CFLAGS) $(HYBRID) $(LFLAGS) usb2can.c $(SOURCEFILES) -o usb2can_hy

test: test.c $(ALLFILES)
	cc $(CFLAGS) $(PURECAP) $(LFLAGS) test.c $(SOURCEFILES) -o test

test_hy: test.c $(ALLFILES)
	cc $(CFLAGS) $(HYBRID) $(LFLAGS) test.c $(SOURCEFILES) -o test_hy

test2: test2.c $(ALLFILES)
	cc $(CFLAGS) $(PURECAP) $(LFLAGS) test2.c $(SOURCEFILES) -o test2

test2_hy: test2.c $(ALLFILES)
	cc $(CFLAGS) $(HYBRID) $(LFLAGS) test2.c $(SOURCEFILES) -o test2_hy

.PHONY: clean

clean:
	rm -f usb2can usb2can_hy test test_hy test2 test2_hy
