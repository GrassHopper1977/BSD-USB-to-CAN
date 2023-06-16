# BSD-USB-to-CAN
An user space implementation of a CAN driver for Geschwister Schneider USB/CAN devices and bytewerk.org candleLight USB CAN interfaces. These devices all apear to be based on the MCP2518 series devices from Microchip. This is a simple attempt at getting a sockets interface working using libusb in user space on FreeBSD and CheriBSD.

To use it simply execute the application with your chosen CAN speed, the connect to port 2303 (Note: port can be changed, see Usage instructions).

Important Note: CheriBSD 22.12 and earlier have a bug in the USB driver that prevents libusb from working (see [here](https://github.com/CTSRD-CHERI/cheribsd/issues/1616)). It has been fixed and the fix will be available in the next release. The fix is also available in one of the later V22.12 releng releases available from [here](https://download.cheribsd.org/snapshots/releng/22.12/arm64/aarch64c/).

# Usage
From shell:
```
usb2can <s[rate] p[nnnn] d[nnnn]>

Where:
  s[rate] = Chosen bitrate. Defaults to 500k.
            Can choose from: 20k, 33.33k, 40k, 50k, 66.66k,
            80k, 83.33k, 100k, 125k, 200k, 250k, 400k, 500k,
            666k, 800k, 1m
  ? = Print help message
  p[nnnn] = Change the service's port number. Defaults to 2303.
  d[nnnn] = If you have multiple devices connected you can specify which one to connect to. If this is omitted it will connect to the first device that it finds.
```

# Message protocol
FreeBSD and CheriBSD don't support [SocketCAN](https://en.wikipedia.org/wiki/SocketCAN) yet but we are creating an interface that works in a similar fashion with the hope that this will make the transition easier. To that end we use `struct can_frame` as defined in usb2can.h to pass messages between `usb2can` and other programs. The format of the struct is based upon the SocketCAN structs (without the timing and CAN FD extensions for now - they will be added at a later date).

## CAN Errors
When a message arrives you can query the `CAN_ERR_FLAG` of the `can_id` memember to identify errors. The contents of `data` then tell you which error it is. Examples of decoding the errors can be seen in the function `print_can_frame()` in `usb2can.c`.

# Example of Use
The code in `test.c` connects to `usb2can` and transmits and recieves data and outputs it to `stdout`.

# Improvements To Be Made
1. Add support for CAN-FD frames.
2. If attempting to transmit multiple frame, it is possible that the current code will miss the extra messages if any of the frames get concatenated.
3. Currently, using synchronus libusb calls as the libusb file handlers weren't triggering. We need to look into this further.
4. Range checking the various inputs.
