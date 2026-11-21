import serial
import time

PORT = 'COM3'  # Replace with arduino port
BAUD_RATE = 57600


ser1 = serial.Serial(PORT, BAUD_RATE, timeout=1)

while(True):
    line = ser1.readline()
    if line:
        raw_message = line.decode('utf-8', errors='ignore').strip()
        print(raw_message)
    time.sleep(0.5)