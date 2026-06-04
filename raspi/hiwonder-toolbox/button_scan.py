#!/usr/bin/env python3
import os
import time
import subprocess
import threading
import ros_robot_controller_sdk as rrc

BUTTON_ID_1 = 1
BUTTON_ID_2 = 2

def reset_wifi():
    os.system("sudo rm /etc/wifi/* -rf > /dev/null 2>&1")
    os.system("sudo systemctl restart wifi.service > /dev/null 2>&1")

def handle_button_press(board, button_id, event_type):
    if button_id == BUTTON_ID_1:
        if event_type == 'short_press':
            print('按钮 ID1 短按')
        elif event_type == 'long_press':
            print('按钮 ID1 长按')
            reset_wifi()
    elif button_id == BUTTON_ID_2:
        if event_type == 'short_press':
            print('按钮 ID2 短按')
        elif event_type == 'long_press':
            print('按钮 ID2 长按')
            os.system('sudo halt')

def listen_to_button_events():
    command = 'source /home/ubuntu/.zshrc && ros2 topic echo /ros_robot_controller/button'
    process = subprocess.Popen(
        ['docker', 'exec', '-u', 'ubuntu', '-w', '/home/ubuntu', 'MentorPi', '/bin/zsh', '-c', command],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    while True:
        output = process.stdout.readline()
        if output:
            line = output.strip()
            if line.startswith("id:"):
                button_id = int(line.split(":")[1].strip())
            elif line.startswith("state:"):
                state = int(line.split(":")[1].strip())

                if button_id == 1:
                    if state == 1:
                        print("检测到 ID 1 短按")
                    elif state == 2:
                        print('检测到 ID 1 长按：重置 WiFi')
                        reset_wifi()
                elif button_id == 2:
                    if state == 1:
                        print("检测到 ID 2 短按")
                    elif state == 2:
                        print('检测到 ID 2 长按：关闭系统')
                        os.system('sudo halt')
        else:
            # 没有更多数据，等待下一个事件
            continue

def check_node_status():
    command = 'source /home/ubuntu/.zshrc && ros2 topic list'
    result = subprocess.run(['docker', 'exec', '-u', 'ubuntu', '-w', '/home/ubuntu', 'MentorPi', '/bin/zsh', '-c', command], capture_output=True, text=True)
    res = result.stdout
    return '/ros_robot_controller/button' in res

if __name__ == "__main__":
    if check_node_status():
        print("检测到 ROS 2 节点：1")
        listener_thread = threading.Thread(target=listen_to_button_events, daemon=True)
        listener_thread.start()
        while listener_thread.is_alive():
            listener_thread.join(1)
    else:
        print("使用 rrc.Board：2")
        board = rrc.Board()
        board.enable_reception()
        while True:
            button_event = board.get_button()
            if button_event:
                button_id, event_type = button_event
                handle_button_press(board, button_id, event_type)
            time.sleep(0.05)
