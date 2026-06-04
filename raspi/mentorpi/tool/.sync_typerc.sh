#!/bin/bash
xhost +
docker exec -u ubuntu -w /home/ubuntu MentorPi /bin/zsh -c "source ~/.zshrc; cd ~/ros2_ws; ~/.sync_typerc.sh"
