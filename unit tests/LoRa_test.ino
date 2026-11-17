#include <TimeLib.h>

/*
  LoRa Unit Test & Throughput Benchmark (Arduino)
  Protocols matched to main script:
  [I] msg \n (Info/Status)
  [D] data \n (Streaming Data)
  [C] rqst \n (Commands / Handshake)
*/

const unsigned long maxLogFileIndex = 9999999UL;
unsigned long logBeginTime = 0;
unsigned long logBeginEpoch = 0;
bool streamingData = false;
unsigned long totalPacketsSent = 0;

void setup() {
    Serial.begin(9600);    // Debug monitor 
    Serial1.begin(57600);  // LoRa module 
    
    printINFO("LoRa Unit Test Initialized. Waiting for Handshake...");
    serialHandshake();
}

void loop() {
    // Check for incoming commands from PC over LoRa
    if (Serial1.available() > 0) {
        String command = Serial1.readStringUntil('\n');
        command.trim();

        if (command == "START_STREAM") {
            streamingData = true;
            totalPacketsSent = 0;
            logBeginTime = millis();
            logBeginEpoch = (unsigned long)now();
            printINFO("LoRa Data Streaming Started");
        } 
        else if (command == "STOP_STREAM") {
            streamingData = false;
            printINFO("LoRa Data Streaming Stopped. Total Packets: " + String(totalPacketsSent));
        }
        else if (command.startsWith("PING:")) {
            // Echo back for round-trip latency testing
            String echoPayload = command.substring(5);
            Serial1.println("[C]PONG:" + echoPayload);
        }
    }

    // If streaming is active, push packets down the LoRa link as fast as possible
    if (streamingData) {
        String packet = generateDataPacket();
        Serial1.println("[D]" + packet);
        totalPacketsSent++;
    }
}

String generateDataPacket() {   
    unsigned long elapsedMillis = millis() - logBeginTime;
    unsigned long sampleSeconds = logBeginEpoch + elapsedMillis / 1000UL;
    unsigned int sampleMilliseconds = elapsedMillis % 1000UL;
    
    // Construct timestamp
    String timeStr = String(sampleSeconds) + ".";
    if (sampleMilliseconds < 100) timeStr += "0";
    if (sampleMilliseconds < 10) timeStr += "0";
    timeStr += String(sampleMilliseconds);

    // Simulated sensor packet with a packet counter for drop-rate tracking
    float mockPressure = 14.7 + random(0, 100) / 10.0;
    float mockThrust = 500.0 + random(0, 500) / 10.0;

    return timeStr + "," + String(mockThrust) + "," + String(mockPressure) + "," + String(totalPacketsSent);
}

void serialHandshake() {
    bool isSynced = false;
    while (!isSynced) {
        sendREQUEST("TIME_REQ");
        if (Serial1.available() > 0) {
            String message = Serial1.readStringUntil('\n');
            message.trim();
            if (message.startsWith("TIME_SYNC:")) {
                String syncPayload = message.substring(10);
                int modeSeparator = syncPayload.indexOf(':');
                String timestampStr = modeSeparator == -1 ? syncPayload : syncPayload.substring(0, modeSeparator);
                time_t pctime = timestampStr.toInt();

                if (pctime > 0) {
                    setTime(pctime);
                    isSynced = true;
                    sendREQUEST("TIME_ACK:" + String((unsigned long)now()));
                    printINFO("Handshake Successful with PC!");
                }
            }
        }
        delay(1000);
    }
}

void printINFO(String msg) {
    Serial1.println("[I]" + msg);
    Serial.println("[I]" + msg);
}

void sendREQUEST(String rqst) {
    Serial1.println("[C]" + rqst);
}