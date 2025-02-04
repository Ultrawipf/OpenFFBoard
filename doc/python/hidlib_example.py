from openffboard import OpenFFBoard

dev = OpenFFBoard(OpenFFBoard.findDevices()[0])
dev.open()

print(dev.readData(0xA01,0,0)) # Read power
print(dev.writeData(0xA01,0,0,100)) # Set power

dev.close()