#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <HX711.h>


/*
[I]msg\n 
[D]data\n
[C]rqst\n
*/

const int ARM2 = 7;
const int FIRE = 6;
const int SD_CS = 8;
const int LOAD_DOUT = 9;
const int LOAD_SCK = 10;
const float calibration_factor = -7050.0; //change to whatever calibration factor of load cell is
const int PRESS_PIN = A0;
const unsigned long armTimeout = 5.0;
const unsigned long logTimeout = 0.5;
const unsigned long fireDelay = 1000; //delay between logging start and fire
const unsigned long fireDuration = 100; //time that fire is pulled high
const int analogResolutionBits = 14; //change to 10 for non-R4 boards
const int sampling_freq = 50; //Hz
const unsigned long maxLogFileIndex = 9999999UL;

String filename; 
File dataFile;  
String dataBuffer;
String data;

bool loggingData = false;
unsigned long lastLogMillis = 0; //when was data last logged
unsigned long armedTime;
unsigned long fireTime;
unsigned long fireElapsedTime;
unsigned long elapsedTime;
unsigned long logBeginTime;
unsigned long logBeginEpoch;
bool armed = false;
bool fired = false;
bool finishfire = false;
bool transmitData = true;

HX711 loadCell;

void setup(){
    Serial.begin(9600);
    Serial1.begin(57600);
    analogReadResolution(analogResolutionBits);
    if(!SD.begin(SD_CS)){
        printINFO("SD Intialization failed. Ensure card is inserted and wiring is correct. Reset board to clear this message.");
    }
    dataBuffer.reserve(1024);
    loadCell.begin(LOAD_DOUT, LOAD_SCK);
    loadCell.set_scale(calibration_factor);
    loadCell.tare();
    pinMode(ARM2, OUTPUT);
    pinMode(FIRE, OUTPUT);
    digitalWrite(ARM2, LOW);
    digitalWrite(FIRE, LOW);
    
    printINFO("Successfully Initialized");

    serialHandshake();
}

void loop() {
    

    if(loggingData && (millis()-logBeginTime) > fireDelay && fired == false){
        digitalWrite(ARM2, LOW);
        digitalWrite(FIRE, HIGH);
        printINFO("FIRED");
        fired = true;
        finishfire = true;
        fireTime = millis();
    } else if(!loggingData){
        digitalWrite(FIRE, LOW);
    }

    if(fired && (millis()- fireTime) > fireDuration && finishfire){
        digitalWrite(FIRE, LOW);
        printINFO("Finished Firing");
        finishfire = false;
    }

    // Add code to stop logging after done launching or smth
    if(loggingData){
        if(float((millis() - logBeginTime)/60000) > logTimeout){
            flushDataBuffer();
            if(dataFile){
                dataFile.close();
            }
            loggingData = false;
            fired = false;
            printINFO("Finished Logging Data");
        }
        else if ((millis() - lastLogMillis) >= (1000/sampling_freq))
        {
            // Record data and write etc
            data = generateDataPacket();
            lastLogMillis = millis();
            if (transmitData) {
                writeData2Serial(data);
            }
            writeData2Card(data);
        }
    }
    if(armed == true && float((millis() - armedTime)/60000) > armTimeout) {
        armed = false;
        digitalWrite(ARM2, LOW);
        printINFO("DISARMED: Arm timed out after " + String(armTimeout) + " minutes");
    }
    if(!loggingData && Serial1.available() > 0) {
        String command = Serial1.readStringUntil('\n');
        if(command == "ARM") {
            armed = true;
            digitalWrite(ARM2, HIGH);
            armedTime = millis(); 
            printINFO("ARMED");
        } else if(command == "FIRE") {
            if (armed) {
                filename = createUniqueFilename();
                if (filename.length() == 0) {
                    printINFO("Failed to create a unique log filename");
                } else {
                    dataFile = SD.open(filename.c_str(), FILE_WRITE);
                    if (!dataFile) {
                        printINFO("Failed to open log file: " + filename);
                        loggingData = false;
                    } else {
                        logBeginTime = millis();
                        logBeginEpoch = (unsigned long)now();
                        loggingData = true;
                        armed = false;
                        printINFO("Logging started");
                    }
                } 
            } else {
                printINFO("Cannot fire: Arm 2 is not activated");
            }
        } else if(command == "DISARM") {
            armed = false;
            digitalWrite(ARM2, LOW);
            printINFO("DISARMED");
        } else {
            printINFO("Unknown command received");
        }
    }
}

