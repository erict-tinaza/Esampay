#include <Arduino.h>

// Pin Definitions
const int MOTOR_ENABLE_PIN = 6;
const int MOTOR_IN1_PIN = 5;
const int MOTOR_IN2_PIN = 4;
const int RAIN_SENSOR_PIN = 14;
const int LIGHT_SENSOR_PIN = 15;
const int NODE_COMMAND_PIN = 7;

// Configuration Constants
const int RAIN_THRESHOLD = 500;
const int LIGHT_THRESHOLD = 200;
const unsigned long ROTATION_TIME = 7000;
const int MOTOR_SPEED = 255;
const unsigned long PULSE_TIMEOUT = 500;
const unsigned long COMMAND_DELAY = 750;

// Function Prototypes
void initializePins();
void startMotor(bool direction);
void stopMotor();
void printSystemStatus(int nodeCommand, int rainValue, int lightValue);
void handleManualControl(int nodeCommand);
void handleAutomatedControl(int rainSensorVal, int ldrVal);
void updateStateFlags(bool isDay, bool isRaining);

// Global State Variables
bool motorRunning = false;
bool hasRunLDRLight = false;
bool hasRunRainWater = false;
bool hasRunLDRDark = false;
bool hasRunRainNoWater = false;

// Pulse Detection Variables
int lastNodeCommand = LOW;
unsigned long lastCommandTime = 0;
int pulseCount = 0;
unsigned long motorStartTime = 0;
bool waitingForPulses = false;

void initializePins()
{
    // Motor control pins
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    pinMode(MOTOR_IN1_PIN, OUTPUT);
    pinMode(MOTOR_IN2_PIN, OUTPUT);

    // Sensor pins
    pinMode(RAIN_SENSOR_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    pinMode(NODE_COMMAND_PIN, INPUT);
}

void startMotor(bool direction)
{
    motorRunning = true;
    analogWrite(MOTOR_ENABLE_PIN, MOTOR_SPEED);

    if (direction)
    {
        digitalWrite(MOTOR_IN1_PIN, LOW); // Counterclockwise (IN)
        digitalWrite(MOTOR_IN2_PIN, HIGH);
        Serial.println(F("Motor starting - Moving IN"));
    }
    else
    {

        digitalWrite(MOTOR_IN1_PIN, HIGH); // Clockwise (OUT)
        digitalWrite(MOTOR_IN2_PIN, LOW);
        Serial.println(F("Motor starting - Moving OUT"));
    }
    motorStartTime = millis();
}

void stopMotor()
{
    motorRunning = false;
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    analogWrite(MOTOR_ENABLE_PIN, 0);
    pulseCount = 0;
    Serial.println(F("Motor stopped"));
}

void printSystemStatus(int nodeCommand, int rainValue, int lightValue)
{
    Serial.println(F("\n--- System Status ---"));
    Serial.print(F("NodeMCU Command: "));
    Serial.println(nodeCommand);
    Serial.print(F("Motor Running: "));
    Serial.println(motorRunning);
    Serial.print(F("Rain Value: "));
    Serial.println(rainValue);
    Serial.print(F("Light Value: "));
    Serial.println(lightValue);
}

void handleManualControl(int nodeCommand)
{
    static int lastLocalNodeCommand = LOW;

    if (nodeCommand != lastLocalNodeCommand)
    {
        if (nodeCommand == HIGH)
        {
            if (millis() - lastCommandTime < PULSE_TIMEOUT)
            {
                pulseCount++;
            }
            else
            {
                pulseCount = 1;
            }
            Serial.print(F("Pulse count: "));
            Serial.println(pulseCount);

            lastCommandTime = millis();
            waitingForPulses = true;
        }
        lastLocalNodeCommand = nodeCommand;
    }

    // Process accumulated pulses
    if (waitingForPulses && !motorRunning &&
        (millis() - lastCommandTime >= COMMAND_DELAY))
    {

        waitingForPulses = false;

        if (pulseCount == 1)
        {
            Serial.println(F("Manual IN command received"));
            startMotor(false);
        }
        else if (pulseCount == 2)
        {
            Serial.println(F("Manual OUT command received"));
            startMotor(true);
        }

        pulseCount = 0;
    }

    // Reset pulse count after timeout
    if (!waitingForPulses && millis() - lastCommandTime > PULSE_TIMEOUT)
    {
        pulseCount = 0;
    }
}

void updateStateFlags(bool isDay, bool isRaining)
{
    if (!isDay)
        hasRunLDRLight = false;
    if (isDay)
        hasRunLDRDark = false;
    if (!isRaining)
        hasRunRainWater = false;
    if (isRaining)
        hasRunRainNoWater = false;
}

void handleAutomatedControl(int rainSensorVal, int ldrVal)
{
    if (!motorRunning && pulseCount == 0)
    {
        const bool isDay = ldrVal > LIGHT_THRESHOLD;
        const bool isRaining = rainSensorVal < RAIN_THRESHOLD;

        // Day time logic
        if (isDay)
        {
            if (!hasRunLDRLight || hasRunLDRDark)
            {
                if (!isRaining)
                {
                    startMotor(true); // Move OUT when not raining during day
                    hasRunLDRLight = true;
                    hasRunLDRDark = false;
                }
                else
                {
                    Serial.println(F("Rain detected, moving clothes outside."));
                    startMotor(true); // Move OUT during rain
                    hasRunRainWater = true;
                }
            }

            // Additional rain logic
            if (isRaining && !hasRunRainWater)
            {
                startMotor(true); // Move OUT during rain
                hasRunRainWater = true;
            }
            else if (!isRaining && !hasRunRainNoWater)
            {
                startMotor(false); // Move IN after rain stops
                hasRunRainNoWater = true;
            }
        }

        // Night time logic
        else if (!isDay && !hasRunLDRDark)
        {
            if (!motorRunning)
            {
                startMotor(false); // Always move IN at night
            }
            hasRunLDRDark = true;
            hasRunLDRLight = false;
        }

        // Reset state flags
       updateStateFlags(isDay, isRaining);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Starting Arduino..."));

    initializePins();
    stopMotor();

    Serial.println(F("Setup complete"));
}

void loop()
{
    static unsigned long lastPrint = 0;

    // Read sensor values
    const int nodeCommand = digitalRead(NODE_COMMAND_PIN);
    const int rainSensorVal = analogRead(RAIN_SENSOR_PIN);
    const int ldrVal = analogRead(LIGHT_SENSOR_PIN);

    // Handle manual control
    handleManualControl(nodeCommand);

    // Check if motor needs to stop
    if (motorRunning && (millis() - motorStartTime >= ROTATION_TIME))
    {
        stopMotor();
    }

    // Handle automated control
    handleAutomatedControl(rainSensorVal, ldrVal);

    // Print status at regular intervals

    if (millis() - lastPrint >= 1000)
    {
        lastPrint = millis();
        printSystemStatus(nodeCommand, rainSensorVal, ldrVal);
    }

    // Small delay for stability
    delay(10);
}