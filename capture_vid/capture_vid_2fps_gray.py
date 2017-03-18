# Using OpenCV 3.2.0.6 and Python 2.7.13 

import numpy as np
import cv2
import time
import pygame

cap = cv2.VideoCapture(0)

pygame.mixer.init()
pygame.mixer.music.load("C:\\Users\\Patroklos\\Downloads\\StartHack2017\\Eyes_on_the_road\\capture_vid\\eyes_on_road.mp3")

while(True):
    # Capture frame-by-frame
    ret, frame = cap.read()

    # Our operations on the frame come here
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Display the resulting frame
    cv2.imshow('frame',gray)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        pygame.mixer.music.play()
        while pygame.mixer.music.get_busy() == True:
            continue
        break
    time.sleep(0.4)

# When everything done, release the capture
cap.release()
cv2.destroyAllWindows()
