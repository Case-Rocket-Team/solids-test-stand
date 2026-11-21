import serial
import time
from enum import StrEnum


PORT = "COM3"
BAUD_RATE = 57600
TRANSMIT_DATA = False
SERIAL_RESPONSE_TIMEOUT_SECONDS = 2 * 60
DATA_RECORD_TIMEOUT_SECONDS = 5 * 60


class MsgType(StrEnum):
    INFO = "[I]"
    DATA = "[D]"
    RQST = "[C]"


def clean_message(message):
    message = message.strip()
    prefix = message[:3]

    try:
        msg_type = MsgType(prefix)
        content = message[3:].strip()
        return content, msg_type
    except ValueError:
        return message, None


def log_message(log_file, direction, raw_message, clean_msg=True):
    if not raw_message:
        return

    if clean_msg:
        message, msg_type = clean_message(raw_message)

        # Data packets are not written to the message log.
        if msg_type == MsgType.DATA:
            return
    else:
        message = raw_message

    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    log_line = f"[{timestamp}] {direction}: {message}\n"

    log_file.write(log_line)
    log_file.flush()
    print(log_line.rstrip())


def send_serial_message(serial_com, log_file, message):
    serial_com.write((message + "\n").encode("ascii"))
    serial_com.flush()
    log_message(log_file, "TX", message)


def receive_serial_message(serial_com, log_file):
    line = serial_com.readline()

    if not line:
        return None, None

    raw_message = line.decode("utf-8", errors="ignore").strip()
    log_message(log_file, "RX", raw_message)
    return clean_message(raw_message)


def serial_handshake(serial_com, log_file, timeout=30):
    timestamp = None
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        print("Attempting Handshake")

        message, msg_type = receive_serial_message(serial_com, log_file)

        if msg_type == MsgType.RQST and message == "TIME_REQ":
            timestamp = int(time.time())
            transmission_mode = "FULL" if TRANSMIT_DATA else "COMMANDS"

            send_serial_message(
                serial_com,
                log_file,
                f"TIME_SYNC:{timestamp}:{transmission_mode}",
            )

        if msg_type == MsgType.RQST and message.startswith("TIME_ACK:"):
            try:
                arduino_timestamp = int(message.split(":", 1)[1])
            except (IndexError, ValueError):
                arduino_timestamp = 0

            if timestamp is not None: # and arduino_timestamp >= timestamp:
                return arduino_timestamp

            log_message(
                log_file,
                "INFO",
                f"Handshake timestamp mismatch: PC={timestamp}, "
                f"Arduino={arduino_timestamp}",
                clean_msg=False,
            )
            return None

        time.sleep(0.2)

    log_message(log_file, "INFO", "Handshake timed out.", clean_msg=False)
    return None


def record_data(serial_com, log_file):
    """
    Wait for the stand to finish firing and logging.

    TRANSMIT_DATA=False:
        No PC CSV is created.
        SD logging and all INFO status messages continue normally.

    TRANSMIT_DATA=True:
        Data packets are saved to a PC CSV as they arrive.
    """
    data_file = None
    data_file_name = None
    deadline = time.monotonic() + DATA_RECORD_TIMEOUT_SECONDS

    if TRANSMIT_DATA:
        data_file_name = (
            "testfire_data_" + time.strftime("%Y%m%d_%H%M%S") + ".csv"
        )
        data_file = open(data_file_name, "a", encoding="utf-8")

    try:
        while time.monotonic() < deadline:
            message, msg_type = receive_serial_message(serial_com, log_file)

            if msg_type == MsgType.DATA:
                if data_file is not None:
                    data_file.write(message + "\n")
                    data_file.flush()
                continue

            if msg_type != MsgType.INFO:
                continue

            if message == "FIRED":
                log_message(log_file, "INFO", "Stand reported: firing relay activated.", clean_msg=False)
            elif message == "Finished Firing":
                log_message(log_file, "INFO", "Stand reported: firing relay released.", clean_msg=False)
            elif message == "Finished Logging Data":
                if data_file_name is not None:
                    print(f"Data recording complete. Data saved to {data_file_name}." )
                else:
                    print("Logging complete. Data transmission was disabled; data remains on the SD card." )
                return True

        error = "Timed out waiting for the test stand to finish logging."
        log_message(log_file, "INFO", error, clean_msg=False)
        return False

    finally:
        if data_file is not None:
            data_file.close()


def check_response(serial_com, log_file, command, timeout=SERIAL_RESPONSE_TIMEOUT_SECONDS):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message, msg_type = receive_serial_message(serial_com, log_file)
        if msg_type != MsgType.INFO:
            continue

        if command in ("arm", "a") and message == "ARMED":
            return True

        if command in ("fire", "f"):
            if message == "Logging started":
                return True

            if (message.startswith("Failed to") or message.startswith("Cannot fire:")):
                log_message(log_file, "INFO", f"Command failed: {message}", clean_msg=False)
                return False

        if command in ("disarm", "d") and message == "DISARMED":
            return True

    error = f"Timed out waiting for response to '{command}'."
    log_message(log_file, "INFO", error, clean_msg=False)
    return False


def send_commands(serial_com, log_file):
    while True:
        command = input("Enter command: Arm, Fire, Disarm, Exit (a/f/d/e) ").lower().strip()
        if command in ("exit", "e"):
            log_message(log_file, "INFO", "Exiting program.", clean_msg=False)
            return

        if command in ("arm", "a"):
            send_serial_message(serial_com, log_file, "ARM")
            log_message(log_file, "INFO", "Arming System.", clean_msg=False)
            check_response(serial_com, log_file, command)

        elif command in ("fire", "f"):
            send_serial_message(serial_com, log_file, "FIRE")
            log_message(log_file, "INFO", "Firing System.", clean_msg=False)

            if check_response(serial_com, log_file, command):
                record_data(serial_com, log_file)

        elif command in ("disarm", "d"):
            send_serial_message(serial_com, log_file, "DISARM")
            log_message(log_file, "INFO", "Disarming System.", clean_msg=False)
            check_response(serial_com, log_file, command)

        else:
            log_message(log_file, "INFO", f"Invalid Command: {command}. Please enter a valid command.", clean_msg=False)

if __name__ == "__main__":
    log_file_name = ("testfire_msglog_" + time.strftime("%Y%m%d_%H%M%S") + ".txt")

    with open(log_file_name, "a", encoding="utf-8") as log_file:
        serial_com = None
        try:
            serial_com = serial.Serial(PORT, BAUD_RATE, timeout=1)
            serial_com.reset_input_buffer()
            serial_com.reset_output_buffer()

            arduino_timestamp = serial_handshake(serial_com, log_file)

            if arduino_timestamp is None:
                log_message(log_file, "INFO", "Handshake failed. Exiting.", clean_msg=False)
                raise SystemExit(1)

            log_message( log_file, "INFO", f"Handshake successful. Arduino timestamp: {arduino_timestamp}", clean_msg=False)

            if TRANSMIT_DATA:
                print("Data transmission enabled: FULL mode.")
            else:
                print("Data transmission disabled: COMMANDS mode. SD logging remains active.")

            send_commands(serial_com, log_file)

        except serial.SerialException as exc:
            log_message( log_file, "INFO", f"Serial error: {exc}", clean_msg=False)
            raise SystemExit(1)

        finally:
            if serial_com is not None and serial_com.is_open:
                serial_com.close()