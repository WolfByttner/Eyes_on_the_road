#!/usr/bin/env python
import serial
import sys
import time

ser = serial.Serial()
ser.port = "/dev/ttyACM0" # may be called something different
ser.baudrate = 19200 # may be different
ser.open()


while True:
	#bytesToRead = ser.inWaiting()
	identifier = ser.read(3)

	if(identifier == 'ACC'):
		ser.read(2) # colon and space
		response = ser.readline()
		response = response.strip('\n')
		accData = response.split(',',3)
		print "Acceleration: ", accData

	elif(identifier == 'GYR'):
		ser.read(2) # colon and space
		response = ser.readline()
		response = response.strip('\n')
		gyrData = response.split(',',3)
		print "Gyro Data: ", gyrData

	elif(identifier == 'ENV'):
		ser.read(2) # colon and space
		response = ser.readline()
		response = response.strip('\n')
		envData = response.split(',',3)
		print "Enviroment Data: ",envData

	elif(identifier == 'LIG'):
		ser.read(2) # colon and space
		response = ser.readline()
		response = response.strip('\n')
		litData = response.split(',',1)
		print "Lighting: ", litData



