#!/usr/bin/env python3
import os
import sys
import time
import logging
import netifaces
import importlib
import threading
import subprocess
import socketserver
from find_device import get_cpu_serial_number
import ros_robot_controller_sdk as rrc

# Get the current script's path
path = os.path.split(os.path.realpath(__file__))[0]
log_file_path = os.path.join(path, "wifi.log")

# Create a log file (if it does not exist).
if not os.path.exists(log_file_path):
    os.system(f'touch {log_file_path}')

# Configuration file path
config_file_name = "wifi_conf.py"
internal_config_file_dir_path = "/etc/wifi"
external_config_file_dir_path = path
internal_config_file_path = os.path.join(internal_config_file_dir_path, config_file_name)
external_config_file_path = os.path.join(external_config_file_dir_path, config_file_name)

# global variables
ros_control_enabled = False
current_wifi_mode = None       # Used to record the current network pattern
server = None                  # Global server instance
ip = None                      # Current IP address

# Initialize Board instance
board = rrc.Board()

# Configure log recording
logger = logging.getLogger("WiFi工具")
logger.setLevel(logging.DEBUG)
log_handler = logging.FileHandler(log_file_path)
log_handler.setLevel(logging.INFO)
log_formatter = logging.Formatter('%(name)s - %(asctime)s - %(levelname)s - %(message)s')
log_handler.setFormatter(log_formatter)
logger.addHandler(log_handler)


