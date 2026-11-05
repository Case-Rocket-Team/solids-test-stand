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
const int PRESS_PIN =
const unsigned long armTimeout = 5;
const unsigned long logTimeout = 2;
const unsigned long fireDelay = 1000; //delay between logging start and fire
const unsigned long fireDuration = 100; //time that fire is pulled high
const int analogResolutionBits = 14; //change to 10 for non-R4 boards
const int sampling_freq = 200; //Hz

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
bool armed = false;
bool fired = false;

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
    
    serialHandshake();
}

void loop() {
    
    digitalWrite(FIRE, LOW);

    if(loggingData && (millis()-logBeginTime) > fireDelay){
        digitalWrite(ARM2, LOW);
        digitalWrite(FIRE, HIGH);
        printINFO("FIRED");
        fired = true;
        fireTime = millis();
    }

    if(fired && (millis()- fireTime) > fireDuration){
        digitalWrite(FIRE, LOW);
        printINFO("Finished Firing");
    }

    // Add code to stop logging after done launching or smth
    if(loggingData){
        if((millis() - logBeginTime)/60000 > logTimeout){
            flushDataBuffer();
            if(dataFile){
                dataFile.close();
            }
            loggingData = false;
        }
        else if ((millis() - lastLogMillis) >= (1000/sampling_freq))
        {
            // Record data and write etc
            data = generateDataPacket();
            lastLogMillis = millis();
            writeData2Serial(data);
            writeData2Card(data);
        }
    }
    if(armed == true && (millis() - armedTime)/60000 > armTimeout) {
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
                filename = String("firedata_") + String((unsigned long)now()) + ".csv";
                dataFile = SD.open(filename, FILE_WRITE);
                if (!dataFile) {
                    printINFO("Failed to open log file: " + filename);
                    loggingData = false;
                } else {
                    logBeginTime = millis();
                    loggingData = true;
                    armed = false;
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

String generateDataPacket(){ 
    String time = String((unsigned long)now());
    String pressure = String(readPressure());
    String thrust = String(loadCell.get_units());
    
    return String(time + "," + thrust + "," + pressure);
}

void writeData2Serial(String data){
    Serial1.println("[D]" + data);
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
                String timestampStr = message.substring(10);
                time_t pctime = timestampStr.toInt();

                if(pctime>0){
                    setTime(pctime);
                    isSynced = true;
                    sendREQUEST("TIME_ACK:" + String((unsigned long)now()));
                }
            }
        }
        delay(2000);
    }

}