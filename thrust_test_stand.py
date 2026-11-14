import serial
import time
from enum import StrEnum

PORT = 'COM3'  # Replace with arduino port
BAUD_RATE = 57600
TRANSMIT_DATA = False  # Set to False to send commands/status only over serial.

class MsgType(StrEnum):
    INFO = "[I]"
    DATA = "[D]"
    RQST = "[C]"

def cleanMessage(message):
    message = message.strip()
    prefix = message[:3]
    try:
        msg_type = MsgType(prefix)
        content = message[3:].strip()
        return (content, msg_type)
    except ValueError:
        return (message, None)

def logMessage(log_file, direction, raw_message, clean_msg = True):
    """
    Logs a message to the log file and prints it to the console.
    Direction: "TX" for transmitted messages, "RX" for received messages, "INFO" for informational messages.
    """
    if not raw_message:
        return
    if clean_msg:
        message, msg_type = cleanMessage(raw_message)
        if msg_type == MsgType.DATA:
            return
    else:
        message = raw_message
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    log_file.write(f"[{timestamp}] {direction}: {message}\n")
    log_file.flush()
    print(f"[{timestamp}] {direction}: {message}")

def sendSerialMessage(serial_com, log_file, message):
    serial_com.write((message + "\n").encode('ascii'))
    serial_com.flush()
    logMessage(log_file, "TX", message)

def receiveSerialMessage(serial_com, log_file):
    line = serial_com.readline()
    if not line:
        return (None, None)
    raw_message = line.decode('utf-8', errors='ignore').strip()
    logMessage(log_file, "RX", raw_message)
    message, msg_type = cleanMessage(raw_message)
    return (message, msg_type)  

def serialHandshake(serial_com, log_file):
    timestamp = None
    while True:
        print("Attempting Handshake")
        message, msgtype = receiveSerialMessage(serial_com, log_file)
        if msgtype == MsgType.RQST and message == "TIME_REQ":
            timestamp = int(time.time())
            transmission_mode = "FULL" if TRANSMIT_DATA else "COMMANDS"
            sync_message = f"TIME_SYNC:{timestamp}:{transmission_mode}"
            sendSerialMessage(serial_com, log_file, sync_message)

        if msgtype == MsgType.RQST and message.startswith("TIME_ACK:"):
            arduino_timestamp = int(message.split(":")[1])
            if arduino_timestamp >= (timestamp or 0):
                return arduino_timestamp
            else:
                print(f"Arduino timestamp {arduino_timestamp} is less than PC timestamp {timestamp}. Sync failed.")
                return arduino_timestamp
                #return None
        time.sleep(1)

def recordData(serial_com, log_file):
    data_file = None
    data_file_name = None
    if TRANSMIT_DATA:
        data_file_name = "testfire_data_" + time.strftime("%Y%m%d_%H%M%S") + ".csv"
        data_file = open(data_file_name, "a")

    try:
        while True:
            message, msgtype = receiveSerialMessage(serial_com, log_file)
            if msgtype == MsgType.DATA:
                if data_file is not None:
                    data_file.write(message + "\n")
                continue
            if msgtype == MsgType.INFO and message == "Finished Logging Data":
                if data_file_name is not None:
                    print(f"Data recording complete. Data saved to {data_file_name}.")
                else:
                    print("Logging complete. Data transmission was disabled; data remains on the SD card.")
                return
    finally:
        if data_file is not None:
            data_file.close()

def checkResponse(serial_com, log_file, command):
    """
    Logs the response after sending a command, true/false only used for firing right now
    """
    message = ""
    while True:#message != None: Change
        message, msgtype = receiveSerialMessage(serial_com, log_file)
        if msgtype == MsgType.INFO:
            match command:
                case "arm" | "a":
                    if message == "ARMED":
                        return True
                case "fire" | "f": 
                    if message == 'Logging started':
                        return True
                case "disarm" | "d":
                    if message == "DISARMED":
                        return True
    return False


def sendCommands(serial_com, log_file):
    while True:
        command = input("Enter command: Arm, Fire, Disarm, Exit (a/f/d/e) ").lower().strip()
        msg = ""
        logmsg = ""
        match command:
            case "exit" | "e":
                logmsg = "Exiting program."
                logMessage(log_file, "INFO", logmsg, clean_msg=False)
                exit(1)
            case "arm" | "a":
                msg = "ARM"
                logmsg = "Arming System."
                sendSerialMessage(serial_com, log_file, msg)
                logMessage(log_file, "INFO", logmsg, clean_msg=False)
                checkResponse(serial_com, log_file, command)
            case "fire" | "f":
                msg = "FIRE"
                logmsg = "Firing System."
                sendSerialMessage(serial_com, log_file, msg)
                logMessage(log_file, "INFO", logmsg, clean_msg=False)
                if (result := checkResponse(serial_com, log_file, command)):
                    recordData(serial_com, log_file)
                print(result)
            case "disarm" | "d":
                msg = "DISARM"
                logmsg = "Disarming System."
                sendSerialMessage(serial_com, log_file, "DISARM")
                logMessage(log_file, "INFO", logmsg, clean_msg=False)
                checkResponse(serial_com, log_file, command)
            case _:
                logmsg = f"Invalid Command: {command}. Please enter a valid command."
                logMessage(log_file, "INFO", logmsg, clean_msg=False)
            
if __name__ == "__main__":
    log_file_name = "testfire_msglog_" + time.strftime("%Y%m%d_%H%M%S") + ".txt"
    print("Made it 1")
    with open(log_file_name, "a") as log_file:
        ser1 = serial.Serial(PORT, BAUD_RATE, timeout=1)
        ser1.reset_input_buffer()
        ser1.reset_output_buffer()

        print("Made it 2")

        if not (arduino_timestamp := serialHandshake(ser1, log_file)):
            logMessage(log_file, "INFO", "Handshake failed. Exiting.", clean_msg=False)
            exit(1)

        logMessage(log_file, "INFO", f"Handshake successful. Arduino timestamp: {arduino_timestamp}", clean_msg=False)
        sendCommands(ser1, log_file)