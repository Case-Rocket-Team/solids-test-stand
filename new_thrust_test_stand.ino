#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <HX711.h>

/*
[I] Informational/status message
[D] CSV data packet
[C] Request message
*/

const int ARM_RELAY_PIN = 7;
const int FIRE_RELAY_PIN = 6;
const int SD_CS_PIN = 8;
const int LOAD_DOUT_PIN = 9;
const int LOAD_SCK_PIN = 10;
const int PRESSURE_PIN = A0;

const float LOAD_CALIBRATION_FACTOR = 2179.0;

const unsigned long ARM_TIMEOUT_MS = 5UL * 60UL * 1000UL; // minutes 
const unsigned long LOG_TIMEOUT_MS = 30UL * 1000UL; // seconds
const unsigned long FIRE_DELAY_MS = 1UL * 1000UL; // ms
const unsigned long FIRE_DURATION_MS = 1UL * 100UL; //ms
const unsigned long SAMPLE_FREQ = 200 // Hz
const unsigned long SAMPLE_INTERVAL_MS = 1000UL / SAMPLE_FREQ_HZ;

const int ANALOG_RESOLUTION_BITS = 14;
const unsigned long MAX_LOG_FILE_INDEX = 9999999UL;

const int ARM_RELAY_ACTIVE = LOW;
const int ARM_RELAY_INACTIVE = HIGH;
const int FIRE_RELAY_ACTIVE = LOW;
const int FIRE_RELAY_INACTIVE = HIGH;

String filename;
File dataFile;
String dataBuffer;
String data;

bool loggingData = false;
bool transmitData = false;
bool armed = false;
bool fired = false;
bool finishFire = false;

unsigned long armedTime = 0;
unsigned long fireTime = 0;
unsigned long lastLogMillis = 0;
unsigned long logBeginMillis = 0;
unsigned long logBeginEpoch = 0;

HX711 loadCell;

void setup()
{
    Serial.begin(9600);
    Serial1.begin(57600);

    analogReadResolution(ANALOG_RESOLUTION_BITS);

    if (!SD.begin(SD_CS_PIN)){
        printInfo("SD initialization failed. Ensure the card is inserted and wiring is correct.");
    }

    dataBuffer.reserve(1024);

    loadCell.begin(LOAD_DOUT_PIN, LOAD_SCK_PIN);
    loadCell.set_scale(LOAD_CALIBRATION_FACTOR);
    loadCell.tare();

    pinMode(ARM_RELAY_PIN, OUTPUT);
    pinMode(FIRE_RELAY_PIN, OUTPUT);

    digitalWrite(ARM_RELAY_PIN, ARM_RELAY_INACTIVE);
    digitalWrite(FIRE_RELAY_PIN, FIRE_RELAY_INACTIVE);

    delay(1000);

    printInfo("Successfully Initialized");

    serialHandshake();
}

void loop()
{
    unsigned long currentMillis = millis();

    // Activate the firing relay one second after logging begins.
    if (loggingData && (currentMillis - logBeginMillis >= FIRE_DELAY_MS) && !fired)
    {
        digitalWrite(FIRE_RELAY_PIN, FIRE_RELAY_ACTIVE);
        printInfo("FIRED");

        fired = true;
        finishFire = true;
        fireTime = currentMillis;
    }
    else if (!loggingData)
    {
        digitalWrite(FIRE_RELAY_PIN, FIRE_RELAY_INACTIVE);
    }

    // Release the firing relay after the configured duration.
    if (fired && finishFire && (currentMillis - fireTime >= FIRE_DURATION_MS)){
        digitalWrite(FIRE_RELAY_PIN, FIRE_RELAY_INACTIVE);
        printInfo("Finished Firing");
        finishFire = false;
    }

    // Continue sampling until the configured logging duration expires.
    if (loggingData)
    {
        if (currentMillis - logBeginMillis >= LOG_TIMEOUT_MS)
        {
            flushDataBuffer();
            if (dataFile)
            {
                dataFile.flush();
                dataFile.close();
            }
            loggingData = false;
            fired = false;
            finishFire = false;
            digitalWrite(FIRE_RELAY_PIN, FIRE_RELAY_INACTIVE);
            printInfo("Finished Logging Data");
        }
        else if (currentMillis - lastLogMillis >= SAMPLE_INTERVAL_MS)
        {
            data = generateDataPacket();
            lastLogMillis = currentMillis;
            if (transmitData)
            {
                writeDataToSerial(data);
            }
            writeDataToCard(data);
        }
    }

    // Disarm automatically after the arm timeout.
    if (armed && (currentMillis - armedTime >= ARM_TIMEOUT_MS))
    {
        armed = false;
        digitalWrite(ARM_RELAY_PIN, ARM_RELAY_INACTIVE);
        printInfo("DISARMED: Arm timed out after " + String(ARM_TIMEOUT_MS / 60000UL) + " minutes");
    }

    // Commands/status remain active, but transmitted data is disabled
    // unless the handshake selects FULL mode.
    if (!loggingData && Serial1.available() > 0)
    {
        String command = Serial1.readStringUntil('\n');
        command.trim();

        if (command == "ARM")
        {
            armed = true;
            armedTime = millis();

            digitalWrite(ARM_RELAY_PIN, ARM_RELAY_ACTIVE);
            printInfo("ARMED");
        }
        else if (command == "FIRE")
        {
            if (!armed){
                printInfo("Cannot fire: Arm 2 is not activated");
            }
            else{
                filename = createUniqueFilename();

                if (filename.length() == 0){
                    printInfo("Failed to create a unique log filename");
                }
                else{
                    dataFile = SD.open(filename.c_str(), FILE_WRITE);

                    if (!dataFile){
                        printInfo("Failed to open log file: " + filename);
                        loggingData = false;
                        armed = false;
                    } else {
                        logBeginMillis = millis();
                        logBeginEpoch = static_cast<unsigned long>(now());
                        lastLogMillis = 0;
                        loggingData = true;
                        armed = false;
                        digitalWrite(ARM_RELAY_PIN, ARM_RELAY_INACTIVE);
                        printInfo("Logging started");
                    }
                }
            }
        } else if (command == "DISARM") {
            armed = false;
            digitalWrite(ARM_RELAY_PIN, ARM_RELAY_INACTIVE);
            printInfo("DISARMED");
        } else {
            printInfo("Unknown command received");
        }
    }
}

