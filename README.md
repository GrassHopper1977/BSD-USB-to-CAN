# BSD-USB-to-CAN
An user space implementation of a CAN driver for Geschwister Schneider USB/CAN devices and bytewerk.org candleLight USB CAN interfaces. These devices all apear to be based on the MCP2518 series devices from Microchip. This is a simple attempt at getting a sockets interface working using libusb in user space on FreeBSD and CheriBSD.

To use it simple execute the application will your chosen CAN speed, the connect to port 2303 (Note: port can be changed, see Usage instructions)

Important Note: CheriBSD 22.12 and earlier have a bug in the USB driver that prevents libusb from working (see [here](https://github.com/CTSRD-CHERI/cheribsd/issues/1616)). It has been fixed and the fix will be available in the next release. You can build from the source to work around the issue (or ask on the Slack channel for someone to send you a build).

# Usage
From shell:
```
usb2can <//s[rate]/p[nnnn]>

Where:
  s[rate] = Chosen bitrate. Defaults to 500k.
            Can choose from: 20k, 33.33k, 40k, 50k, 66.66k,
            80k, 83.33k, 100k, 125k, 200k, 250k, 400k, 500k,
            666k, 800k, 1m
  ? = Print help message
  p[nnnn] = Change the service's port number. Defaults to 2303.
```

# Message protocol
The same format is used to for messages sent and received.
```
IIIIIIII LL DDDDDDDDDDDDDDDD[CR][LF]
Where:
  IIIIIIII = Message ID in Hex. Any message Id larger than 1023 
             will automatically become a 29-bit message ID.
             Note: You can force a lower value ID to use the 
             29-bit frame by setting the Most Significant Bit. 
             e.g. 80000001 would be seen as a 00000001 instead of 001
  LL = Data length code. The length of the data beening received or transmitted.
  DD == The data in ASCII Coded Hex. Two characters per octet. You should only needed octets.
  [CR] = Carridge Return
  [LF] = Line Feed
```

#Improvements To Be Made
1. Change protocol to be more similar to SocketCAN.
2. Add support for multiple devices.
3. Add support for CAN-FD frames.
4. If attempting to transmit multiple frame, it is possible that the current code will miss the extra messages if any of the frames get concatenated.
5. Currently, using synchronus libusb calls as the libusb file handlers weren't triggering. We need to look into this further.
6. Range checking the various inputs.
7. Ability to return error statuses from the CAN bus (may be possible).
