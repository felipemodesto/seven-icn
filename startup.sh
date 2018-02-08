#!/bin/bash

echo " --> System Name is $HOSTNAME"
#if [ "Brick" = "${HOSTNAME}" ]; then
#	echo "Environment set to [    Brick     ]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
#elif [ "Ubuntu" = "${hostname}" ]; then
#	echo "Environment set to [    Ubuntu    ]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
#elif [ "OptiPlex-7020" = "${HOSTNAME}" ]; then
#	echo "Environment set to [   OptiPlex   ]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
#elif [ "Optiplex-7010" = "${HOSTNAME}" ]; then
#	echo "Environment set to [OptiPlex-7010 ]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumo
#elif [ "Optiplex-70H0" = "${HOSTNAME}" ]; then
#	echo "Environment set to [OptiPlex-70H0]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumo
#elif [ "Optiplex-70X0" = "${HOSTNAME}" ]; then
#	echo "Environment set to [OptiPlex-70X0]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumo
#elif [ "Optiplex-70K0" = "${HOSTNAME}" ]; then
#	echo "Environment set to [OptiPlex-70K0]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
#elif [ "bulbasaur" = "${HOSTNAME}" ]; then
#	echo "Environment set to [ Ubuntu 17.10 ]"
#	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
#else
echo "Environment set on Machine [  ${HOSTNAME}  ]"
~/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD &
#fi
