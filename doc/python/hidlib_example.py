from openffboard import OpenFFBoard
import time

def readData(cmdtype,cls,instance,cmd,val,addr):
    print(f"Type: {cmdtype}, Class: {cls}.{instance}: cmd: {cmd}, val: {val}, addr: {addr}")

dev = OpenFFBoard(OpenFFBoard.findDevices()[0])
dev.open()
dev.registerReadCallback(readData)

dev.readData(0xA01,0,0) # Read power
dev.writeData(0xA01,0,0,100) # Set power

time.sleep(0.5) # Wait until report is sent
dev.close()