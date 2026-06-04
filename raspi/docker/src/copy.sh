#!/bin/zsh
sleep 5
cp ~/share/src/config.yaml ~/armpi_fpv/src/armpi_fpv_bringup/config/config.yaml
cp ~/share/src/in.py ~/armpi_fpv/src/warehouse/scripts/in.py
cp ~/share/src/out.py ~/armpi_fpv/src/warehouse/scripts/out.py
cp ~/share/src/exchange.py ~/armpi_fpv/src/warehouse/scripts/exchange.py
cp ~/share/src/pallezting.py ~/armpi_fpv/src/object_pallezting/scripts/pallezting.py
cp ~/share/src/sorting.py ~/armpi_fpv/src/object_sorting/scripts/sorting.py
source /home/ubuntu/armpi_fpv/src/armpi_fpv_bringup/scripts/source_env.bash roslaunch armpi_fpv_bringup bringup.launch
