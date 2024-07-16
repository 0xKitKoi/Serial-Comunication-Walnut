This is an attempt to make Michael Reevee's laser turret wireless. 
This setup uses two Pico W's. One is connected to my computer and takes input.
The Second Pico W controls the turret, lets say that circuit is across the room.

The Client is built with TheCherno's wrapper lib around ImGUI.
    It connects to the Pico w running Central.py over USB Serial Port / Com Port.
    It can send commands over WiFi as well!
    You can send the microcontroller text or use Mouse Mode to send Mouse coordinates
    

The Central.py would take input over serial and has set commands.
    \X44Y180 would send the servos to X: 44, Y: 180

Peripheral.py would recieve the commands over bluetooth 
    and parse for the ints, to then control the servo motors.
