# Walnut App for Microcontroller Communication

This is Client built with [Walnut](https://github.com/TheCherno/Walnut) - This project was intended to make [Michael Reevee's](https://youtu.be/_P24em7Auq0?si=aHVvcCby17MTgddT) Laser Turret Wireless. This Project uses two Raspberry Pi Pico W's to acomplish this. The Client sends commands to one Pico W connected to the computer via USB Serial Ports. That Pico then hosts a GATT Server sending the data received from the Client. Another Pico W would then scan for the GATT Server via Bluetooth and read commands to then parse and control Servo Motors to move the Laser. This setup allows the Laser Turret System to be anywhere in the room, or be controlled remotely anywhere in the world. 

## Getting Started
Once you've cloned, run `scripts/Setup.bat` to generate Visual Studio 2022 solution/project files. The app is located in the `WalnutApp/` directory. 

## How To Use
By default, the application is set to text mode. this will allow the user to select a COM port for communcation. When you plug a microcontroller like an arduino into your PC, it should be assigned a COM Port. Select the correct one and send it raw data with the text box.
You can also send data to a microcontroller over WiFi. In the settings there is an IP and PORT text box. This is assuming the microcontroller is set up to be the server, and the client will attempt to connect and send data. 

# Mouse Mode
This project was intended to be a remake of Michael Reevee's laser turret software he wrote in C# using WinForms. In his video (Link attached), He shows how to pipe mouse coordinates over to the microcontroller to parse, which the microcontroller then uses to control 2 servo motors for Pan and Tilt, all to aim a small laser diode. Selecting mouse mode in my application will display an image to act as our mouse pad. Holding left click will continuously send the mouse coordinates to the selected communication mode, whether that be a com port or an IP address. The structure of the char buffer being sent is as follows:

# Mouse Mode Raw Buffer Structure
```"\X(INT)Y(INT)\n0"``` \
the backslash helps the microcontroller parse. X and Y are also parsed out, and the ints are extracted. the string is null terminated and a \n is added just for good measure. 

```C++
if (ImGui::IsMouseDragging(0)) {
				//printf("Holding mouse button down.\n");
				// Michael's arduino parses this to control two servo motors for Pan And Yaw. Yee Haw.
				//char coords[10];
				std::string temp;
				temp += "\\"; // this is a custom command format, the microcontroller will parse this for the ints.
				temp += "X";
				temp += std::to_string((int)relativePos.x);
				temp += "Y";
				temp += std::to_string((int)relativePos.y);
				temp += "\n";
				temp += '\0';

    /// continued ........
}
```
