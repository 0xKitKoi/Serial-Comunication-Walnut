# Walnut App for Microcontroller Communication

This is Client built with [Walnut](https://github.com/TheCherno/Walnut) - This project was intended to make [Michael Reevee's](https://youtu.be/_P24em7Auq0?si=aHVvcCby17MTgddT) Laser Turret Wireless. This Project uses two Raspberry Pi Pico W's to acomplish this. The Client sends commands to one Pico W connected to the computer via USB Serial Ports. That Pico then hosts a GATT Server sending the data received from the Client. Another Pico W would then scan for the GATT Server via Bluetooth and read commands to then parse and control Servo Motors to move the Laser. This setup allows the Laser Turret System to be anywhere in the room, or be controlled remotely anywhere in the world. 

## Getting Started
Once you've cloned, run `scripts/Setup.bat` to generate Visual Studio 2022 solution/project files. The app is located in the `WalnutApp/` directory. 