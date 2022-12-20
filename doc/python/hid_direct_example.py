import pywinusb.hid as hid
import time
import struct

# Receive and print data
def readData(data):
    if(data[0] == 0xA1):
        t = data[1]
        cls = struct.unpack('<H', bytes(data[2:4]))
        instance = data[4]
        cmd = struct.unpack('<L', bytes(data[5:9]))
        val = struct.unpack('<Q', bytes(data[9:17]))
        addr = struct.unpack('<Q', bytes(data[17:25]))
        print(f"Type: {t}, Class: {cls}.{instance}: cmd: {cmd}, val: {val}, addr: {addr}")


def sendCommand(report,type,cls,inst,cmd,data=0,adr=0):
    report[hid.get_full_usage_id(0xff00, 0x01)]=type # type. (0 = write, 1 = read)
    report[hid.get_full_usage_id(0xff00, 0x02)]=cls # cls (axis)
    report[hid.get_full_usage_id(0xff00, 0x03)]=inst # instance
    report[hid.get_full_usage_id(0xff00, 0x04)]=cmd # cmd (power)
    report[hid.get_full_usage_id(0xff00, 0x05)]=data # data
    report[hid.get_full_usage_id(0xff00, 0x06)]=adr # adr
    report.send()

def main():
    device = hid.HidDeviceFilter(vendor_id = 0x1209,product_id = 0xFFB0).get_devices()[0]
    device.open()

    device.set_raw_data_handler(readData)

    reports = device.find_output_reports(0xff00,0x01)
    if(reports):
        report = reports[0]
        print("Found",report)
        sendCommand(report,1,0xA01,0,0x00,0) # get power
        sendCommand(report,0,0xA01,0,0x00,500) # set power
        sendCommand(report,1,0xA01,0,0x00,0) # get power

        #sendCommand(report,1,0x0,0,0x08,0) # lsactive

    while device.is_plugged():
        time.sleep(0.5)
    device.close()

if __name__ == '__main__':
    main()
