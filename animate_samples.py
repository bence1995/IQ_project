import serial
import numpy as np
from matplotlib import pyplot as plt

from matplotlib.animation import FuncAnimation

import csv
import threading
from collections import deque

PORT = "COM5"
filename = "data.csv"

fig = plt.figure()

def listen_on_com(port):
    with serial.Serial(port, 115200) as ser:
        while(1):
            try:
                data = [int(ser.readline().decode("utf-8").rstrip('\n'))]
                append_csv(data)
            except:
                pass

def append_csv(data):
    with open(filename, 'a', newline="") as f:
        writer = csv.writer(f, delimiter = " ")
        writer.writerow(data)

def animate(i):

    q = deque()
    with open(filename) as csvfile:
        q = deque(csvfile, 4000)


    data_I = [int(d.rstrip('\n')) for d in list(q)[::2]] # read data from serial
    data_Q = [int(d.rstrip('\n')) for d in list(q)[1::2]] # read data from serial


    plt.cla()

    plt.plot(data_I, label='Channel 1')
    plt.plot(data_Q, label='Channel 2')

    plt.tight_layout()

x =  threading.Thread(target = listen_on_com, args = (PORT,))
x.start()

ani = FuncAnimation(plt.gcf(), animate, interval=500)

def onPress(event):
    global ani
    if(event.key == "p"):
        print("pause")
        ani.event_source.stop()
    if(event.key == "c"):
        print("continue")
        ani.event_source.start()

fig.canvas.mpl_connect('key_press_event', onPress)


plt.tight_layout()
plt.show()