String createUniqueFilename() {
    unsigned long candidate = (unsigned long)now() % maxLogFileIndex + 1;
    for (unsigned long attempts = 0; attempts < maxLogFileIndex; attempts++) {
        String number = String(candidate);
        while (number.length() < 7) {
            number = "0" + number;
        }

        String candidateName = "F" + number + ".CSV";
        if (!SD.exists(candidateName.c_str())) {
            return candidateName;
        }
        candidate = candidate == maxLogFileIndex ? 1 : candidate + 1;
    }
    return "";
}

String generateDataPacket(){ 
    unsigned long elapsedMillis = millis() - logBeginTime;
    unsigned long sampleSeconds = logBeginEpoch + elapsedMillis / 1000UL;
    unsigned int sampleMilliseconds = elapsedMillis % 1000UL;
    String time = String(sampleSeconds) + ".";
    if (sampleMilliseconds < 100) {
        time += "0";
    }
    if (sampleMilliseconds < 10) {
        time += "0";
    }
    time += String(sampleMilliseconds);
    String pressure = String(readPressure());
    String thrust = String(loadCell.get_units());
    
    return String(time + "," + thrust + "," + pressure +"\n");
}

void writeData2Serial(String data){
    Serial1.println("[D]" + data);
    Serial.println("[D]" + data);
}

void writeData2Card(String data){
    if (!dataFile) {
        return;
    }

    dataBuffer += data;
    const size_t chunkSize = 512;
    while (dataBuffer.length() >= chunkSize) {
        dataFile.write(dataBuffer.c_str(), chunkSize);
        dataBuffer.remove(0, chunkSize);
    }
}

void flushDataBuffer() {
    if (!dataFile || dataBuffer.length() == 0) {
        return;
    }
    dataFile.write(dataBuffer.c_str(), dataBuffer.length());
    dataBuffer = "";
}

void printINFO(String msg){
    msg = "[I]" + msg;
    Serial.println(msg);
    Serial1.println(msg);
}

void sendREQUEST(String rqst){
    rqst = "[C]" + rqst;
    Serial1.println(rqst);
}

float readPressure(){
    float analogPressure = analogRead(PRESS_PIN);
    float pressureVolts = (analogPressure * 5.0)/(pow(2, analogResolutionBits)); // Make sure calibrated correctly
    if(pressureVolts<0.5){
        return 0.0;
    } else{
        return (pressureVolts - 0.5) * 400;
    }
}

void serialHandshake(){
    bool isSynced = false;
    while(!isSynced){
        sendREQUEST("TIME_REQ");
        if(Serial1.available() > 0){
            String message = Serial1.readStringUntil('\n');
            if(message.startsWith("TIME_SYNC:")){
                String syncPayload = message.substring(10);
                int modeSeparator = syncPayload.indexOf(':');
                String timestampStr = modeSeparator == -1
                    ? syncPayload
                    : syncPayload.substring(0, modeSeparator);
                String transmissionMode = modeSeparator == -1
                    ? "FULL"
                    : syncPayload.substring(modeSeparator + 1);
                time_t pctime = timestampStr.toInt();

                if (transmissionMode != "FULL" && transmissionMode != "COMMANDS") {
                    printINFO("Invalid data transmission mode");
                } else if(pctime>0){
                    transmitData = transmissionMode == "FULL";
                    setTime(pctime);
                    isSynced = true;
                    sendREQUEST("TIME_ACK:" + String((unsigned long)now()));
                }
            }
        }
        delay(2000);
    }

}