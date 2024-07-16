import machine
import time
led = machine.Pin("LED", machine.Pin.OUT)
servoPin = 16
servo = machine.PWM(machine.Pin(servoPin))
servo.freq(50)
on = True


def map_value(x, in_min, in_max, out_min, out_max):
    return (x - in_min) * (out_max - out_min) // (in_max - in_min) + out_min # magic

def set_servo_angle(servo, angle):
    min_duty = 1200  # This is 0 degrees?
    max_duty = 7000  # this seems to me 180 degrees
    
    # Calculate duty for desired angle
    duty = map_value(angle, 0, 180, min_duty, max_duty)
    print(f"Setting angle to {angle}, duty cycle to {duty}")  # Debug print
    # Set the duty cycle
    #servo.duty_u16(int(duty * 65535 / 1023)) // garbage,doesnt work
    servo.duty_u16(int(duty)) # seems to work correctly

# Example usage: Move servo to 0, 90, and 180 degrees
angles = [0, 45, 90, 135, 180, 135, 90]
#servo.duty_u16(4100)

for angle in angles:
    set_servo_angle(servo, angle)
    led.on()
    time.sleep(1)
    led.off()
    time.sleep(1)


while True:
    angle=int(input('-> '))
    #writeVal = 6553/180*angle+1638
    #servo.duty_u16(int(writeVal))
    set_servo_angle(servo, angle)
    if on:
        led.off()
        on = False
    else:
        on = True
        led.on()
    time.sleep(0.5)