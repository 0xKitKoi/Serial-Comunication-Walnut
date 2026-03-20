import sys
# ruff: noqa: E402
sys.path.append("")
from micropython import const
import asyncio
import aioble
import bluetooth
import random
import struct

led = machine.Pin("LED", machine.Pin.OUT)
_connected_ = False

# bluetooth boilerplate for specifications.
# org.bluetooth.service.environmental_sensing
_ENV_SENSE_UUID = bluetooth.UUID(0x181A)
# org.bluetooth.characteristic.temperature
_ENV_SENSE_TEMP_UUID = bluetooth.UUID(0x2A6E)
# org.bluetooth.characteristic.gap.appearance.xml
_ADV_APPEARANCE_GENERIC_THERMOMETER = const(768)

# How frequently to send advertising beacons.
_ADV_INTERVAL_MS = 250_000


# Register GATT server.
temp_service = aioble.Service(_ENV_SENSE_UUID)
temp_characteristic = aioble.Characteristic(
    temp_service, _ENV_SENSE_TEMP_UUID, read=True, notify=True
)
aioble.register_services(temp_service)

# take the string, turn into bytes and pack it with the length for parsing on ther other side. 
def _encode_(temp):
    str_bytes = temp.encode('UTF-8')
    str_size = struct.pack('!Q', len(str_bytes))
    return str_size + str_bytes


async def sensor_task():
    while 1:
        led.toggle()
        if _connected_:
            t = input("-> ")
            if (t.lower() == "help"):
                print("Pico w Servo Communications\n\tSend Coords: \\X(int)Y(int)\n\tHalt operations: /noop\n\tReboot: /reboot\n\tBootloader: /stop")
            elif (t.startswith("/") or t.startswith("\\")):
                temp_characteristic.write(_encode_(t), send_update=True)
        await asyncio.sleep_ms(1000)


# Serially wait for connections. Don't advertise while a central is
# connected.
async def peripheral_task():
    global _connected_
    while True:
        async with await aioble.advertise(
            _ADV_INTERVAL_MS,
            name="mpy-temp",
            services=[_ENV_SENSE_UUID],
            appearance=_ADV_APPEARANCE_GENERIC_THERMOMETER,
        ) as connection:
            print("Connection from", connection.device)
            _connected_ = True
            temp_characteristic.write(_encode_("HeartBeat"), send_update=False)
            await connection.disconnected(timeout_ms=None)


# Run both tasks.
async def main():
    t1 = asyncio.create_task(sensor_task())
    t2 = asyncio.create_task(peripheral_task())
    await asyncio.gather(t1, t2)


asyncio.run(main())


