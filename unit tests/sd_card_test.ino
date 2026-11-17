#include <SD.h>
#include <TimeLib.h>

const int SD_CS = 8;
const unsigned long maxLogFileIndex = 9999999UL;
const int test_duration = 1; // minutes
const int sampling_freq = 100; // Hz

unsigned long lastLogMillis = 0;
unsigned long logBeginTime = 0;

bool logging = false;
bool finished_logging = false;
String data;
String filename;
File dataFile;   

String dataBuffer;
const size_t chunkSize = 512;

void setup(){
    Serial.begin(9600);
    while (!Serial);
    
    while(!SD.begin(SD_CS)){
        Serial.println("ERROR: SD Initialization failed.");
        delay(5000);
    }
    dataBuffer.reserve(1024); 
}

void loop(){
    if(!finished_logging){
        if(!logging){
            filename = createUniqueFilename();
            if (filename.length() == 0) {
                Serial.println("ERROR: Failed to create unique filename");
                finished_logging = true;
            } else {
                dataFile = SD.open(filename.c_str(), FILE_WRITE);
                if (!dataFile) {
                    Serial.println("ERROR: Failed to open log file");
                    finished_logging = true;
                } else {
                    logging = true;
                    logBeginTime = millis();
                }  
            }
        } 
        else if (((millis() - logBeginTime) / 60000.0) > test_duration){
            flushDataBuffer(); 
            if(dataFile){
                dataFile.close();
            }
            finished_logging = true;
            Serial.println("SUCCESS: Logged successfully to: " + filename);
        }
        else if ((millis() - lastLogMillis) >= (1000 / sampling_freq)){
            lastLogMillis = millis();
            data = generateDataPacket();
            writeData2Card(data); 
        }
    }
}

void writeData2Card(String data){
    dataBuffer += data;
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

String generateDataPacket() {   
    return String(random(1, 99)) + "," + String(random(1, 99)) + "," + String(random(1, 99)) + "\n";
}