import hid

h = hid.device()
for device_dict in hid.enumerate(vendor_id=0x4249, product_id=0x4287 ):
    if(device_dict['interface_number'] == 1):
        path = device_dict['path']
        print("Opening device:" + device_dict["product_string"])
        h.open_path(path)
        break

while(True):
    for i in range(0, 100):
        
        x = i / 100
        h.write([1,int(x * 0xFD), int(x * 0x80) ,int(x * 0x46), int(x * 0x80), int(x * 0x45), int(x * 0x65), int(x * 0x2d),int(x * 0x1d),int(x * 0x7a)])
    for i in range(100, 0, -1):
        
        x = i / 100
        h.write([1,int(x * 0xFD), int(x * 0x80) ,int(x * 0x46), int(x * 0x80), int(x * 0x45), int(x * 0x65), int(x * 0x2d),int(x * 0x1d),int(x * 0x7a)])

h.write([1 ,0xFD, 0x80 ,0x46, 0x80, 0x45, 0x65, 0x2d,0x1d,0x7a])

h.close()