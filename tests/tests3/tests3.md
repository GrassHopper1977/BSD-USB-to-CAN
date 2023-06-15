# Tests 3

The aim of this test was to compare the performance of the USB to CAN module when using the Morello CHERI board in CheriBSD's Hybrid and Full Caps modes.

# Synopsis

The test revealed that, some aspects of the code, ran faster in Full Caps mode than in Hybrid Mode. We analysed the times taken and found that they did not fit a standard distribution. Instead the results seems quite quantised.

# Method

1. We connected the candleLight device to a CAN network with two other nodes (which were only listening). The specific device used is the `candlelight-D01-R01-V01-C00`.
2. We started the USB to CAN code using: `./usb2can d0' or `./usb2can_hy d0'.
3. We then started the test code on another console using: `./test 127.0.0.1 2303` or `./test_hy 127.0.0.1 2303`.
4. We had enabled all the logging information on both sets of code so we were able to trace how long it took each aspect of the code to execute in nanoseconds.
5. The `test` code would generate an 8-byte message (which includes which message number it is as a part of the data). That is sent over the sockets interface to `usb2can` which writes it to the CAN. We then read the message back from the CAN and return the message read back to `test` via teh sockets interface where it was displayed on the screen. Note: We still have a bug where, approximately, half on the messages that we send on teh CAN don't get read back and we're still working on this issue but it may be an issue with the USB device.
6. We've broken the test into 8 stages:
   1. Stage 1 - from `test`: Indicates that we have sent the message to `usb2can` over the socket interfaces.
   2. Stage 2 - from `usb2can`: Indicates that a packet has been received from the socket.
   3. Stage 3 - from `usb2can`: Indicates that the message has been processed.
   4. Stage 4 - from `usb2can`: Indicates that the mesage has been queued for transmission on the USB device
   5. Stage 5 - from `usb2can`: Indicates that the message has been accepted by the USB device for transmission.
   6. Stage 6 - from `usb2can`: Indicates that the message has been received by the USB device and processed by `usb2can` as valid
   7. Stage 7 - from `usb2can`: Indicates that the received message has been transmitted back to `test` via the sockets
   8. Stage 8 - from `test`: Indicates that `test` has received the message from `usb2can` via the sockets interface.
7. We let `test` send over 2500 messages each time to get a good spread of results.
8. 
9. We then created a spread sheet to analyse the time taken between each of these stages.
10. The test was repeated four times using different combinations of kernel and code:
    1. Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
    2. Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
    3. Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
    4. Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
11. The version of CheriBSD that we used was: V22.12 releng V22.12 2023-03-03

# Results
Note: When we're analysing the distributions of times taken, we're skipping those where the time is 0ns as teh charts would be meaningless.

## Analysis of times taken between each stage using Pure Caps and Hybrid code on Pure Caps and Hybrid Kernels
### Comparison of Average Times Between Stages for Each Test

| # | Code Type |  Kernel Type | Stage 1 to 2 (ns) | (ns) Stage 2 to 3 (ns) | (ns) Stage 3 to 4 (ns) | Stage 4 to 5 (ns) | Stage 5 to 6 (ns) | Stage 6 to 7 (ns) | Stage 7 to 8 (ns) | Messages Sent | Messages Received | USB Errors |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Pure Caps  | Pure Caps | 329024.50 | 0.00 | 314.76 | 0.00 | 256150.70 | 49739.66 | 218767.25 | 3177 | 1588 | 142 |
| 2 | Pure Caps  | Hybrid    | 259044.13 | 0.00 | 0.00 | 0.00 | 209490.16 | 21263.34 | 94995.31 | 3008 | 1504 | 141 |
| 3 | Hybrid     | Pure Caps | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0 | 0 | 0 |
| 4 | Hybrid     | Hybrid    | 522734.48 | 0.00 | 707.17 | 0.00 | 541420.69 | 41173.03 | 140934.41 | 2853 | 1427 | 147 |

### Comparison of Minimum Times Between Stages for Each Test

| # | Code Type |  Kernel Type | Stage 1 to 2 (ns) | (ns) Stage 2 to 3 (ns) | (ns) Stage 3 to 4 (ns) | Stage 4 to 5 (ns) | Stage 5 to 6 (ns) | Stage 6 to 7 (ns) | Stage 7 to 8 (ns) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Pure Caps  | Pure Caps | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| 2 | Pure Caps  | Hybrid    | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| 3 | Hybrid     | Pure Caps | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| 4 | Hybrid     | Hybrid    | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |

### Comparison of Maximum Times Between Stages for Each Test

| # | Code Type |  Kernel Type | Stage 1 to 2 (ns) | (ns) Stage 2 to 3 (ns) | (ns) Stage 3 to 4 (ns) | Stage 4 to 5 (ns) | Stage 5 to 6 (ns) | Stage 6 to 7 (ns) | Stage 7 to 8 (ns) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Pure Caps  | Pure Caps | 1124770.00 | 0.00 | 999989.00 | 0.00 | 1003508.00 | 1001729.00 | 1002429.00 |
| 2 | Pure Caps  | Hybrid    | 1004189.00 | 0.00 | 0.00 | 0.00 | 1002029.00 | 1000729.00 | 1000869.00 |
| 3 | Hybrid     | Pure Caps | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| 4 | Hybrid     | Hybrid    | 2000199.00 | 0.00 | 1000449.00 | 0.00 | 1001929.00 | 1000990.00 | 1000870.00 |

### Interpretation of Results

## Analysis on Times Taken from Stage 1 to Stage 2

### Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/8f1ba6e7-a786-49b5-8524-af5643d3551e)

### Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/9ff4eec8-9887-4a17-92f7-b0034eed7a64)

### Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
### Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/aab671a1-8a6e-4837-888b-03b1ec279f55)

## Analysis on Times Taken from Stage 2 to Stage 3
Results are all 0.

## Analysis on Times Taken from Stage 3 to Stage 4

### Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/0f7071b6-cb46-401e-a1d2-2213fe5ad3d6)

### Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
Results are all 0.

### Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
### Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/d3773e99-9b95-4252-86fe-4e2a0606e7fd)

## Analysis on Times Taken from Stage 4 to Stage 5
Results are all 0.

## Analysis on Times Taken from Stage 5 to Stage 6

### Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/d5a762a1-f3ca-4250-ac0f-dd37ef1b25e9)

### Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/087b0732-b74e-4c6b-aa0f-c3d437d4c696)

### Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
### Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/b1911854-af30-4a7a-ade1-52bd34230e16)

## Analysis on Times Taken from Stage 6 to Stage 7

### Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/39cba67b-98b4-4bd6-8d79-f0b85804880f)

### Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/859fb3e8-37fb-4780-b10d-41ddb057210f)

### Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
### Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/b1911854-af30-4a7a-ade1-52bd34230e16)

## Analysis on Times Taken from Stage 7 to Stage 8

### Test 1: Pure Caps code (`test` and `usb2can`) on Pure Caps kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/d4d88271-aca2-4e2a-9493-ad6b3e26b902)

### Test 2: Pure Caps code (`test` and `usb2can`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/04f0020b-926d-45f3-919a-be593de675c8)

### Test 3: Hybrid code (`test_hy` and `usb2can`) on Pure Caps kernel
### Test 4: Hybrid code (`test_hy` and `usb2can_hy`) on Hybrid kernel
![image](https://github.com/GrassHopper1977/BSD-USB-to-CAN/assets/52569451/a3049554-31d9-41d0-90dd-187aefbd7ad3)



