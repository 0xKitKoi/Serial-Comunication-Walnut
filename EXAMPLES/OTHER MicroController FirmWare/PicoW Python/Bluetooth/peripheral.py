import sys
# ruff: noqa: E402
sys.path.append("")
from micropython import const
import asyncio
import aioble
import bluetooth
import random
import struct
import machine
from machine import Pin

led = machine.Pin(25, Pin.OUT) # Onboard LED, made to flash
servoPin = 16
servo = machine.PWM(machine.Pin(servoPin))
servo.freq(50)

def map_value(x, in_min, in_max, out_min, out_max):
    return (x - in_min) * (out_max - out_min) // (in_max - in_min) + out_min # magic

def set_servo_angle(servo, angle):
    min_duty = 1200  # This is 0 degrees?
    max_duty = 7000  # this seems to be 180 degrees

    # Calculate duty for desired angle
    duty = map_value(angle, 0, 180, min_duty, max_duty)
    print(f"Setting angle to {angle}, duty cycle to {duty}")  # Debug print
    # Set the duty cycle
    servo.duty_u16(int(duty))

# BoilerPlate Bluetooth spec requirements
# need these to advertise?
# org.bluetooth.service.environmental_sensing
_ENV_SENSE_UUID = bluetooth.UUID(0x181A)
# org.bluetooth.characteristic.temperature
_ENV_SENSE_TEMP_UUID = bluetooth.UUID(0x2A6E)


# seems like max buffer size is 15 bytes.
def _decode_(sb: bytes, offset=0) -> (str, int):
    # Unpack the length of the string as a 32-bit unsigned integer
    str_size_bytes = struct.unpack('!Q', sb[offset:offset + 8])[0]

    # Calculate the end index for slicing the string bytes
    end_index = offset + 8 + str_size_bytes

    # Extract and decode the string bytes
    str_bytes = sb[offset + 8:end_index].decode('UTF-8')

    # Return the extracted string and the total consumed bytes
    return str_bytes, end_index + offset


async def find_temp_sensor():
    # Scan for 5 seconds, in active mode, with very low interval/window (to
    # maximise detection rate).
    async with aioble.scan(5000, interval_us=30000, window_us=30000, active=True) as scanner:
        async for result in scanner:
            # See if it matches our name and the environmental sensing service.
            if result.name() == "mpy-temp" and _ENV_SENSE_UUID in result.services():
                return result.device
    return None


async def main():
    device = await find_temp_sensor()
    if not device:
        print("Temperature sensor not found")
        return

    try:
        print("Connecting to", device)
        connection = await device.connect()
    except asyncio.TimeoutError:
        print("Timeout during connection")
        return

    async with connection:
        try:
            temp_service = await connection.service(_ENV_SENSE_UUID)
            temp_characteristic = await temp_service.characteristic(_ENV_SENSE_TEMP_UUID)
        except asyncio.TimeoutError:
            print("Timeout discovering services/characteristics")
            return
        while connection.is_connected():
            led.toggle()
            temp_data, consumed_bytes = _decode_(await temp_characteristic.read())

            if (temp_data.startswith("\\")):
                coords = temp_data[2:].split('Y')
                print(f"X: ", coords[0])
                set_servo_angle(servo, int(coords[0]))
                print(f"Y: ", coords[1])
                set_servo_angle(servo, int(coords[1]))
            
            elif (temp_data == "HeartBeat"):
                continue;
            
            elif (temp_data.startswith("/")):
                command = temp_data[1:]
                if (command.lower() == "restart"):
                    machine.reset()
                if (command.lower() == "stop"):
                    machine.bootloader()
                if (command.lower() == "noop"):
                    print("noop")
            await asyncio.sleep_ms(1000)


asyncio.run(main())
