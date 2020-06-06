#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define oneSec    1000
#define tenSec    10000
#define thirtySec 30000
#define oneMin    60000
#define ONE_WIRE_BUS 12
#define MAXTEMP 200
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

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

const int numReadings2 = 5;
int readings2[numReadings2];                // the readings from the analog input
int readIndex2 = 0;                        // the index of the current reading
int total2 = 0;                            // the running total

const int clkPin = 2; //the clk attach to pin 2
const int dtPin = 3; //the dt pin attach to pin 3
const int swPin = 4;//the sw pin attach to pin 4

int inputPin = 0;                         // Tmp36 data pin
int compTemp;
int setTemp = 65;
int waterTemp;
int waterHysteresisUp = 2;
int waterHysteresisDwn = 2;
int onTime = 0;
int waitTime = 0;
int downTime = 0;
int safeTime = 300;
int encoderVal = 0;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup()
{
	//set clkPin,dePin,swPin as INPUT
	pinMode(clkPin, INPUT);
	pinMode(dtPin, INPUT);
	pinMode(swPin, INPUT);
	digitalWrite(swPin, HIGH);
	pinMode(8, OUTPUT);
	startTime = millis();
	sensors.begin();
	wasRunning = false;
	safeStart = true;
	Serial.begin(115200);
	encoderVal = setTemp;

	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
		Serial.println(F("SSD1306 allocation failed"));
		for (;;); // Don't proceed, loop forever
	}
}

void loop() {
	int change = getEncoderTurn();//
	encoderVal = encoderVal + change;
	if (digitalRead(swPin) == LOW)//if button pull down
	{
		setTemp = encoderVal;
	}
	setTemp = encoderVal;

	//Serial.println(encoderVal);
	if (millis() - startTime > oneSec) {
		startTime = millis();
		CompressorTemp();
		GetWaterTemp();
		//Delay initial start to ensure compressor has had time to decompress.
		//otherwise the compressor will have a hard time starting.

		if (waterTemp <= (setTemp - waterHysteresisDwn)) {
			Cooling = false;
		}
		else if (waterTemp >= (setTemp + waterHysteresisUp)) {
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
					if (downTime < 301) {
						downTime++;
					}
				}
			}
		}
		display.clearDisplay();
		display.setTextSize(2);             // Normal 1:1 pixel scale
		display.setTextColor(WHITE);        // Draw white text
		display.setCursor(0, 0);             // Start at top-left corner
		display.print(F("Temp - "));
		display.println(waterTemp);
		display.setCursor(0, 18);             // Start at top-left corner
		display.print(F("Set  - "));
		display.println(setTemp);
		display.display();
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

void GetWaterTemp() {
	total2 = total2 - readings2[readIndex2];
	sensors.requestTemperatures(); // Send the command to get temperatures
	int wTemp = (int)(sensors.getTempCByIndex(0));
	int wTemp2 = ((wTemp * 9.0 / 5.0) + 32.0);
	readings2[readIndex2] = wTemp2;
	total2 = total2 + readings2[readIndex2];
	readIndex2 = readIndex2 + 1;
	if (readIndex2 >= numReadings2) {
		readIndex2 = 0;
	}
	float average2 = total2 / numReadings2;
	waterTemp = int(average2);
	Serial.println("Set Temp -- " + (String)(setTemp)+" Temp -- " + (String)(waterTemp)+" --  Compressor Temp " + (String)(compTemp));
}

int getEncoderTurn(void)
{
	static int oldA = HIGH; //set the oldA as HIGH
	static int oldB = HIGH; //set the oldB as HIGH
	int result = 0;
	int newA = digitalRead(clkPin);//read the value of clkPin to newA
	int newB = digitalRead(dtPin);//read the value of dtPin to newB
	if (newA != oldA || newB != oldB) //if the value of clkPin or the dtPin has changed
	{
		// something has changed
		if (oldA == HIGH && newA == LOW)
		{
			result = (oldB * 2 - 1);
		}
	}
	oldA = newA;
	oldB = newB;
	return result;
}