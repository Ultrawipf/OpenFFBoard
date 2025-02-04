from openffboard import OpenFFBoard
import time

# Make sure FFBoard ist in FFB EXT mode
FX_MANAGER = 0xA03

# Possible effect types:
"""
Constant=1,Ramp=2,Square=3,Sine=4,Triangle=5,Sawtooth Up=6,Sawtooth Down=7,Spring=8,Damper=9,Inertia=10,Friction=11

Conditional effects (spring,damper,inertia,friction) do not use magnitude but a coefficient per axis instead
"""


effects = []

def readData(cmdtype,cls,inst,cmd,val,addr):
    if cls == FX_MANAGER and cmd == 2:
        effects.append(val)
        print("Got new effect at index",val)
        
    print(f"Type: {cmdtype}, Class: {cls}.{inst}: cmd: {cmd}, val: {val}, addr: {addr}")

dev = OpenFFBoard(OpenFFBoard.findDevices()[0])
dev.open()
dev.registerReadCallback(readData)

dev.readData(FX_MANAGER,0,1) # Reset FFB
effects = [] # When reset all effects are reset

dev.writeData(FX_MANAGER,0,0,1) # Enable FFB

### Constant force effect
dev.writeData(FX_MANAGER,0,2,1) # Make new constant force (1) effect
print(effects) # We should have a new effect now
dev.writeData(FX_MANAGER,0,0x5,data=1,adr=effects[0]) #  Enable effect

dev.writeData(FX_MANAGER,0,0x4,data=2000,adr=effects[0]) # Set constant foce magnitude
time.sleep(1)
dev.writeData(FX_MANAGER,0,0x4,data=-2000,adr=effects[0]) # Set constant foce magnitude
time.sleep(1)
dev.writeData(FX_MANAGER,0,0x4,data=0,adr=effects[0]) # Set constant foce magnitude 0
dev.writeData(FX_MANAGER,0,0x5,data=0,adr=effects[0]) #  Disable effect

### Spring effect
dev.writeData(FX_MANAGER,0,2,8) # Make new spring (8) effect
print(effects) # We should have a new effect now
dev.writeData(FX_MANAGER,0,0xB,data=8000,adr=effects[1]) #  Change intensity (coefficient)
dev.writeData(FX_MANAGER,0,0x5,data=1,adr=effects[1]) #  Enable effect
time.sleep(3)
dev.writeData(FX_MANAGER,0,0x8,data=10000,adr=effects[1]) #  Change offset position for axis 0

time.sleep(10)
dev.writeData(FX_MANAGER,0,0,0) # Disable FFB

dev.close()