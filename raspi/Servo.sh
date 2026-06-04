#!/bin/bash
xhost + 
docker exec -u ubuntu -w /home/ubuntu MentorPi /bin/zsh -c "source /home/ubuntu/ros2_ws/.zshrc; python3 /home/ubuntu/software/Servo_upper_computer/main.py"
