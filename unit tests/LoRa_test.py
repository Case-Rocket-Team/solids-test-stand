import serial
import time
from enum import StrEnum

PORT = 'COM3'  # Match your LoRa radio's COM port on your PC
BAUD_RATE = 57600
SERIAL_RESPONSE_TIMEOUT_SECONDS = 10

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
        return (content, msg_type)
    except ValueError:
        return (message, None)

def send_message(ser, message):
    ser.write((message + "\n").encode('ascii'))
    ser.flush()

def receive_message(ser):
    line = ser.readline()
    if not line:
        return (None, None)
    raw_message = line.decode('utf-8', errors='ignore').strip()
    message, msg_type = clean_message(raw_message)
    return (message, msg_type)

def run_handshake(ser):
    print("Waiting for Arduino handshake request...")
    deadline = time.monotonic() + SERIAL_RESPONSE_TIMEOUT_SECONDS
    
    while time.monotonic() < deadline:
        message, msg_type = receive_message(ser)
        if msg_type == MsgType.RQST and message == "TIME_REQ":
            timestamp = int(time.time())
            sync_msg = f"TIME_SYNC:{timestamp}:FULL"
            send_message(ser, sync_msg)
            print(f"Sent time sync: {timestamp}")
            
        elif msg_type == MsgType.RQST and message.startswith("TIME_ACK:"):
            print("Handshake acknowledged by Arduino!")
            return True
            
        time.sleep(0.1)
    print("Handshake timed out.")
    return False

def test_latency(ser, pings=5):
    print(f"\n--- Testing LoRa Round-Trip Latency ({pings} pings) ---")
    latencies = []
    
    for i in range(pings):
        ping_id = str(i + 1)
        start_time = time.monotonic()
        send_message(ser, f"PING:{ping_id}")
        
        while time.monotonic() - start_time < 2.0:
            msg, msg_type = receive_message(ser)
            if msg_type == MsgType.RQST and msg == f"PONG:{ping_id}":
                rtt = (time.monotonic() - start_time) * 1000.0 # ms
                latencies.append(rtt)
                print(f"Ping {ping_id}: Round-trip time = {rtt:.2f} ms")
                break
        time.sleep(0.5)
        
    if latencies:
        print(f"Average Latency: {sum(latencies)/len(latencies):.2f} ms")
    else:
        print("Latency test failed: No pong responses received.")

def test_throughput(ser, duration_seconds=10):
    print(f"\n--- Testing LoRa Data Stream Throughput ({duration_seconds}s test) ---")
    send_message(ser, "START_STREAM")
    
    packet_count = 0
    byte_count = 0
    dropped_packets = 0
    expected_next_seq = None
    
    start_time = time.monotonic()
    deadline = start_time + duration_seconds
    
    try:
        while time.monotonic() < deadline:
            raw_line = ser.readline()
            if not raw_line:
                continue
            
            raw_str = raw_line.decode('utf-8', errors='ignore').strip()
            message, msg_type = clean_message(raw_str)
            
            if msg_type == MsgType.DATA:
                packet_count += 1
                byte_count += len(raw_str)
                
                # Parse sequence number from the end of the packet format: time,thrust,pressure,seq
                try:
                    parts = message.split(',')
                    seq_num = int(parts[-1])
                    if expected_next_seq is not None and seq_num != expected_next_seq:
                        dropped_packets += (seq_num - expected_next_seq)
                    expected_next_seq = seq_num + 1
                except (ValueError, IndexError):
                    pass
                    
            elif msg_type == MsgType.INFO:
                print(f"[Arduino Info]: {message}")
                
    except KeyboardInterrupt:
        print("\nStream test interrupted by user.")
        
    send_message(ser, "STOP_STREAM")
    
    elapsed = time.monotonic() - start_time
    print("\n==============================")
    print("THROUGHPUT BENCHMARK RESULTS:")
    print(f"Total Duration:     {elapsed:.2f} seconds")
    print(f"Packets Received:   {packet_count}")
    print(f"Data Transferred:   {byte_count / 1024:.2f} KB")
    print(f"Transfer Rate:      {packet_count / elapsed:.2f} packets/sec ({(byte_count / elapsed) / 1024:.2f} KB/s)")
    print(f"Estimated Drops:    {dropped_packets} packets")
    print("==============================")

if __name__ == "__main__":
    with serial.Serial(PORT, BAUD_RATE, timeout=0.5) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        if run_handshake(ser):
            test_latency(ser, pings=5)
            input("\nPress Enter to start the high-speed throughput stream test...")
            test_throughput(ser, duration_seconds=10)
        else:
            print("Exiting due to failed handshake.")