##### MACHINE TYEP SET ######
def set_machine_ros():
    ros_machine_command = "ros2 service call /ros_robot_controller/set_machine_type std_srvs/srv/Trigger '{}'"
    full_command = [
        'docker', 'exec', '-u', 'ubuntu', '-w', '/home/ubuntu', 'MentorPi',
        '/bin/zsh', '-c', f"source /home/ubuntu/.zshrc && {ros_machine_command}"
    ]
    result = subprocess.run(full_command, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def get_machine_type_from_config(config_path):
    try:
        with open(config_path, 'r') as f:
            for line in f:
                line = line.strip()
                match = re.match(r'^\s*export\s+MACHINE_TYPE\s*=\s*["\']?(.*?)["\']?\s*$', line)
                if match:
                    return match.group(1)
            raise ValueError(f"MACHINE_TYPE IS None")
    except FileNotFoundError:
        raise FileNotFoundError(f"CONFIG PATH IS ERROR")


def send_machine_type(board,machine_type):
    if 'Tank' in machine_type:
        motor_type = 0x01
        battery_level = 0x1af4
    elif 'Acker' in machine_type or 'Mecanum' in machine_type:
        motor_type = 0x02
        battery_level = 0x1af4
    else:
        motor_type = 0x09
        battery_level = 0x1af4
    if motor_type is not None:
        for i in range(2):
            '''
            MOTOR_TYPE_JGB520 0x00
            MOTOR_TYPE_JGB37  0x01
            MOTOR_TYPE_JGB27  0x02
            MOTOR_TYPE_JGB528 0x03
            '''
            board.set_motor_type(motor_type)
            board.set_battery_level(battery_level)
    else:
        print('Please Set the machine_type')

def set_machine_serial(board):
    config_path = "/home/pi/docker/tmp/.typerc"
    machine_type = get_machine_type_from_config(config_path)
    send_machine_type(board,machine_type)

###################################


def update_globals(module):
    """
    Dynamically import or reload modules and update the variables in the modules to the global namespace.
    """
    if module in sys.modules:
        mdl = importlib.reload(sys.modules[module])
    else:
        mdl = importlib.import_module(module)
    if "__all__" in mdl.__dict__:
        names = mdl.__dict__["__all__"]
    else:
        names = [x for x in mdl.__dict__ if not x.startswith("_")]
    globals().update({k: getattr(mdl, k) for k in names})

def get_wifi_ip():
    """
    Get the IP address of the currently connected WiFi interface.
    """
    interfaces = netifaces.interfaces()

    for interface in interfaces:
        if interface.startswith('wlan'):
            addresses = netifaces.ifaddresses(interface)
            if netifaces.AF_INET in addresses:
                ip_info = addresses[netifaces.AF_INET][0]
                ip_address = ip_info['addr']
                return ip_address

    return None

def reset_wifi():
    """
    Reset the WiFi configuration and restart the WiFi service.
    """
    os.system("sudo rm /etc/wifi/* -rf > /dev/null 2>&1")
    os.system("sudo systemctl restart wifi.service > /dev/null 2>&1")
    logger.info("WiFi has been reset and the service restarted.")
class TCPHandler(socketserver.BaseRequestHandler):
    """
    A TCP request handling class used to receive commands and execute corresponding operations.
    """
    def handle(self):
        self.request.settimeout(2)
        data = b""
        while True:
            try:
                chunk = self.request.recv(1024)
                if not chunk:
                    break
                data += chunk
                if data.strip() == b"resetwifi":
                    logger.info("Received reset WiFi command.")
                    reset_wifi()
                    break
            except socket.timeout:
                logger.warning('TCP connection timed out.')
                break
            except Exception as e:
                logger.error(f"Error during TCP processing: {e}")
                break
        self.request.close()

class PhoneServer(socketserver.TCPServer):
    """
    A customized TCP server class.
    """
    def __init__(self, server_address, RequestHandlerClass):
        super().__init__(server_address, RequestHandlerClass)

    def handle_timeout(self):
        logger.warning('TCP server timed out.')

def start_server(ip, port):
    """
    Start the TCP server.
    """
    server = PhoneServer((ip, port), TCPHandler)
    thread = threading.Thread(target=server.serve_forever)
    thread.daemon = True
    thread.start()
    logger.info(f"The TCP server has been started. {ip}:{port}")
    return server

def stop_server(server):
    """
    Stop the TCP server.
    """
    if server:
        server.shutdown()
        server.server_close()
        logger.info("The TCP server has been stopped.")
    return None

def check_ros2_control():
    """
    Check if the ROS2 control node exists to determine the LED control method.
    """
    global ros_control_enabled
    try:
        process = subprocess.Popen(
            ["docker", "exec", '-u', 'ubuntu', '-w', '/home/ubuntu', 'MentorPi', 
             '/bin/zsh', '-c', 'source /home/ubuntu/.zshrc && ros2 topic list'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        stdout, stderr = process.communicate(timeout=5)  # 设置超时为5秒
        topics = stdout.decode().split('\n')
        if '/ros_robot_controller/set_led' in topics:
            ros_control_enabled = True
            logger.info("Detected ROS control node, enabling ROS control of LEDs.")
        else:
            ros_control_enabled = False
            logger.info("ROS control node not detected, using local LED control.")
    except subprocess.TimeoutExpired:
        process.kill()
        ros_control_enabled = False
        logger.warning("Detection of ROS2 control timed out, using local LED control.")
    except Exception as e:
        ros_control_enabled = False
        logger.error(f"Error occurred while detecting ROS2 node: {e}")

def set_led(led_id, on_time, off_time, repeat=0):
    """
    Set the LED status and select different control methods depending on whether ROS control is enabled.
    """
    if ros_control_enabled:
        ros_command = f"ros2 topic pub /ros_robot_controller/set_led ros_robot_controller_msgs/msg/LedState '{{id: {led_id}, on_time: {on_time}, off_time: {off_time}, repeat: {repeat}}}' --once"
        full_command = [
            'docker', 'exec', '-u', 'ubuntu', '-w', '/home/ubuntu', 'MentorPi',
            '/bin/zsh', '-c', f"source /home/ubuntu/.zshrc && {ros_command}"
        ]
        logger.info(f"Executing ROS command: {' '.join(full_command)}")
        result = subprocess.run(full_command, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        set_machine_ros()
        if result.returncode != 0:
            logger.error(f"Error occurred while publishing ROS 2 message: {result.stderr.decode('utf-8')}")
    else:
        try:
            board.set_led(on_time, off_time, repeat=repeat, led_id=led_id)
            logger.info(f"Using rrc.Board for local LED control: id={led_id}, on_time={on_time}, off_time={off_time}, repeat={repeat}")
            set_machine_serial(board) 
        except Exception as e:
            logger.error(f"Error occurred while controlling LED locally: {e}")

def get_connect():
    """
    Get the name of the currently connected WiFi network.
    """
    try:
        result = subprocess.run(['nmcli', '-t', 'con', 'show', '--active'], stdout=subprocess.PIPE)
        active_conns = result.stdout.decode().split('\n')
        for conn in active_conns:
            if conn:
                conn_details = conn.split(':')
                if len(conn_details) >= 3 and 'wireless' in conn_details[2]:
                    wifi = conn_details[0]
                    return wifi
    except Exception as e:
        logger.error(f"Error occurred while fetching WiFi connection: {e}")
    return None

def disconnect():
    """
    Disconnect the current WiFi connection.
    """
    try:
        wifi = get_connect()
        if wifi:
            os.system(f'nmcli connection down {wifi}')
            os.system(f'nmcli connection delete {wifi}')
            os.system('rm /etc/NetworkManager/system-connections/*')
            logger.info(f"Disconnected and deleted WiFi connection: {wifi}")
    except Exception as e:
        logger.error(f"Error occurred while disconnecting WiFi connection: {e}")

def setup_ap_mode():
    """
    The specific implementation of switching to AP mode.
    """
    global current_wifi_mode, ip, server

    current_wifi_mode = 1
    set_led(1, 1, 0, 1)  # LED1常亮
    set_led(2, 0.5, 0.5, 0)  # LED2闪烁

    disconnect()

    # 配置AP模式
    try:
        os.system(f'nmcli con add type wifi ifname wlan0 con-name {WIFI_AP_SSID} autoconnect yes ssid {WIFI_AP_SSID}')
        os.system(f'nmcli con modify {WIFI_AP_SSID} 802-11-wireless.mode ap ipv4.method shared')
        os.system(f'nmcli con modify {WIFI_AP_SSID} wifi-sec.key-mgmt wpa-psk wifi-sec.psk {WIFI_AP_PASSWORD}')
        os.system(f'nmcli con modify {WIFI_AP_SSID} wifi.band {WIFI_FREQ_BAND} wifi.channel {WIFI_CHANNEL}')
        os.system(f'nmcli con modify {WIFI_AP_SSID} ipv4.addresses {WIFI_AP_GATEWAY}/24')
        logger.info(f"配置AP模式: SSID={WIFI_AP_SSID}, 密码={WIFI_AP_PASSWORD}, 频段={WIFI_FREQ_BAND}, 频道={WIFI_CHANNEL}")

        # 等待AP模式生效
        timeout = 0
        while True:
            timeout += 1
            wifi = get_connect()
            if wifi == WIFI_AP_SSID:
                logger.info(f"AP创建成功: {WIFI_AP_SSID}")
                ip = WIFI_AP_GATEWAY
                server = start_server(ip, 9028)
                break
            if timeout == 20:
                logger.warning("重启NetworkManager...")
                os.system('systemctl restart NetworkManager')
            if timeout > 20:
                logger.error("无法创建AP，重启系统...")
                os.system('reboot')
                break
            time.sleep(1)
    except Exception as e:
        logger.error(f"设置AP模式时出错: {e}")

def setup_client_mode():
    """
    切换到客户端模式的具体实现。
    """
    global current_wifi_mode, ip, server

    current_wifi_mode = 2
    set_led(1, 1, 0, 1)  # LED1常亮
    set_led(2, 0.05, 0.05, 0)  # LED2 50ms闪烁

    disconnect()

    retry_count = 0
    max_retries = 3

    # 尝试连接客户端模式三次
    while retry_count < max_retries:
        try:
            p = subprocess.Popen(['nmcli', 'device', 'wifi', 'connect', WIFI_STA_SSID, 'password', WIFI_STA_PASSWORD], 
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = p.communicate(timeout=10)  # 设置超时为10秒
            if p.returncode == 0:
                logger.info(f"成功连接到WiFi: {WIFI_STA_SSID}")
                break
            else:
                retry_count += 1
                logger.warning(f"连接到WiFi失败: {stderr.decode('utf-8')}. 重试 {retry_count}/{max_retries}")
                time.sleep(2)
        except subprocess.TimeoutExpired:
            p.kill()
            retry_count += 1
            logger.warning(f"连接到WiFi超时。重试 {retry_count}/{max_retries}")
        except Exception as e:
            p.kill()
            retry_count += 1
            logger.error(f"连接到WiFi时出错: {e}. 重试 {retry_count}/{max_retries}")

    if retry_count >= max_retries:
        logger.error(f"无法连接到SSID: {WIFI_STA_SSID}，切换到AP模式。")
        # 删除 /etc/wifi/* 文件
        try:
            os.system("sudo rm /etc/wifi/* -rf > /dev/null 2>&1")
            logger.info("已删除 /etc/wifi/* 文件。")
        except Exception as e:
            logger.error(f"删除 /etc/wifi/* 文件时出错: {e}")
        # 切换到AP模式
        setup_ap_mode()
        return

    # 检查是否成功连接并获取IP
    timeout = 0
    while True:
        ssid_name = get_connect()
        if ssid_name == WIFI_STA_SSID:
            logger.info(f"已连接到 {WIFI_STA_SSID}")
            set_led(1, 1, 0, 1)  # LED1常亮
            set_led(2, 1, 0, 1)  # LED2常亮，表示成功连接

            # 获取IP地址
            while True:
                wifi_ip = get_wifi_ip()
                if wifi_ip:
                    if wifi_ip != ip:
                        ip = wifi_ip
                        if server:
                            server = stop_server(server)
                        server = start_server(ip, 9028)
                    break
                time.sleep(1)
            break
        else:
            if timeout >= WIFI_TIMEOUT:
                logger.error(f"无法连接到SSID: {WIFI_STA_SSID}，切换到AP模式。")
                # 删除 /etc/wifi/* 文件
                try:
                    os.system("sudo rm /etc/wifi/* -rf > /dev/null 2>&1")
                    logger.info("已删除 /etc/wifi/* 文件。")
                except Exception as e:
                    logger.error(f"删除 /etc/wifi/* 文件时出错: {e}")
                # 切换到AP模式
                setup_ap_mode()
                break
            timeout += 1
            time.sleep(1)

def WIFI_MGR():
    """
    管理WiFi连接的主逻辑，根据WIFI_MODE进行模式切换。
    """
    global WIFI_AP_SSID, WIFI_STA_SSID, WIFI_AP_PASSWORD, WIFI_STA_PASSWORD
    global server, ip, current_wifi_mode

    check_ros2_control()  # 在WiFi管理器启动时检查ROS控制

    if WIFI_MODE == 1:  # AP模式
        setup_ap_mode()
    elif WIFI_MODE == 2:  # 客户端模式
        setup_client_mode()
    else:
        logger.error("无效的WIFI_MODE")
        set_led(1, 1, 0, 1)  # LED1常亮
        set_led(2, 0.5, 0.5, 0)  # LED2闪烁，表示错误

def monitor_ros_node():
    """
    监控ROS节点的线程函数，动态调整LED状态。
    """
    global ros_control_enabled
    global current_wifi_mode
    ros_was_enabled = False  # 记录上一次的ROS控制状态

    while True:
        check_ros2_control()

        if ros_control_enabled:
            if not ros_was_enabled:
                logger.info("检测到ROS控制，发送LED指令...")
                if current_wifi_mode == 1:
                    set_led(1, 1, 0, 1)  # AP模式: LED1常亮
                    set_led(2, 0.5, 0.5, 0)  # AP模式: LED2闪烁
                elif current_wifi_mode == 2:
                    wifi_connected = get_connect()
                    if wifi_connected:
                        set_led(1, 1, 0, 1)  # 客户端模式: LED1常亮
                        set_led(2, 1, 0, 1)  # 客户端模式: LED2常亮
                    else:
                        set_led(1, 1, 0, 1)  # 客户端模式: LED1常亮
                        set_led(2, 0.05, 0.05, 0)  # 客户端模式: LED2闪烁
                ros_was_enabled = True  # 更新为ROS控制启用状态
        else:
            if ros_was_enabled:
                logger.info("ROS控制丢失，保持当前LED状态...")
                ros_was_enabled = False  # 更新状态标志

        time.sleep(5)  # 每5秒检查一次

if __name__ == "__main__":
    # 配置参数
    ap_prefix = 'HW-'
    sn = get_cpu_serial_number()   # 获取CPU序列号
    WIFI_MODE = 2  # 1表示AP模式，2表示客户端模式，3表示AP模式并共享eth0的互联网
    WIFI_AP_SSID = ''.join([ap_prefix, sn[0:8]])
    WIFI_STA_SSID = "ssid"  # 请替换为目标WiFi名称
    WIFI_AP_PASSWORD = "hiwonder"  # 请替换为AP模式的密码
    WIFI_STA_PASSWORD = "12345678"  # 请替换为客户端模式的密码
    WIFI_AP_GATEWAY = "192.168.149.1"
    WIFI_CHANNEL = 36
    WIFI_FREQ_BAND = 'a'  # 'a'表示5G, 'g'表示2.4G
    WIFI_TIMEOUT = 30  # 客户端模式下的连接超时时间（秒）
    WIFI_LED = True
    ip = WIFI_AP_GATEWAY

    # 读取配置文件
    if os.path.exists(config_file_name):
        update_globals(os.path.splitext(config_file_name)[0])
    if os.path.exists(internal_config_file_path):
        sys.path.insert(0, internal_config_file_dir_path)
        update_globals(os.path.splitext(config_file_name)[0])
    if os.path.exists(external_config_file_path):
        sys.path.insert(1, external_config_file_dir_path)
        update_globals(os.path.splitext(config_file_name)[0])

    # 启动WiFi管理线程
    if WIFI_LED:
        wifi_thread = threading.Thread(target=WIFI_MGR)
        wifi_thread.start()

    # 启动ROS节点监控线程
    node_monitor_thread = threading.Thread(target=monitor_ros_node)
    node_monitor_thread.start()

    # 等待线程结束
    wifi_thread.join()
    node_monitor_thread.join()
