/*
 * Epic Laser Tag firmware
 */

#include <IRremoteInt.h>
#include <IRremote.h>

// The PINS where stuff is connected
#define BTN_FIRE_PIN	6
#define IR_RECV_PIN		11
#define IR_SEND_PIN		3     // NOTE: this is always on Pin 3 because it requires PWM and timer support

// IR send support
IRsend irsend;
unsigned long sendData = 0x04000502;	// IR data to send

// IR Recieve support
IRrecv irrecv(IR_RECV_PIN);
decode_results results;

// current command being sent from the serial port
String command;

void setup()
{
	// enable the serial port
	Serial.begin(9600);

	//make the button pin an input
	pinMode(BTN_FIRE_PIN, INPUT);

	// Start the IR receiver
	irrecv.enableIRIn();

	Serial.println("Ready for something Epic?");
}

void loop() {
	// if the button is pressed (high) send the fire signal to the IR
	if (digitalRead(BTN_FIRE_PIN) == HIGH)
	{
		Serial.println("fire");
		irsend.sendLightStrike(sendData, 32);
		
		// sending shares the same timer and interupts as recieving so we need to re-enable the IR reciever after sending
		irrecv.enableIRIn();
	}

	// If we recieved an IR signal then send it to the debug serial port
	if (irrecv.decode(&results)) {
		
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
