#!/bin/bash
####################################################
####
#### 	===== SUMO SCENARIO BUILDER =====
####
#### REQUIRED TOOLS:
####
#### * JOSM (Either get .jar or from software library)
####	* Mostly to cleanup the generated .osm file prior to compilation
####
#### * Sumo (v0.21.0 recommended for Omnetpp compatibility)
#### 	* sumo-gui (make sure your build of sumo has this! this means solving its dependencies)
#### 	* netconvert (sumo tool)
####	* polyconvert (sumo tool)
#### 	* osmfilter (sumo tool) --> Not really working / used right now
####	* randomTrips.py (sumo python script. Location varies from 0.21 to 0.24. beware)
####
#### * Python
####
#### * OpenStreetMap
####	* Get an OSM file using their export tool.
####	   \--> If you get a web error is because your area is too big.
####	   \--> If you need something big, merg files with JOSM
####
#### Useful Links:
####
#### http://sumo.dlr.de/wiki/Tutorials/ScenarioGuide
#### http://wiki.openstreetmap.org/wiki/JOSM/Advanced_editing
#### http://sumo.dlr.de/wiki/Tutorials/Import_from_OpenStreetMap
#### http://sumo.dlr.de/wiki/Networks/Import/OpenStreetMap
#### http://wiki.openstreetmap.org/wiki/JOSM/Advanced_editing
#### http://sumo.dlr.de/wiki/POLYCONVERT
#### http://sumo.dlr.de/wiki/SUMO_edge_type_file
#### https://www.youtube.com/watch?v=_2KIKjGvBwU
####
####
####################################################
#### Definitions

PYTHON_PATH="null"
SUMO_GUI="sumo-gui"
POLYCONVERT="polyconvert"
NETCONVERT="netconvert"

OSM_CLEAN_FILE="map_clean.osm"
#OSM_FILE="map.osm"
OSM_FILE="ottawa.osm"

#CUBE SIMULATION CENARIO
#declare -i NUMBER_OF_TRIPS
#declare -i SIMULATION_LENGTH
#declare -i VEHICLE_SPAWN_START_TIME
#declare -i VEHICLE_SPAWN_END_TIME


FRINGE_FACTOR=1			#PROPORTION OF TRIPS THAT START IN AN EDGE/LEAF

NUMBER_OF_TRIPS=200		#MAXIMUM NUMBER OF VEHICLES AT THE SAME TIME
SIMULATION_LENGTH=1000	#SIMULATION LENGTH
MIN_TRIP_LENGTH=0	 	#MINIMUM STRAIGHT LINE DISTANCE
MIN_EDGES=50			#MINIMUM NUMBER OF EDGES VEHICLE HAS TO TRAVEL
TIME_STEP=0.1			#SIMULATION TIMESTEP

INTER_VEHICLE_SPAWN_PERIOD=0.1

NET_FILE="map.roads.xml"
ROUTE_FILE="map.routes.xml"
OVERLAY_FILE="map.poly.xml"

SUMO_CONFIG_FILE="map.sumo.cfg"
OMNET_CONFIG_FILE="map.launchd.xml"

TRIP_CONFIG_FILE="randomTrips.xml"

RANDOM_TRIP_FILE="trip/randomTrips.py"
ROUTE_TRIP_FILE="trip/route2trips.py"

VEHICLE_SPAWN_START_TIME=0
#VEHICLE_SPAWN_END_TIME=$(( VEHICLE_SPAWN_START_TIME + NUMBER_OF_TRIPS * INTER_VEHICLE_SPAWN_PERIOD ))
#VEHICLE_SPAWN_END_TIME=$(awk "BEGIN {printf \"%.2f\", ${VEHICLE_SPAWN_START_TIME}+${NUMBER_OF_TRIPS}*${INTER_VEHICLE_SPAWN_PERIOD} }")
VEHICLE_SPAWN_END_TIME=$(awk "BEGIN {printf \"%.2f\",${VEHICLE_SPAWN_START_TIME}+${NUMBER_OF_TRIPS}*${INTER_VEHICLE_SPAWN_PERIOD}}")

#INTER_VEHICLE_SPAWN_PERIOD=$(($SIMULATION_LENGTH/$NUMBER_OF_TRIPS))
#INTER_VEHICLE_SPAWN_PERIOD=$(awk "BEGIN {printf \"%.2f\",(${SIMULATION_LENGTH}-${STARTUP_TIME})/${NUMBER_OF_TRIPS}}")

EXCLUDED_VEHICLE_CLASSES="truck,tram,rail_urban,rail,rail_electric,hov,taxi,bus,delivery,motorcycle,bicycle,pedestrian"

TRIP_FILE="$RANDOM_TRIP_FILE"

####################################################
#### Actual Script stuff