String createUniqueFilename()
{
    unsigned long candidate = (static_cast<unsigned long>(now()) % MAX_LOG_FILE_INDEX) + 1;

    for (unsigned long attempt = 0; attempt < MAX_LOG_FILE_INDEX; attempt++) {
        String number = String(candidate);
        while (number.length() < 7){
            number = "0" + number;
        }
        String candidateName = "F" + number + ".CSV";
        if (!SD.exists(candidateName.c_str())){
            return candidateName;
        }
        candidate = candidate == MAX_LOG_FILE_INDEX ? 1 : candidate + 1;
    }
    return "";
}

String generateDataPacket()
{
    unsigned long elapsedMillis = millis() - logBeginMillis;
    unsigned long sampleSeconds = logBeginEpoch + elapsedMillis / 1000UL;

    unsigned int sampleMilliseconds = elapsedMillis % 1000UL;

    String timestamp = String(sampleSeconds) + ".";

    if (sampleMilliseconds < 100){
        timestamp += "0";
    }

    if (sampleMilliseconds < 10){
        timestamp += "0";
    }

    timestamp += String(sampleMilliseconds);

    String pressure = String(readPressure());
    String thrust = String(loadCell.get_units());

    return timestamp + "," + thrust + "," + pressure + "\n";
}

void writeDataToSerial(String packet){
    Serial1.println("[D]" + packet);
}

void writeDataToCard(String packet)
{
    if (!dataFile){
        return;
    }

    dataBuffer += packet;

    const size_t chunkSize = 512;

    while (dataBuffer.length() >= chunkSize){
        dataFile.write(dataBuffer.c_str(), chunkSize);
        dataBuffer.remove(0, chunkSize);
    }
}

void flushDataBuffer()
{
    if (!dataFile){
        return;
    }

    if (dataBuffer.length() > 0){
        dataFile.write(dataBuffer.c_str(), dataBuffer.length());
        dataBuffer = "";
    }

    dataFile.flush();
}

void printInfo(String message)
{
    String formattedMessage = "[I]" + message;

    Serial.println(formattedMessage);
    Serial1.println(formattedMessage);
}

void sendRequest(String request)
{
    Serial1.println("[C]" + request);
}

float readPressure()
{
    float analogPressure = analogRead(PRESSURE_PIN);

    float pressureVolts = (analogPressure * 5.0) / static_cast<float>(pow(2, ANALOG_RESOLUTION_BITS));

    if (pressureVolts < 0.5){
        return 0.0;
    }

    return (pressureVolts - 0.5) * 400.0;
}

void serialHandshake()
{
    bool isSynced = false;

    while (!isSynced)
    {
        sendRequest("TIME_REQ");

        if (Serial1.available() > 0)
        {
            String message = Serial1.readStringUntil('\n');
            message.trim();

            if (message.startsWith("TIME_SYNC:"))
            {
                String syncPayload = message.substring(10);
                int modeSeparator = syncPayload.indexOf(':');

                String timestampString =
                    modeSeparator == -1
                        ? syncPayload
                        : syncPayload.substring(0, modeSeparator);

                String transmissionMode =
                    modeSeparator == -1
                        ? "FULL"
                        : syncPayload.substring(modeSeparator + 1);

                transmissionMode.trim();

                time_t pcTime = timestampString.toInt();

                if (transmissionMode != "FULL" &&
                    transmissionMode != "COMMANDS")
                {
                    printInfo("Invalid data transmission mode");
                }
                else if (pcTime > 0)
                {
                    // This should normally be false because the Python
                    // program sends COMMANDS while transmit is disabled.
                    transmitData = transmissionMode == "FULL";

                    setTime(pcTime);
                    isSynced = true;

                    sendRequest("TIME_ACK:" + String(static_cast<unsigned long>(now())));
                }
            }
        }

        if (!isSynced){
            delay(2000);
        }
    }
}