#!/bin/bash

echo " --> System Name is $HOSTNAME"
if [ "Brick" = "${HOSTNAME}" ]; then
	echo "Environment set to [    Brick     ]"
	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
elif [ "Ubuntu" = "${hostname}" ]; then
	echo "Environment set to [    Ubuntu    ]"
	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
elif [ "OptiPlex" = "${HOSTNAME}" ]; then
	echo "Environment set to [   OptiPlex   ]"
	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
elif [ "Optiplex-7010" = "${HOSTNAME}" ]; then
	echo "Environment set to [OptiPlex-7010 ]"
	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumo
elif [ "OptiPlex-7010-hamid" = "${HOSTNAME}" ]; then
	echo "Environment set to [OptiPlex-hamid]"
	/home/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
elif [ "MacBook-Air.local" = "${HOSTNAME}" ]; then
	echo "Environment set to [   Mac OS X   ]"
	/Users/felipe/Simulation/BaconNet/sumo-launchd.py -vv -c sumoD
else
	echo "Envorinment set to [   Unknown    ] --> (${HOSTNAME})"
	~/Simulation/BaconNet/sumo-launchd.py -vv -c sumo &
fi
