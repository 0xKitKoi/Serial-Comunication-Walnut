This program is a tool to communicate with microcontrollers like an Arduino.

IF YOU DO NOT HAVE A MICROCONTROLLER:
I included an example program that acts as an ECHO Server. You can connect to the echo server with the client via network mode.
Assuming you are running the Example Echo Server on your machine, ( Default Settings are 127.0.0.1:5000 )
You can use it to test the commands you will send to a microcontroller. (or just test this out ig)

If you DO have a microcontroller, Example Firmware is included:

Arduino Code for a tiny OLED Screen over I2C ( I used a SSD1315 chip )
	Displays text sent over USB, or plays a small animation if the play command was sent

Raspberry Pi Pico (W) code in C/C++ and Python (MicroPython)
	Pico W C SDK Firmware includes:
		DataBridge: 
			A WiFi Proxy device, sends data to and from Another Pico to the Desktop Application.
		
		Wireless Servo Controller: 
			Controls 2 servo motors for a laser turret via WiFi connected to the Desktop Application (mouse mode)
	Pico W Python Firmware includes:
		Bluetooth Example:
			Uses 2 Raspberry Pi Pico's to control 2 servo motors via BLUETOOTH. One is needed to act as a Bluetooth adapter for the Desktop Application.
		Wifi and Servo Motor Examples:
			Same thing as before Communication with the Desktop Application via WiFi with the ability to control the laser turret, just written in Python.
