import sys
import serial
from xmodem import XMODEM

serial_port = '/dev/ttyUSB0'
baud_rate = 115200  # In arduino, Serial.begin(baud_rate)

ser = serial.Serial(serial_port, baud_rate)


def getc(size, timeout=1):
    return ser.read(size) or None


def putc(data, timeout=1):
    return ser.write(data)  # note that this ignores the timeout


print("Press d for dump ROM else CTRL+C to exit.")
ch = sys.stdin.read(1)

if ch == '0':
    ser.write('0')
print ser.readline().decode()
print ser.readline().decode()
print ser.readline().decode()
print ser.readline().decode()
print ser.readline().decode()

ch2 = sys.stdin.read(2)
ser.write('r')

ch3 = sys.stdin.read(2)

print ser.readline().decode()
print ser.readline().decode()

modem = XMODEM(getc, putc)

stream = open('output.rom', 'wb')
modem.recv(stream)
