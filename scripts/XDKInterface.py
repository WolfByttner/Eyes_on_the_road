#!/usr/bin/env python
import serial
import sys
import time


import os
dirs = os.path.abspath(os.path.dirname(__file__))
sys.path.append(dirs + "/../tutorial")
print sys.path
from kfkdfull import *
import threading
global our_queue
our_queue = [0]
t = threading.Thread(target=plot_video, kwargs={"data_queue":our_queue})
t.start()
lock = threading.Lock()

ser = serial.Serial()
ser.port = "/dev/ttyACM0" # may be called something different
ser.baudrate = 19200 # may be different
ser.open()
initAltitude = -1
accX = [0]*100
accY = [0]*100
accZ = [0]*100
acc_idx = 0
gyrX = [0]*100
gyrY = [0]*100
gyrZ = [0]*100
mem_abs_gyrZ = [0]*5
mem_abs_gyrZ_idx = 0
abs_gyrZ_avg5 = 0
gyrZ_score = -1
gyr_idx = 0
start_time = 0
decision_vector = [0] * 6 # [Camera, Gyro, Light, Temperature, Humidity, Altitude]

def refresh_vector():
                print "Camera:      ", "%0.2f" % decision_vector[0]
                print "Gyro:        ", "%0.2f" % decision_vector[1]
                print "Light:       ", "%0.2f" % decision_vector[2]
                print "Temperature: ", "%0.2f" % decision_vector[3]
                print "Humidity:    ", "%0.2f" % decision_vector[4]
                print "Altitude:    ", "%0.2f" % decision_vector[5]
                print "-----------------------------"

while True:
        if ((time.time() - start_time)>1):

                refresh_vector()
                start_time = time.time()
        identifier = ser.read(3)
        lock.acquire()
        try:
            decision_vector[0] = our_queue[0]
            #print our_queue
        except Exception as e:
            print e
            pass
        finally:
            lock.release()
        
        dv = decision_vector
        active = (dv[1] + dv[0]) * 1.6
        passive = (dv[2] * 3 + dv[3] + dv[4] + dv[5]) * 0.4
        #if active >= .5 and passive >= .5:
        if active + passive > 1.6:
            print "Sending sleep"
            ser.write('sleep')
            #exit(0)
        #time.sleep(3)
        #ser.write('sleep')
        #print "sent sleep"
        #identifier = ser.readline()
        #print "Got back:", identifier
        #break
        if(identifier == 'ACC'):
                ser.read(2) # colon and space
                response = ser.readline()
##                response = response.strip('\n')
##                accData = response.split(',',3)
##                # print "Acceleration: ", accData
##                try:
##                        accX[acc_idx] = round(float(accData[0]))
##                except:
##                        accX[acc_idx] = int(accData[0])
##                try:
##                        accY[acc_idx] = round(float(accData[1]))
##                except:
##                        accY[acc_idx] = int(accData[1])
##                try:
##                        accZ[acc_idx] = round(float(accData[2]))
##                except:
##                        accZ[acc_idx] = int(accData[2])
##                acc_idx += 1
##                if (acc_idx == 100):
##                        acc_idx = 0
##                        elapsed_time = time.time() - start_time
##                        # print "---------------", elapsed_time
##                        start_time = time.time()

        elif(identifier == 'GYR'):
                ser.read(2) # colon and space
                response = ser.readline()
                response = response.strip('\n')
                gyrData = response.split(',',3)
                # print "Gyro Data: ", gyrData
                try:
                        gyrX[gyr_idx] = round(float(gyrData[0]))
                except:
                        gyrX[gyr_idx] = int(gyrData[0])
                try:
                        gyrY[gyr_idx] = round(float(gyrData[1]))
                except:
                        gyrY[gyr_idx] = int(gyrData[1])
                try:
                        gyrZ[gyr_idx] = round(float(gyrData[2]))
                except:
                        gyrZ[gyr_idx] = int(gyrData[2])
                if (gyr_idx > 2):
                        gyrZdiff3 = gyrZ[gyr_idx] - gyrZ[gyr_idx-3]
                        mem_abs_gyrZ[mem_abs_gyrZ_idx] = abs(gyrZdiff3) / 5.0
                        abs_gyrZ_avg5 += mem_abs_gyrZ[mem_abs_gyrZ_idx]
                        mem_abs_gyrZ_idx = (mem_abs_gyrZ_idx+1)%5
                        abs_gyrZ_avg5 -= mem_abs_gyrZ[mem_abs_gyrZ_idx]
                        if (((abs_gyrZ_avg5 - 50) / 50)>gyrZ_score):
                                gyrZ_score = (abs_gyrZ_avg5 - 50) / 50
                        else:
                                gyrZ_score = gyrZ_score - (gyrZ_score + 1)*0.05
                        if (gyrZ_score>1):
                                gyrZ_score = 1
                        #print gyrZ_score
                        decision_vector[1] = gyrZ_score
                gyr_idx += 1
                if (gyr_idx == 100):
                        gyr_idx = 0
                        #elapsed_time = time.time() - start_time
                        #start_time = time.time()               

        elif(identifier == 'ENV'):
                ser.read(2) # colon and space
                response = ser.readline()
                response = response.strip('\n')
                envData = response.split(',',3)
                # print "Enviroment Data: ",envData
                try:
                        altData = round(float(envData[0]))
                except:
                        altData = int(envData[0])
                temData = int(envData[1])/1000.0
                humData = int(envData[2])
                
                if (initAltitude == -1):
                        initAltitude = altData
                        
                if (altData < (initAltitude-1000)):
                        altScore = -1.0
                elif (altData < (initAltitude+1000)):
                        altScore = (altData - initAltitude) / 1000.0
                else:
                        altScore = 1.0
                    
                if (temData<13):
                        temScore = -1.0
                elif (temData<18):
                        temScore = (temData-18.0)/5.0
                elif (temData<20):
                        temScore = (temData-18.0)/2.0
                elif (temData<22):
                        temScore = (22.0-temData)/2.0
                elif (temData<29):
                        temScore = (22.0-temData)/7.0
                else:
                        temScore = -1.0

                if (humData<25):
                        humScore = -1.0
                elif (humData<45):
                        humScore = (humData-35.0)/10.0
                elif (humData<65):
                        humScore = (55.0-humData)/10.0
                else:
                        humScore = -1.0
                        
                #print "Altitude:    ", altData, "Score: ", altScore
                #print "Temperature: ", temData, "Score: ", temScore
                #print "Humidity:    ", humData, "Score: ", humScore
                decision_vector[3] = temScore
                decision_vector[4] = humScore
                decision_vector[5] = altScore
        elif(identifier == 'LIG'):
                ser.read(2) # colon and space
                response = ser.readline()
                response = response.strip('\n')
                litData = response.split(',',1)
                litData = int(litData[0],16)
                if(litData<0x650):
                        litScore = (1616.0 - litData) / 80.0
                elif(litData<0x750):
                        litScore = (litData - 1872.0) / 256.0
                else:
                        litScore = -1.0
                #print "Lighting:    ", litData, "Score: ", litScore
                decision_vector[2] = litScore


