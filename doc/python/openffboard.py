import hid # hidapi
import struct
import time

CMDTYPE_WRITE=0x00
CMDTYPE_READ= 0x01
CMDTYPE_WRITEADR=0x03
CMDTYPE_READADR= 0x04
CMDTYPE_ACK = 0x0A
CMDTYPE_ERR = 0x07

# HIDAPI version

class OpenFFBoard_base():
    @staticmethod
    def findDevices(vid=0x1209,pid=0xFFB0):
        devices = []
        for dev in hid.enumerate():
            if(dev["vendor_id"] == vid and dev["product_id"] == pid):
                devices.append(dev)

        return devices
        
    def __init__(self,devdict : dict):
        self.devdict = devdict
        self.device = hid.device()        
            
    def printData(cmdtype,cls,inst,cmd,val,addr):
        """Helper callback to print data nicely"""
        print(f"Type: {cmdtype}, Class: {cls}.{inst}: cmd: {cmd}, val: {val}, addr: {addr}")
                
    def open(self):
        """Opens the device"""
        self.device.open(self.devdict["vendor_id"],self.devdict["product_id"])      
        

    def close(self):
        self.device.close()


    def make_command(self,cmdtype,cls,inst,cmd,data=0,adr=0):
        """Generates a command packet"""
        buffer = bytearray()
        buffer += bytearray(struct.pack('B',0xA1)) # HIDCMD
        buffer += bytearray(struct.pack('B',cmdtype)) # type. (0 = write, 1 = read)
        buffer += bytearray(struct.pack('<H',cls))
        buffer += bytearray(struct.pack('B',inst))
        buffer += bytearray(struct.pack('<L',cmd))
        buffer += bytearray(struct.pack('<q',data))
        buffer += bytearray(struct.pack('<q',adr if adr else 0 ))
        return buffer
    
    def parse_command(self,data):
        """Returns a parsed packet as a dict
        Entries: "cmdtype":cmdtype,"cls":cls,"inst":instance,"cmd":cmd,"val":val,"addr":addr
        """
        cmdtype = int(data[1])
        cls = int(struct.unpack('<H', bytes(data[2:4]))[0])
        instance = int(data[4])
        cmd = int(struct.unpack('<L', bytes(data[5:9]))[0])
        val = int(struct.unpack('<q', bytes(data[9:17]))[0])
        addr = int(struct.unpack('<q', bytes(data[17:25]))[0])
        return {"cmdtype":cmdtype,"cls":cls,"inst":instance,"cmd":cmd,"val":val,"addr":addr}


class OpenFFBoard(OpenFFBoard_base):
    def __init__(self,devdict):
        """devdict is one entry of hid api enumerate list"""
        OpenFFBoard_base.__init__(self,devdict)
        self.callback = None

    def writeData(self,cls,inst,cmd,data,adr=None,timeout = 100000):
        """Sends data to the FFBoard. Returns True on success"""
        reply = self.sendCommand(CMDTYPE_WRITE if adr is None else CMDTYPE_WRITEADR,cls=cls ,inst=inst,cmd=cmd,data=data,adr=adr,timeout=timeout)
        return reply["cmdtype"] != CMDTYPE_ERR
    
    def registerReadCallback(self,callback):
        """Register a callback to also call this function on every received reply"""
        self.callback = callback
    
    def readData(self,cls,inst,cmd,adr=None,timeout = 100000):
        """Returns a value from the FFBoard. 
        Returns int for single value replies or a tuple for cmd and addr replies"""
        reply = self.sendCommand(CMDTYPE_READ if adr is None else CMDTYPE_READADR,cls,inst,cmd,0,adr,timeout=timeout)
        if reply:
            if reply["cmdtype"] == CMDTYPE_READ:
                return reply["val"]
            elif reply["cmdtype"] == CMDTYPE_READADR:
                return reply["val"],reply["addr"]
 
    def sendCommand(self,cmdtype,cls,inst,cmd,data=0,adr=0,timeout = 100000):
        self.device.set_nonblocking(False)
        buffer = self.make_command(cmdtype,cls,inst,cmd,data,adr)
        self.device.write(buffer) # Send raw packet

        found = False
        while not found and timeout: # Receive all reports until correct one is found.
            timeout -= 1
            reply = self.device.read(25)
            if reply[0] == 0xA1:
                found = True
                repl = self.parse_command(reply)
                if (repl["cls"] == cls and repl["inst"] == inst and repl["cmd"] == cmd):
                    found = True
        if found:
            if self.callback:
                self.callback(repl["cmdtype"],repl["cls"],repl["inst"],repl["cmd"],repl["val"],repl["addr"])
            if repl:
                return repl

