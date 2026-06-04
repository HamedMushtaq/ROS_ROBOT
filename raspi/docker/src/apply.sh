#!/bin/bash
xhost +
docker exec -u ubuntu -w /home/ubuntu armpi_fpv /bin/zsh -c "~/.stop_ros.sh&~/share/src/copy.sh"
