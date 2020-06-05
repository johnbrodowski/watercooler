#include <OneWire.h>
#include <DallasTemperature.h>

#define oneSec    1000
#define tenSec    10000
#define thirtySec 30000
#define oneMin    60000
#define ONE_WIRE_BUS 2
#define MAXTEMP 200

unsigned long startTime;
unsigned long compTempTime;

bool wasRunning = false;
bool Cooling = false;
bool ProtectionMode = false;
bool safeStart;

const int numReadings = 5;
int readings[numReadings];                // the readings from the analog input
int readIndex = 0;                        // the index of the current reading
int total = 0;                            // the running total
int inputPin = 0;                         // Tmp36 data pin
int compTemp;
int setTemp = 67;
int waterTemp;
int waterHysteresis = 2;
int onTime = 0;
int waitTime = 0;
int downTime = 0;
int safeTime = 300;
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

void setup()
{
	Serial.begin(115200);
	pinMode(8, OUTPUT);
	startTime = millis();
	sensors.begin();
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

		if (waterTemp <= (setTemp - waterHysteresis)) {
			Cooling = false;
		}
		else if (waterTemp >= (setTemp + waterHysteresis)) {
			if (Cooling == false) {
				safeStart = true;
			}
			Cooling = true;
		}
		if (safeStart) {
			if ((waitTime + downTime) >= safeTime) {
				safeStart = false;
				if (Cooling) {
					digitalWrite(8, HIGH);
				}
			}
			else {
				waitTime++;
				if (Cooling) {
					Serial.println("                  Wait time " + (String)(waitTime));
				}
			}
		}
		else {
			if (ProtectionMode) {  // Compressor has overheated and needs time to cool down
				Serial.println("Protection Mode");
				if (compTemp <= 100) {  // Wait for compressor to get down to 120 degrees.
					Cooling = true;
					ProtectionMode = false;
					Serial.println("Overheat Protection Off");
				}
			}
			else {
				if (Cooling) {  // Start cooling
					
					if (compTemp >= MAXTEMP) { // If max temp is reached go into protection mode.
						ProtectionMode = true;
						safeStart = true;
						StopCompressor();
						onTime = 0;
						Serial.println("Overheat Protection Temp over 200F");
					}
					else {
						if (onTime <= 1800) {  // Turn on compressor for 30 min.
							digitalWrite(8, HIGH);
							onTime++;
							waitTime = 0;
							downTime = 0;
							Serial.println("Running " + (String)(onTime));
						}
						else if ((onTime >= 1800) && (onTime <= 2100)) {  // Shut off compressor for 5 min.
							digitalWrite(8, LOW);
							onTime++;
							Serial.println("Cool down " + (String)(onTime));
						}
						else if (onTime > 2100) {
							onTime = 0;
						}
					}
				}
				else
				{
					onTime = 0;
					Serial.println("                  Down Time  " + (String)(downTime));
					digitalWrite(8, LOW);
					downTime++;
				}
			}
		}
	}
}

void StopCompressor() {
	Cooling = false;
	onTime = 0;
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
}

void WaterTemp() {
	sensors.requestTemperatures(); // Send the command to get temperatures
	waterTemp = (int)(sensors.getTempCByIndex(0));
	waterTemp = (waterTemp * 9.0 / 5.0) + 32.0;
	Serial.println("Water Temp " + (String)(waterTemp)+" --  Compressor Temp " + (String)(compTemp));
}