#include <OneWire.h>
#include <DallasTemperature.h>

#define oneSec    1000
#define tenSec    10000
#define thirtySec 30000
#define oneMin    60000
#define ONE_WIRE_BUS 2
#define MAXTEMP 80

unsigned long startTime;
unsigned long compTempTime;

bool wasRunning = false;
bool Cooling = false;
bool ProtectionMode = false;
bool safeStart = true;

const int numReadings = 5;
int readings[numReadings];                // the readings from the analog input
int readIndex = 0;                        // the index of the current reading
int total = 0;                            // the running total
int inputPin = 0;                         // Tmp36 data pin
int compTemp;
int setTemp = 27;
int waterTemp;
int waterHysteresis = 1;
int onTime = 0;
int waitTime = 0;

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

void setup()
{
	Serial.begin(115200);
	pinMode(3, OUTPUT);
	startTime = millis();
	sensors.begin();
	Cooling = true;
	wasRunning = false;
	safeStart = true;
}

void loop() {
	if (millis() - startTime > oneSec) {
		startTime = millis();
		CompressorTemp();
		WaterTemp();
		//Delay initial start to ensure compressor has had time to decompress.
		//otherwise the compressor will have a hard time starting.
		if (safeStart) {
			if (waitTime >= 5) {
				safeStart = false;
				waitTime = 0;
			}
			else {
				waitTime++;
				Serial.println("Wait time " + String(waitTime));
			}
		}
		else {
			if (ProtectionMode) {  // Compressor has overheated and needs time to cool down
				Serial.println("Protection Mode");
				if (compTemp <= 75) {  // Wait for compressor to get down to 120 degrees.
					Cooling = true;
					ProtectionMode = false;
					Serial.println("Overheat Protection Off");
				}
			}
			else {
				if (Cooling) {  // Start cooling
					if (compTemp >= MAXTEMP) { // If max temp is reached go into protection mode.
						ProtectionMode = true;
						StopCompressor();
						onTime = 0;
						Serial.println("Overheat Protection Temp over 200F");
					}
					else {
						if (onTime <= 29) {  // Turn on compressor for 30 min.
							onTime++;
							StartCompressor();
							Serial.println("Running");
						}
						else if ((onTime > 29) && (onTime <= 34)) {  // Shut off compressor for 5 min.
							onTime++;
							CoolCompressor();
							Serial.println("Cool down ");
						}
						else {
							onTime = 0;
						}
					}
				}
			}
		}
	}
}

void StartCompressor() {
	Cooling = true;
	digitalWrite(8, HIGH);
}

void StopCompressor() {
	Cooling = false;
	digitalWrite(8, LOW);
}

void CoolCompressor() {
	digitalWrite(8, LOW);
}

void CompressorTemp() {
	total = total - readings[readIndex];
	readings[readIndex] = analogRead(inputPin);
	total = total + readings[readIndex];
	readIndex = readIndex + 1;
	if (readIndex >= numReadings) {
		readIndex = 0;
	}
	float average = 0;
	average = total / numReadings;
	float voltage = average * 5.00;
	voltage /= 1024.0;
	float temperatureC = (voltage - 0.5) * 100;  //converting from 10 mv per degree wit 500 mV offset
	float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
	compTemp = (int)temperatureF + 5;
	Serial.println("Compressor Temp " + (String)(compTemp));
}

void WaterTemp() {
	sensors.requestTemperatures(); // Send the command to get temperatures
	waterTemp = (int)(sensors.getTempCByIndex(0));
	int tempInt = (waterTemp * 9.0 / 5.0) + 32.0;
	Serial.println("Water Temp " + (String)(tempInt));
	if (waterTemp <= (setTemp - waterHysteresis)) {
		Cooling = false;
	}
	else if (waterTemp >= (setTemp + waterHysteresis)) {
		Cooling = true;
	}
}