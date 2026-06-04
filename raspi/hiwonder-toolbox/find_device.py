#!/usr/bin/env python3
import os
import re
import sys
import socket
import importlib
import subprocess 
def get_cpu_serial_number():
    address = "/sys/class/net/eth0/address"
    if os.path.exists(address):
        with open(address, 'r') as f:
            serial_num = f.read().replace('\n', '').replace(':', '').upper()
            serial_num = serial_num[-8:]
    else:
        device_serial_number = open("/proc/device-tree/serial-number")
        serial_num = device_serial_number.readlines()[0][-10:-1]
    return serial_num

def update_globals(module):
    if module in sys.modules:
        mdl = importlib.reload(sys.modules[module])
    else:
        mdl = importlib.import_module(module)
    if "__all" in mdl.__dict__:
        names = mdl.__dict__["__all__"]
    else:
        names = [x for x in mdl.__dict__ if not x.startswith("_")]
    globals().update({k: getattr(mdl, k) for k in names})

        
def get_typerc(container_id):  
    result = subprocess.run(['docker', 'exec', container_id, 'cat', '/home/ubuntu/ros2_ws/.typerc'],  
                            capture_output=True, text=True, check=True)  
    data = result.stdout  
      
    machine = re.findall(r'export MACHINE_TYPE.*?\n', data)[0].split('=')[1].replace('\n', '')
        
    app_version = re.findall(r'export VERSION.*?\n', data)[0].split('=')[1].split('|')[2][4:].replace('\n', '')  
      
    return machine, app_version 
    
if __name__ == "__main__":
    host = '0.0.0.0'
    port = 9027
    container_id = 'adb8457c2eec' 
    machine_type, APP_VERSION = get_typerc(container_id)
    print(APP_VERSION)
    if machine_type == "MentorPi_Mecanum": 
        robot_type = 'MENTORPI_MECANUM'
    elif machine_type == 'MentorPi_Acker':
        robot_type = 'MENTORPI_ACKER_1.0'

   
    sn = get_cpu_serial_number()
    WIFI_AP_SSID = ''.join(["HW-", sn[0:8]])
    WIFI_STA_SSID = ""

    path = os.path.split(os.path.realpath(__file__))[0]
    config_file_name = "wifi_conf.py"
    external_config_file_dir_path = path
    external_config_file_path = os.path.join(external_config_file_dir_path, config_file_name)
    if os.path.exists(external_config_file_path):
        sys.path.insert(0, external_config_file_dir_path)
        update_globals(os.path.splitext(config_file_name)[0])

    sn = WIFI_AP_SSID[3:].ljust(32, '0')

    udpServer = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udpServer.bind((host, port))
    while True:
        data, addr = udpServer.recvfrom(1024)
        msg = str(data, encoding = 'utf-8')
        print(robot_type +":" + sn, addr)
        print(WIFI_AP_SSID)
        if msg == "LOBOT_NET_DISCOVER":
            udpServer.sendto(bytes(robot_type +":" + sn + "\n", encoding='utf-8'), addr)

