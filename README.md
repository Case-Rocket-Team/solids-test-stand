# solids-test-stand
Code for Solid Propulsions R&amp;D's thrust test stand

**Serial Ports**

Set `PORT` in `thrust_test_stand.py` to your Serial path for the LoRa serial module (usually something like `COM3` for Windows or  `/dev/cu.usbmodem2101` for Mac/Linux)

**Serial data transmission**

Set `TRANSMIT_DATA` in `thrust_test_stand.py` to `True` to transmit the full
data stream to the computer, or `False` to transmit commands and status
messages only. The test stand continues saving data to its SD card in either
mode.



***FAQ***

**Error: LoRa Module not working and has Blue LED on**

This means the LoRa is stuck in bootloader mode. You will need to reflash the firmware following the steps here: https://docs.sparkfun.com/SparkFun_LoRaSerial/firmware_update/
