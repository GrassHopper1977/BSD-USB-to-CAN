commonfiles := usb2can.h ./utils/timestamp.c ./utils/timestamp.h ./utils/logs.h

all: usb2can usb2can_hy test test_hy test2 test2_hy

usb2can: usb2can.c $(commonfiles)
	cc -g -O2 -Wall -mabi=purecap -cheri-bounds=subobject-safe -lusb -lssl -o usb2can usb2can.c utils/timestamp.c
	
usb2can_hy: usb2can.c $(commonfiles)
	cc -g -O2 -Wall -mabi=aapcs -cheri-bounds=subobject-safe -lusb -lssl -o usb2can_hy usb2can.c utils/timestamp.c

test: test.c $(commonfiles)
	cc -g -O2 -Wall -mabi=purecap -cheri-bounds=subobject-safe -lusb -lssl -o test test.c utils/timestamp.c

test_hy: test.c $(commonfiles)
	cc -g -O2 -Wall -mabi=aapcs -cheri-bounds=subobject-safe -lusb -lssl -o test_hy test.c utils/timestamp.c

test2: test2.c $(commonfiles)
	cc -g -O2 -Wall -mabi=purecap -cheri-bounds=subobject-safe -lusb -lssl -o test2 test2.c utils/timestamp.c

test2_hy: test2.c $(commonfiles)
	cc -g -O2 -Wall -mabi=aapcs -cheri-bounds=subobject-safe -lusb -lssl -o test2_hy test2.c utils/timestamp.c

.PHONY: clean

clean:
	rm -f usb2can usb2can_hy test test_hy test2 test2_hy
