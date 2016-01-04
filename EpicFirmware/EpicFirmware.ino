/*
 * Epic Laser Tag firmware
 */

#include <Adafruit_NeoPixel.h>
#include <PinChangeInterrupt.h>
#include <IRremoteInt.h>
#include <IRremote.h>

// The PINS where stuff is connected
#define BTN_FIRE_PIN		5
#define BTN_RELOAD_PIN		8
#define BTN_SILENCER_PIN	A0
#define LED_FLASH_PIN		9
#define LED_RGB_PIN			6
#define LED_IR_PIN			3     // NOTE: this is always on Pin 3 because it requires PWM and timer support
#define RECV_IR_PIN			11
#define SOUND_FIRE_PIN		10
#define SOUND_SILENCER_PIN	12
#define SOUND_RELOAD_PIN	13

// number of RGB LEDs in the chain
#define NUMPIXELS			1

// Gun Settings
#define CLIP_SIZE			8
#define RATE_OF_FIRE_MS		250
#define AUTO_FIRE			0
#define ENABLE_IR_HIT		false
#define ENABLE_FLASH		true

// IR send support
IRsend irsend;
unsigned long sendData = 0x04000502;	// IR data to send

// IR Recieve support
IRrecv irrecv(RECV_IR_PIN);
decode_results results;

// current command being sent from the serial port
String command;

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);


void setup()
{
	// enable the serial port
	Serial.begin(9600);

	pixels.begin(); // This initializes the NeoPixel library.

	//make the button pins an input
	pinMode(BTN_FIRE_PIN, INPUT_PULLUP);
	pinMode(BTN_RELOAD_PIN, INPUT_PULLUP);
	pinMode(BTN_SILENCER_PIN, INPUT_PULLUP);
	
	// Enable the LEDs and sounds as outputs
	pinMode(LED_FLASH_PIN, OUTPUT);
	pinMode(SOUND_FIRE_PIN, OUTPUT);
	pinMode(SOUND_RELOAD_PIN, OUTPUT);
	pinMode(SOUND_SILENCER_PIN, OUTPUT);

	// Start the IR receiver
	if (ENABLE_IR_HIT)
		irrecv.enableIRIn();

	// attach the new PinChangeInterrupt and enable event function below
	attachPCINT(digitalPinToPCINT(BTN_FIRE_PIN), fireChanged, CHANGE);
	attachPCINT(digitalPinToPCINT(BTN_RELOAD_PIN), reloadChanged, CHANGE);

	Serial.println("Ready for something Epic?");
}

int fireCount = 0;
bool firePressed = false;
unsigned long fireTime = 0;

int shellCount = 0;
bool reloadSound = false;

void fireChanged(void) {
	if (!digitalRead(BTN_FIRE_PIN) && millis() - fireTime > RATE_OF_FIRE_MS)
	{
		fireTime = millis();
		firePressed = true;
	}
}

void reloadChanged(void) {
	if (!digitalRead(BTN_RELOAD_PIN) && !reloadSound && shellCount == 0)
	{
		shellCount = CLIP_SIZE;
		reloadSound = true;
	}
}



void loop() {
	// if the button is pressed (high) send the fire signal to the IR
	if (firePressed)
	{
		firePressed = false;

		Serial.println("fire");

		if (shellCount > 0)
		{
			shellCount--;

			// Play the fire sound
			if (digitalRead(BTN_SILENCER_PIN))
			{
				digitalWrite(SOUND_FIRE_PIN, HIGH);
				sendData = 0x07000502;	// blue
			}
			else
			{
				digitalWrite(SOUND_SILENCER_PIN, HIGH);
				sendData = 0x04000502;	// red
			}

			// turn the flash led on
			if (ENABLE_FLASH)
				digitalWrite(LED_FLASH_PIN, HIGH);

			// Send the IR data
			irsend.sendLightStrike(sendData, 32);

			// sending shares the same timer and interupts as recieving so we need to re-enable the IR reciever after sending
			if (ENABLE_IR_HIT)
				irrecv.enableIRIn();

			// turn the flash led off
			if (ENABLE_FLASH)
				digitalWrite(LED_FLASH_PIN, LOW);

			// turn off the sound signal
			digitalWrite(SOUND_FIRE_PIN, LOW);
			digitalWrite(SOUND_SILENCER_PIN, LOW);
		}
	}

	// play the reload sound
	if (reloadSound)
	{
		reloadSound = false;
		digitalWrite(SOUND_RELOAD_PIN, HIGH);
		delay(100);
		digitalWrite(SOUND_RELOAD_PIN, LOW);
	}

	
	// Update the LED Status
	// pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
	uint32_t statusColor;
	uint8_t brightness = 150;  // Moderately bright
	if (shellCount > 1)
		statusColor = pixels.Color(0, brightness, 0);	// green
	else if (shellCount == 1)
		statusColor = pixels.Color(brightness, brightness, 0);  // yellow
	else
		statusColor = pixels.Color(brightness, 0, 0);  // red

	// Update the status LED if needed
	if (pixels.getPixelColor(0) != statusColor)
	{
		pixels.setPixelColor(0, statusColor);
		pixels.show();
	}
	

	// If we recieved an IR signal then send it to the debug serial port
	if (ENABLE_IR_HIT && irrecv.decode(&results)) {
		if (results.decode_type == LIGHTSTRIKE)
			Serial.println(results.value, HEX);
		else
			Serial.println("miss");

		irrecv.resume();		// Receive the next value
	}

	// if we have data from the serial port check
	if (readCommandAvailible())
	{
		if (command.length() == 8) // make sure it is 8 charactors
		{
			// set the value
			sendData = hexToDec(command);

			Serial.print("Fire set to: ");
			Serial.println(sendData, HEX);
		}
		else
		{
			Serial.print("unknown command: ");
			Serial.println(command);
		}
	}

	// wait a few milliseconds so we dont loop too quickly and consume too much power
	delay(40);
}


// reads a line from the serial port if it is availible
String commandBuffer;
bool readCommandAvailible() {
	while (Serial.available())
	{
		char c = Serial.read();  //gets one byte from serial buffer
		if (c == '\r' || c == '\f' || c == '\n') // end of the line so we are done
		{
			if (commandBuffer.length() > 0)
			{
				command = commandBuffer;
				commandBuffer = "";
				return true;
			}
		}
		else
			commandBuffer += c; // append the charactor to the command
	}
	
	return false;
}

// convert a hex string into a 32bit number
// from: https://github.com/benrugg/Arduino-Hex-Decimal-Conversion/blob/master/hex_dec.ino
unsigned long hexToDec(String hexString) {

	unsigned long decValue = 0;
	int nextInt;

	for (int i = 0; i < hexString.length(); i++) {

		nextInt = int(hexString.charAt(i));
		if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
		if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
		if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
		nextInt = constrain(nextInt, 0, 15);

		decValue = (decValue * 16) + nextInt;
	}

	return decValue;
}
