#include <HX711.h>

// ==========================================
// SENSOR TEST CONFIGURATION FLAGS
// Set to true to test, false to skip/ignore
// ==========================================
bool testPressure = true;
bool testLoadCell = true;

const int LOAD_DOUT = 9;
const int LOAD_SCK = 10;
const float calibration_factor = 2179; 
const int PRESS_PIN = A0;
const int analogResolutionBits = 14; // Use 10 for non-R4 boards if needed

const unsigned long sampleInterval = 200; // Time between sensor reads (ms)
unsigned long lastSampleMillis = 0;
bool testStarted = false;

HX711 loadCell;

void setup() {
    Serial.begin(9600);
    while (!Serial);

    analogReadResolution(analogResolutionBits);

    // Initialize Load Cell if enabled
    if (testLoadCell) {
        loadCell.begin(LOAD_DOUT, LOAD_SCK);
        delay(200);
        loadCell.tare();
        loadCell.set_scale(calibration_factor);
        Serial.println("[INIT] Load Cell initialized and tared.");
    } else {
        Serial.println("[INFO] Load Cell testing is DISABLED.");
    }

    if (testPressure) {
        Serial.println("[INIT] Pressure Transducer pin configured (A0).");
    } else {
        Serial.println("[INFO] Pressure Transducer testing is DISABLED.");
    }

    printStartPrompt();
}

void loop() {
    // 1. Wait for user input to start or stop the test stream
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();

        if (!testStarted) {
            if (command == "s" || command == "start") {
                testStarted = true;
                lastSampleMillis = millis();
                Serial.println("\n[INFO] Starting sensor data stream... Type 'stop' or 'q' to end.");
                printHeader();
            } else {
                Serial.println("Invalid command. Type 's' or 'start' to begin:");
            }
        } else {
            if (command == "stop" || command == "q") {
                testStarted = false;
                Serial.println("\n[INFO] Sensor test paused.");
                printStartPrompt();
            }
        }
    }

    // 2. Stream sensor data non-blockingly at `sampleInterval`
    if (testStarted) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastSampleMillis >= sampleInterval) {
            lastSampleMillis = currentMillis;

            String output = "";
            unsigned long timestamp = millis();
            output += "Time: " + String(timestamp) + " ms\t";

            // Read Pressure Transducer if enabled
            if (testPressure) {
                float pressure = readPressure();
                output += "Pressure: " + String(pressure, 2) + " PSI\t";
            }

            // Read Load Cell if enabled
            if (testLoadCell) {
                float thrust = 0.0;
                if (loadCell.is_ready()) {
                    thrust = loadCell.get_units();
                } else {
                    thrust = 0.0; // or indicator of not ready
                }
                output += "Thrust: " + String(thrust, 2) + " units";
            }

            Serial.println(output);
        }
    }
}

float readPressure() {
    float analogPressure = analogRead(PRESS_PIN); 
    float pressureVolts = (analogPressure * 5.0) / (pow(2, analogResolutionBits)); // Make sure calibrated correctly
    if (pressureVolts < 0.5) {
        return 0.0;
    } else {
        return (pressureVolts - 0.5) * 400;
    }
}

void printStartPrompt() {
    Serial.println("\n========================================");
    Serial.println("SENSOR UNIT TEST READY");
    Serial.println("Configuration:");
    Serial.println(" - Pressure Transducer: " + String(testPressure ? "ENABLED" : "DISABLED"));
    Serial.println(" - Load Cell:           " + String(testLoadCell ? "ENABLED" : "DISABLED"));
    Serial.println("Type 's' or 'start' and press Enter to begin:");
    Serial.println("========================================");
}

void printHeader() {
    Serial.println("----------------------------------------");
    String header = "LOGGING STREAM -> ";
    if (testPressure) header += "[Pressure] ";
    if (testLoadCell) header += "[Thrust]";
    Serial.println(header);
    Serial.println("----------------------------------------");
}