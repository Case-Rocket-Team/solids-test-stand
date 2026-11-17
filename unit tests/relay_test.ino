const int ARM2 = 7;
const int FIRE = 6;

const byte RELAY_ON = LOW;
const byte RELAY_OFF = HIGH;

const unsigned long testInterval = 3000; // Time between test steps (ms)
unsigned long previousMillis = 0;
int testStep = 0;

bool testStarted = false; // Tracks whether the user has started the test

void setup() {
    Serial.begin(9600);
    while (!Serial); 

    pinMode(ARM2, OUTPUT);
    pinMode(FIRE, OUTPUT);

    // Default state: Both relays inactive 
    digitalWrite(ARM2, RELAY_OFF);
    digitalWrite(FIRE, RELAY_OFF);

    printStartPrompt();
}

void loop() {
    // Wait for user to type 's' or 'start' to kick off or restart the test
    if (!testStarted) {
        if (Serial.available() > 0) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            command.toLowerCase(); // Make it case-insensitive

            if (command == "s" || command == "start") {
                testStarted = true;
                previousMillis = millis(); // Initialize the timer right when it starts
                Serial.println("\n[INFO] Starting diagnostic sequence...");
            } else {
                Serial.println("Invalid command. Type 's' or 'start' to begin:");
            }
        }
        return; 
    }

    // Run test sequence steps non-blockingly every `testInterval`
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= testInterval) {
        previousMillis = currentMillis;
        testStep++;

        switch (testStep) {
            case 1:
                Serial.println("\n[TEST 1] Testing ARM2 Relay (Pin 7)...");
                Serial.println("Action: Pulling ARM2 LOW (System Armed).");
                Serial.println("Expected Result: ARM2 indicator should change state.");
                digitalWrite(ARM2, RELAY_ON); // Active state
                break;

            case 2:
                Serial.println("\nAction: Pulling ARM2 HIGH (System Disarmed).");
                Serial.println("Expected Result: ARM2 indicator should turn off.");
                digitalWrite(ARM2, RELAY_OFF); // Inactive state
                break;

            case 3:
                Serial.println("\n[TEST 2] Testing FIRE Relay (Pin 6)...");
                Serial.println("Use multimeter to measure voltage to simulate ematch.");
                Serial.println("Expected Result: If ARM1 physical switch is closed, voltage should increase while firing. If open, no change.");
                delay(1000); // Short pause to read instructions before firing
                
                Serial.println("Action: Pulling FIRE LOW (Firing state) for 3s.");
                digitalWrite(FIRE, RELAY_ON); // Fire pulse active
                delay(3000);                  // Hold for fire duration
                digitalWrite(FIRE, RELAY_OFF); // Release fire pulse
                Serial.println("Action: Pulling Fire HIGH (Stop Firing).");
                break;

            default:
                Serial.println("\n========================================");
                Serial.println("Finished Testing Sequence.");
                Serial.println("========================================");
                testStep = 0;         // Reset test steps for next time
                testStarted = false;  // Reset flag to require 's' or 'start' again
                printStartPrompt();   // Prompt user to start over
                break;
        }
    }
}

void printStartPrompt() {
    Serial.println("\n========================================");
    Serial.println("RELAY UNIT TEST READY");
    Serial.println("Type 's' or 'start' and press Enter to begin:");
    Serial.println("========================================");
}