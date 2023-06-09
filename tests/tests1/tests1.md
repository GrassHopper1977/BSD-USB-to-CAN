# Test 1
This was a simple test to check that we can reliably write to and read from the CAN bus. This was tested with CheriBSD V22.12 (Dated 15th December 2022). We used a special kernel that was built for us by Jessica Clarke to work around teh USB bug that was present in the default kernels. There have been newer releases of V22.12 with the same version number but a different date (CheriBSD has inherited FreeBSD's confusing VCS organisational structure!) that include patches to fix the error. We only found out recently that there were multiple versions of V22.12 (we said it was confusing!). We will rerun these tests with this latest build once we've set up our second machine with teh latest version.

## Method
1. We used a PCAN-USB Pro (dual channel USB to CAN device connected to a Windows PC running PCAN-View.
2. We opened two copies of PCAN-View, one for each channel of the device, and wired the device such that the two channels were connected together.
3. We opened both CAN interfaces for standard CAN at 500kbps.
4. We transmitted message ID 100 from the first channel, and message ID 101 from the second channel.
![PCAN-View](https://user-images.githubusercontent.com/52569451/236238165-99bfd3b2-253e-4175-9969-3175e01b3302.png)
5. We ensured that we could see the messages from the otehr channel appearing on our copies PCAN-View. This ensured that the connecvtion was working.
6. The CAN bus load remained under 7%.
7. We then connected a candleLight (open source USB to CAN device) to our Morello Cheri board.
![PXL_20230504_113814102](https://user-images.githubusercontent.com/52569451/236237441-d0419ee4-b076-4b0b-bdea-8efd9306187c.jpg)
8. We started recording the CAN data on one of the PCAN-View instances on the Windows machine.
9. We ran the test program `testtx` on Cheri BSD on our Morello Cheri board running the modified Pure Caps Kernel.
10. When the program had finished we saved the ouytput from PCAN-view and the we saved the output from `testtx`.
11. We filtered the output from PCAN-View to show only the messages from  `testtx` (only message ID 001).


## Attached Files
test1.trc - Output from PCAN-View showing all messages on the bus during the test on the Pure Caps kernel.
test1-filtered.trc - Same as above but filtered to only show message ID 001.
Test1.txt - Output from the `testtx` program on the Pure Caps kernel.
Test1-filtered.txt - Same as above but removing unrequired data.

## Immediate Observations
1. The `testtx` program should only send 270 message and you can see that the contents of byte 0 is incremented everytime. We noticed that we sent 429 messages.
2. The additional messages was due to the fact that we did not always receive every message that we had transmitted. Therefore our code's built in re-transmission ability sent the messages again until they were acknowledged. This could be an issue as we've noticed that most of those that we failed to recieve were actually transmitted.
3. Occasionally, we were receiving `LIBUSB_ERROR_PIPE` when reading from the device. These don't correspond to the missing receives in any way that we could see. It's possible that we were just querying the USB device too quickly.
4. The unmodified kernels did not work in either Pure Caps or Hybrid build. We will need to retry the experiment on the latest version of V22.12.

## Analysis of Results
We created a MS Excel Spreadsheet (test1.xlsx - attached) to analyse our results. 
* Once the messages are written to the CAN Bus the average time before reading the messages back in is 1.64ms (maximum 2.65ms, minimum 0.12ms). This is very very indeed, it is about as fast as you can transmit on a bus of 500kbps. I suspect that we won't be able to see much difference with any other kernel (or even OS).
* The previous figure excludes the transmissions that we never had echoed back to us. 37.06% of our transmissions weren't echoed back to us so we'll need to see if we can fix that error. It's possible that we're not reading fast enough to capture them all.
