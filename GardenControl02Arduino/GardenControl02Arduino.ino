/**
 * Since we are prescaling to /2, we must set the board as 8MH instead of 16MH, or
 * else the baud rates will actually be *2 and we won't be able to read the serial.
 * Prescaler lib isn't standard: https://github.com/fschaefer/Prescaler/blob/master/prescaler.h
 * LowPower lib isn't standard: https://github.com/rocketscream/Low-Power
 */
#include <SoftwareSerial.h>
#include "Prescaler.h"
#include "LowPower.h"

SoftwareSerial espSerial(10, 11); // RX, TX
const unsigned char ESP_POWER_PIN = 7;
const unsigned char PUMP_POWER_PIN = 8;
const unsigned char MOISTURE_READ_PIN = A0;
const unsigned char LED_PIN = 13;

const byte READING_CACHE_SIZE = 20; // How many to keep locally before sync

const unsigned char SLEEP_FOR = 50; // Number of cycles * 8s

const unsigned long PUMP_TIMER = 30000 * 1; // How long to keep the pump ON
const unsigned long PUMP_MOISTURE_TRESHOLD = 500; // Moisture level that triggers pump

unsigned short lastMoistureRead = 0;

unsigned long sleptFor = 0; // How many ms did u sleep since last sync? Need to keep track since the timer is off and millis() won't work

byte currReadingIdx = 0;
unsigned long readingTimes[READING_CACHE_SIZE];
unsigned short moistureLevel[READING_CACHE_SIZE];
unsigned short temperature[READING_CACHE_SIZE];
unsigned short light[READING_CACHE_SIZE];

byte currPumpIdx = 0;
unsigned long pumpTimes[READING_CACHE_SIZE];

void setup() {
	// 8MH
	setClockPrescaler(CLOCK_PRESCALER_2);

	// LED
	pinMode(LED_PIN, OUTPUT);

	// Moisture sensor
	pinMode(MOISTURE_READ_PIN, INPUT);
	digitalWrite(MOISTURE_READ_PIN, HIGH); // PullUp

	// Pump
	pinMode(PUMP_POWER_PIN, OUTPUT);
	digitalWrite(PUMP_POWER_PIN, LOW);

	// ESP power (transistor) + serial
	pinMode(ESP_POWER_PIN, OUTPUT);
	digitalWrite(ESP_POWER_PIN, LOW);
	espSerial.begin(9600);

	// Debug
	Serial.begin(9600);
}

/**
 * When sending a reading or command post, wait for either success or error
 */
boolean waitForESPSyncCommand() {
	for (byte i = 0; i < 10; i++) {
		if (espSerial.available()) {
			String line = espSerial.readString();
			Serial.print("Received: ");
			Serial.println(line);
			if ((line.indexOf("ERR") >= 0) || (line.indexOf("SUCPST") >= 0)) {
				return (line.indexOf("SUCPST") >= 0);
			}
		}
		Serial.println("Waiting response...");
		trueDelay(5000);
	}
	Serial.println("Command timed out.");
	return false;
}

/**
 * Waits for the "prompt"
 */
boolean waitForESPCommandLine() {
	for (byte i = 0; i < 20; i++) {
		Serial.println("Waiting for ESP to come alive...");
		if (espSerial.available()) {
			String line = espSerial.readString();
			Serial.print("Received: ");
			Serial.println(line);
			if ((line.indexOf("REQCMD") >= 0)) {
				Serial.println("All good!");
				return true;
			}
		}
		trueDelay(5000);
	}
	Serial.println("ESP timed out");
	return false;
}

/**
 * Sends all the local stored reading/commands to the cloud
 */
void send2Cloud() {
	// Turn it on
	digitalWrite(ESP_POWER_PIN, HIGH);

	if (!waitForESPCommandLine()) {
		Serial.println("Couldn't connect to ESP. Turning it off and resetting cache.");
	} else {
		unsigned long currTime = trueMillis() + sleptFor;

		// Sync
		Serial.println("Sending pump logs");
		for (byte i = 0; i < currPumpIdx; i++) {
			String data = "PSTLOG:PUMP.A&";
			data.concat(currTime - pumpTimes[i]);
			espSerial.println(data);
			waitForESPSyncCommand();
		}

		Serial.println("Sending reading logs");
		for (byte i = 0; i < currReadingIdx; i++) {
			// Moisture
			String data = "PSTRDG:Moisture&";
			data.concat(moistureLevel[i]);
			data += "&";
			data.concat(currTime - readingTimes[i]);
			espSerial.println(data);
			waitForESPSyncCommand();

			// Temperature
			data = "PSTRDG:Temperature&";
			data.concat(temperature[i]);
			data += "&";
			data.concat(currTime - readingTimes[i]);
			espSerial.println(data);
			waitForESPSyncCommand();

			// Light
			data = "PSTRDG:Light&";
			data.concat(light[i]);
			data += "&";
			data.concat(currTime - readingTimes[i]);
			espSerial.println(data);
			waitForESPSyncCommand();
		}
	}

	// Turn it off
	digitalWrite(ESP_POWER_PIN, LOW);

	// Zero everything
	currReadingIdx = 0;
	currPumpIdx = 0;
	sleptFor = 0;
}

/**
 * Reads all sensors and store it locally
 */
void readSensors() {
	readingTimes[currReadingIdx] = trueMillis() + sleptFor;
	lastMoistureRead = 1023 - analogRead(MOISTURE_READ_PIN); // Since we're pulling up
	moistureLevel[currReadingIdx] = lastMoistureRead;
	temperature[currReadingIdx] = 0; // Waiting for sensor
	light[currReadingIdx] = 0; // Waiting for sensor
	currReadingIdx++;
}

/**
 * Switches the pump on for the pre-defined period
 */
void switchPump() {
	pumpTimes[currPumpIdx] = trueMillis() + sleptFor;
	digitalWrite(PUMP_POWER_PIN, HIGH);
	trueDelay(PUMP_TIMER);
	digitalWrite(PUMP_POWER_PIN, LOW);
	currPumpIdx++;
}

/**
 * Sleeps for the pre-defined period
 */
void goToSleep() {
	for (unsigned char i = 0; i < SLEEP_FOR; i++) {
		LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,
					SPI_OFF, USART0_OFF, TWI_OFF);
	}
	sleptFor += 8 * 1000; // Slept for 8s
}

void loop() { // run over and over
	goToSleep();
	Serial.print("Reading sensors: "); Serial.println(currReadingIdx);
	readSensors();
	if (lastMoistureRead < PUMP_MOISTURE_TRESHOLD) {
		Serial.println("Switching pump.");
		switchPump();
	}
	if (currReadingIdx == READING_CACHE_SIZE || currPumpIdx == READING_CACHE_SIZE) {
		Serial.println("Sending to cloud.");
		send2Cloud();
	}
	trueDelay(5000);
}