#### CONFIGURATIONS FOR MACHINE-SPECIFIC USAGE WITH GENERIC FALLBACK (Necessary if machine has SUMO 0.24.0 set on path)
if [ "Brick" = "$(hostname)" ]; then
	echo "Environment set to [ Brick Ubuntu ]"
	PYTHON_PATH="/home/felipe/Simulation/sumo-0.25.0/tools/"
	RANDOM_TRIP_FILE="randomTrips.py"
	ROUTE_TRIP_FILE="route2trips.py"
	SUMO_GUI="sumo-guiD"
	POLYCONVERT="polyconvertD"
	NETCONVERT="netconvertD"
elif [ "OptiPlex" = "$(hostname)" ]; then
	echo "Environment set to [    Ubuntu    ]"
	PYTHON_PATH="/home/felipe/Simulation/sumo-0.25.0/tools/"
	RANDOM_TRIP_FILE="randomTrips.py"
	ROUTE_TRIP_FILE="route2trips.py"
	SUMO_GUI="sumo-guiD"
	POLYCONVERT="polyconvertD"
	NETCONVERT="netconvertD"
elif [ "MacBook-Air.local" = "$(hostname)" ]; then
	echo "Environment set to [   Mac OS X   ]"
	PYTHON_PATH="/Applications/sumo-0.25.0/tools/"
	RANDOM_TRIP_FILE="randomTrips.py"
	ROUTE_TRIP_FILE="route2trips.py"
	SUMO_GUI="sumo-guiD"
	POLYCONVERT="polyconvertD"
	NETCONVERT="netconvertD"
else
	echo "Envorinment set to [   Unknown    ]"
	echo "	\\--> Current Hostname is: <$(hostname)>"
	#"${SUMO_HOMEX:?WARNING, SUMO_HOME is not set, abording simulation.}"
	if [ -z "$SUMO_HOME" ]; then
		echo "SUMO_HOME is not set, script cannot proceed until you get your shit together."
		exit 1
	else
		PYTHON_PATH="$SUMO_HOME/tools/"
	fi
fi

echo "	\\--> Inter-Vehicle Spawn Time is set to: $INTER_VEHICLE_SPAWN_PERIOD"
echo "	\\--> Vehicle Instantiation period is [$VEHICLE_SPAWN_START_TIME ; $VEHICLE_SPAWN_END_TIME]"

#exit

# Not actually cleaning the document for now, so let's just copy it to the clean file position, keeping the original safe
cp $OSM_FILE $OSM_CLEAN_FILE

#### Cleaning File
#### REMOVING ALL NAME VARIATIONS! name:xx (NON ASCII MESSED UP TEXT)
printf "Removing utf8-extension characters from name fields..."
while read -r line; do
	#[[ ! $s =~ "*name:*" ]] && echo "$line"
	[[ ! $line = *"name:"* ]] && echo "$line"
	#echo "$line"
done < $OSM_FILE > $OSM_CLEAN_FILE
echo " \\--> done."

#### Building the restriction document for the random trip python script
#### (This is not really being used, but in theory is passed as an argument to the RandomTrips script)
printf "Writing Restriction Class Document..."
#echo "<?xml version=\"1.0\"?>" > $TRIP_CONFIG_FILE
echo "<additional>" > $TRIP_CONFIG_FILE
echo "	<vType id=\"myType\" maxSpeed=\"27\" vClass=\"passenger\"/>" >> $TRIP_CONFIG_FILE
echo "</additional>" >> $TRIP_CONFIG_FILE
echo " \\--> done."


#### Convert OSM to SUMO Map:
printf "Creating Road Network XML File From OSM..."
$NETCONVERT --osm-files $OSM_CLEAN_FILE --tls.guess-signals --remove-edges.by-vclass $EXCLUDED_VEHICLE_CLASSES  --geometry.remove --tls.join --remove-edges.isolated --no-turnarounds -o $NET_FILE
echo " \\--> done."


#### ADD Polygon Features to Map (copy typemap.xml file from somewhere, possibly Wiki if you don't have one)
printf "Creating Feature Map XML File From OSM..."
$POLYCONVERT --net-file $NET_FILE --osm-files $OSM_CLEAN_FILE --ignore-errors true --type-file typemap.xml -o $OVERLAY_FILE
echo " \\--> done."


#### Running SUMO's python script that generates random paths 
#### (-r ==  Random Trips, -s == Random Seed, -e == EndTime, -l == Weight Edge Probability)
printf "Generating Random Route System for Road Network XML File..."
python $PYTHON_PATH$RANDOM_TRIP_FILE -n $NET_FILE  --r $ROUTE_FILE --begin $VEHICLE_SPAWN_START_TIME --end $VEHICLE_SPAWN_END_TIME --period $INTER_VEHICLE_SPAWN_PERIOD --min-distance $MIN_TRIP_LENGTH --fringe-factor $FRINGE_FACTOR --intermediate $MIN_EDGES -l
echo " \\--> done."


#### Displaying Trips in Route FIle
printf "Printing Routes to Trips..."
python $PYTHON_PATH$ROUTE_TRIP_FILE $ROUTE_FILE
echo " \\--> done."


echo "Opening Simulation Scenario in SUMO GUI..."
#### Run SUMO-GUI
#$SUMO_GUI -c $SUMO_CONFIG_FILE -Q false -S true &
echo " \\--> done."