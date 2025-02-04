import pywinusb.hid as hid
import struct
import time

CMDTYPE_WRITE=0x00
CMDTYPE_READ= 0x01
CMDTYPE_WRITEADR=0x03
CMDTYPE_READADR= 0x04

# Pywinusb version

class OpenFFBoard():
    @staticmethod
    def findDevices(vid=0x1209,pid=0xFFB0):
        return hid.HidDeviceFilter(vendor_id = vid,product_id = pid).get_devices()
        
    def __init__(self,device : hid.HidDevice):
        self.device = device
        self.callback = self.printData
        self.device.set_raw_data_handler(self.readDataCallback)
            
    def printData(cmdtype,cls,instance,cmd,val,addr):
        print(f"Type: {cmdtype}, Class: {cls}.{instance}: cmd: {cmd}, val: {val}, addr: {addr}")
                
    def open(self):
        self.device.open()        

    def close(self):
        self.device.close()

    def registerReadCallback(self,callback):
        self.callback = callback


    # Callback in format callback(cmdtype,cls,instance,cmd,val,addr)
    def readDataCallback(self,data):
        if(data[0] == 0xA1):
            self.done = True
            cmdtype = int(data[1])
            cls = int(struct.unpack('<H', bytes(data[2:4]))[0])
            instance = int(data[4])
            cmd = int(struct.unpack('<L', bytes(data[5:9]))[0])
            val = int(struct.unpack('<Q', bytes(data[9:17]))[0])
            addr = int(struct.unpack('<Q', bytes(data[17:25]))[0])
            self.callback(cmdtype,cls,instance,cmd,val,addr)

    def writeData(self,cls,inst,cmd,data,adr=None,wait=True):
        self.sendCommand(CMDTYPE_WRITE if adr == None else CMDTYPE_WRITEADR,cls=cls ,inst=inst,cmd=cmd,data=data,adr=adr,wait=wait)
    
    def readData(self,cls,inst,cmd,adr=None,wait=True):
        self.sendCommand(CMDTYPE_READ if adr == None else CMDTYPE_READADR,cls,inst,cmd,0,adr,wait=wait)

    def sendCommand(self,type,cls,inst,cmd,data=0,adr=0,wait=True):
        reports = self.device.find_output_reports(0xff00,0x01)
        if(reports):
            self.done = False
            report = reports[0]
            report[hid.get_full_usage_id(0xff00, 0x01)]=type # type. (0 = write, 1 = read)
            report[hid.get_full_usage_id(0xff00, 0x02)]=cls # cls (axis)
            report[hid.get_full_usage_id(0xff00, 0x03)]=inst # instance
            report[hid.get_full_usage_id(0xff00, 0x04)]=cmd # cmd (power)
            report[hid.get_full_usage_id(0xff00, 0x05)]=data # data
            report[hid.get_full_usage_id(0xff00, 0x06)]=adr if adr else 0 # adr
            report.send()
            while not self.done and wait:
                time.sleep(0.001)
            