#!/bin/bash
#set -x << for debuging.

####################################################
####
####   ===== SUMO SCENARIO BUILDER =====
####
#### REQUIRED TOOLS:
####
#### * JOSM (Either get .jar or from software library)
####  * Mostly to cleanup the generated .osm file prior to compilation
####
#### * Sumo (v0.21.0 recommended for Omnetpp compatibility)
####   * sumo-gui (make sure your build of sumo has this! this means solving its dependencies)
####   * netconvert (sumo tool)
####  * polyconvert (sumo tool)
####   * osmfilter (sumo tool) --> Not really working / used right now
####  * randomTrips.py (sumo python script. Location varies from 0.21 to 0.24. beware)
####
#### * Python2
####
#### * OpenStreetMap
####  * Get an OSM file using their export tool.
####     \--> If you get a web error is because your area is too big.
####     \--> If you need something big, merg files with JOSM
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

PROG_NAME="${0}"

# Default programs (assumed to be in PATH).
SUMO_GUI=$(which sumo-gui)
POLYCONVERT=$(which polyconvert)
NETCONVERT=$(which netconvert)
DUAROUTER=$(which duarouter)
RANDOM_TRIP_FILE=$(which randomTrips.py)
ROUTE_TRIP_FILE=$(which route2trips.py)

# Defining the input map file.
if [[ -n "${1}" ]]; then
  OSM_FILE="${1}"
else
  OSM_FILE="ottawa.osm"
fi

if [[ ! -e "${OSM_FILE}" ]]; then
  echo "File '${OSM_FILE}' not found. Make sure it exists."
  echo "Usage: ${PROG_NAME} [OSM MAP FILE] [NUMBER OF VEHICLES]"
  echo "Example: ${PROG_NAME} ottawa.osm 30"
  echo "Defaults:"
  echo "  'map.osm' is used as default OSM MAP FILE if none is passed as argument to this script"
  echo "  '20' is used as the default number of vehicles."
  exit 1
fi

if [[ -n "${2}" ]]; then
  NUMBER_OF_TRIPS="${2}"
else
  NUMBER_OF_TRIPS=50
fi

# Based on the input map file.
CLEAN_NAME=$(basename ${OSM_FILE} .osm | tr ' ' _)
SCENARIO_NAME=$(basename ${OSM_FILE} .osm | tr ' ' _)".${NUMBER_OF_TRIPS}"


#if [[ -n "${3}" ]]; then
#  OUTPUT_DIR="${3}"
#else
#  OUTPUT_DIR=./output
#fi
#
#mkdir -p "${OUTPUT_DIR}"



FRINGE_FACTOR=1         #PROPORTION OF TRIPS THAT START IN AN EDGE/LEAF
SIMULATION_LENGTH=5000 	#SIMULATION LENGTH
MIN_TRIP_LENGTH=500	    #MINIMUM STRAIGHT LINE DISTANCE IN METERS BETWEEN START AND END
MIN_EDGES=50      	    #MINIMUM NUMBER OF EDGES VEHICLE HAS TO TRAVEL
TIME_STEP=0.1           #SIMULATION TIMESTEP
INTER_VEHICLE_SPAWN_PERIOD=0.1
VEHICLE_SPAWN_START_TIME=0

ROUTE_FILE=${SCENARIO_NAME}".routes.xml"

#These files are not really dependent on the scenario configuration and can be shared
OSM_CLEAN_FILE=${CLEAN_NAME}".clean.osm"
OVERLAY_FILE=${CLEAN_NAME}".poly.xml"
REROUTE_FILE=${CLEAN_NAME}".reroute.xml"

NET_FILE=${CLEAN_NAME}".net.xml"
WEIGHT_FILE=${CLEAN_NAME}".weight.xml"

SUMO_CONFIG_FILE=${SCENARIO_NAME}".sumo.cfg"
OMNET_CONFIG_FILE=${SCENARIO_NAME}".omnet.xml"

TRIP_CONFIG_FILE=${SCENARIO_NAME}".randomTrips.xml"
OUTPUT_TRIP_FILE=${SCENARIO_NAME}".trips.xml"


VEHICLE_SPAWN_END_TIME=$(awk "BEGIN {printf \"%.2f\",${VEHICLE_SPAWN_START_TIME}+${NUMBER_OF_TRIPS}*${INTER_VEHICLE_SPAWN_PERIOD}}")

EXCLUDED_VEHICLE_CLASSES="truck,tram,rail_urban,rail,rail_electric,hov,taxi,bus,delivery,motorcycle,bicycle,pedestrian"

TRIP_FILE="$RANDOM_TRIP_FILE"

####################################################
#### Actual Script stuff


#### CONFIGURATIONS FOR MACHINE-SPECIFIC USAGE WITH GENERIC FALLBACK (Necessary if machine has SUMO 0.24.0 set on path)
if [ -e /etc/arch-release ]; then
    # ArchLinux. Sane system, so assume everything is in PATH already.
    echo "Environment set to [    ArchLinux    ]"
elif [ "Brick" = "$(hostname)" ] ||[ "OptiPlex" = "$(hostname)" ] || [ "OptiPlex-7010" = "$(hostname)" ] || [ "OptiPlex-7010-hamid" = "$(hostname)" ]; then
  echo "Environment set to [    Ubuntu    ]"
  PYTHON_PATH="/home/felipe/Simulation/sumo-0.25.0/tools"
  RANDOM_TRIP_FILE=${PYTHON_PATH}"/randomTrips.py"
  ROUTE_TRIP_FILE=${PYTHON_PATH}"/route2trips.py"
  SUMO_GUI="sumo-guiD"
  POLYCONVERT="polyconvertD"
  NETCONVERT="netconvertD"
  DUAROUTER="duarouterD"
elif [ "MacBook-Air.local" = "$(hostname)" ]; then
  echo "Environment set to [   Mac OS X   ]"
  PYTHON_PATH="/Applications/sumo-0.25.0/tools"
  RANDOM_TRIP_FILE=${PYTHON_PATH}"/randomTrips.py"
  ROUTE_TRIP_FILE=${PYTHON_PATH}"/route2trips.py"
  SUMO_GUI="sumo-guiD"
  POLYCONVERT="polyconvertD"
  NETCONVERT="netconvertD"
  DUAROUTER="duarouterD"
else
  echo "Envorinment set to [   Unknown    ]"
  echo "  \\--> Current Hostname is: <$(hostname)>"
  #"${SUMO_HOMEX:?WARNING, SUMO_HOME is not set, abording simulation.}"
  if [ -z "$SUMO_HOME" ]; then
    echo "SUMO_HOME is not set, script cannot proceed without minimal setup."
    exit 1
  else
    PYTHON_PATH="$SUMO_HOME/tools"
    RANDOM_TRIP_FILE=${PYTHON_PATH}"/randomTrips.py"
    ROUTE_TRIP_FILE=${PYTHON_PATH}"/route2trips.py"
    SUMO_GUI="sumo-gui"
    POLYCONVERT="polyconvert"
    NETCONVERT="netconvert"
    DUAROUTER="duarouter"
  fi
fi

# Sanity check -- making sure the PATHs are all set.
function cmd_exists() {
  hash ${1} 2>/dev/null
  return $?
}

for cmd in python2 ${RANDOM_TRIP_FILE} ${ROUTE_TRIP_FILE} ${SUMO_GUI} ${POLYCONVERT} ${NETCONVERT}; do
  if ! cmd_exists ${cmd}; then
    echo "Command '${cmd} does not exist or is not available in PATH; exiting..."
    exit 1
  fi
done

printf "  \\--> Inter-Vehicle Spawn Time is set to: $INTER_VEHICLE_SPAWN_PERIOD\n"
printf "  \\--> Vehicle Instantiation period is [$VEHICLE_SPAWN_START_TIME ; $VEHICLE_SPAWN_END_TIME]\n"


#Removing previous simulation-related stuff
printf "Removing old files"
rm "${SCENARIO_NAME}.*"

#exit

# Not actually cleaning the document for now, so let's just copy it to the clean file position, keeping the original safe
#cp ${OSM_FILE} ${OSM_CLEAN_FILE}

#### Cleaning File
#### REMOVING ALL NAME VARIATIONS! name:xx (NON ASCII MESSED UP TEXT)
#printf "Removing utf8-extension characters from name fields...\n"
#while read -r line; do
#  [[ ! ${line} = *"name:"* ]] && echo "${line}"
#done < ${OSM_FILE} > ${OSM_CLEAN_FILE}
#printf " \\--> done.\n"


#### Building the restriction document for the random trip python script
#### (This is not really being used, but in theory is passed as an argument to the RandomTrips script)
#printf "Writing Restriction Class Document...\n"
echo "<additional>" > ${TRIP_CONFIG_FILE}
echo "	<vType id=\"fastCar\" accel=\"0.8\" decel=\"4.5\" sigma=\"0.5\" maxSpeed=\"45\" vClass=\"passenger\" probability=\"1.0\" speedFactor=\"1.2\" speedDev=\"0.3\" impatience=\"0.5\"/>" >> ${TRIP_CONFIG_FILE}
echo "</additional>" >> ${TRIP_CONFIG_FILE}
echo " \\--> done.\n"

#### Convert OSM to SUMO Map:
printf "Creating Road Network XML File From OSM...\n"
${NETCONVERT} -c netconvertConfiguration.xsd --osm-files ${OSM_FILE} --tls.guess-signals true --remove-edges.by-vclass ${EXCLUDED_VEHICLE_CLASSES} --geometry.remove true -o ${NET_FILE} --verbose true --tls.join true --tls.guess false --remove-edges.isolated true --no-turnarounds true --no-turnarounds.tls true 
printf " \\--> done.\n"


#### ADD Polygon Features to Map (copy typemap.xml file from somewhere, possibly Wiki if you don't have one)
#printf "Creating Feature Map XML File From OSM...\n"
#${POLYCONVERT} --net-file ${NET_FILE} --osm-files ${OSM_CLEAN_FILE} --ignore-errors true --type-file typemap.xml -o ${OVERLAY_FILE}
#printf " \\--> done.\n"


#### Running SUMO's python script that generates random trips
#### (-r ==  Random Trips, -s == Random Seed, -e == EndTime, -l == Weight Edge Probability)
printf "Generating Random Trips for Road Network XML File...\n"
python2 ${RANDOM_TRIP_FILE} -n ${NET_FILE} --begin ${VEHICLE_SPAWN_START_TIME} --end ${VEHICLE_SPAWN_END_TIME} --period ${INTER_VEHICLE_SPAWN_PERIOD} --min-distance ${MIN_TRIP_LENGTH} --fringe-factor ${FRINGE_FACTOR} --intermediate ${MIN_EDGES} --trip-attributes="departSpeed=\"max\"  departPos=\"random\"  type=\"fastCar\"" -a ${TRIP_CONFIG_FILE} -o ${OUTPUT_TRIP_FILE}  --weights-prefix ${CLEAN_NAME}
printf " \\--> done.\n"


#### Running SUMO's python trip to route script
printf "Generating Vehicle Routes based on Random Trips File...\n"
${DUAROUTER} -v -l "duarouter.log"  -d "${TRIP_CONFIG_FILE}" --keep-all-routes true --skip-new-routes true --repair true --repair.from true --repair.to true -n ${NET_FILE} -t ${OUTPUT_TRIP_FILE} -o ${ROUTE_FILE} --begin ${VEHICLE_SPAWN_START_TIME} --end ${VEHICLE_SPAWN_END_TIME} --weight-attribute "traveltime" --weight-files ${WEIGHT_FILE}
printf " \\--> done.\n"


#### Generating config file.
cat > ${SUMO_CONFIG_FILE} << EOF
<?xml version="1.0"?>
<configuration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://sumo.sf.net/xsd/sumoConfiguration.xsd">
  <input>
    <additional-files value="${REROUTE_FILE}"/>
    <route-files value="${ROUTE_FILE}"/>
    <net-file value="${NET_FILE}"/>
    <weight-files value="${WEIGHT_FILE}"/>
    <weight-attribute value="traveltime"/>
  </input>
  <time>
    <begin value="${VEHICLE_SPAWN_START_TIME}"/>
    <end value="${SIMULATION_LENGTH}"/>
    <step-length value="${TIME_STEP}"/>
  </time>
  <time-to-teleport value="100"/>
  <time-to-teleport.highways value="50"/>
  <gui_only>
    <start value="true"/>
  </gui_only>
</configuration>
EOF


#### Generating omnet config file.
cat > ${OMNET_CONFIG_FILE} << EOF
<?xml version="1.0"?>
<!-- debug config -->
<launch>
  <copy file="${TRIP_CONFIG_FILE}"/>
	<copy file="${NET_FILE}"/>
	<copy file="${ROUTE_FILE}"/>
	<copy file="${OVERLAY_FILE}"/>
  	<copy file="${REROUTE_FILE}"/>
  	<copy file="${REROUTE_FILE}"/>
	<copy file="${WEIGHT_FILE}"/>
	<copy file="${SUMO_CONFIG_FILE}" type="config"/>
</launch>
EOF


#### Generating alternative name for launchd file.
#cp ${OMNET_CONFIG_FILE} ${LAUNCHD_CONFIG_FILE}


#### Displaying Trips in Route FIle
#printf "Printing Routes to Trips..."
#python2 ${ROUTE_TRIP_FILE} ${ROUTE_FILE}
#echo " \\--> done.\n"


#### Removing Redundant clean osm file now that we are done
#rm ${OSM_CLEAN_FILE}
#${PYTHON_PATH}"/showDepartsAndArrivalsPerEdge.py" "${ROUTE_FILE}" --output-file "${SCENARIO_NAME}.results.xml"


#### Run SUMO-GUI
#printf "Opening Simulation Scenario in SUMO GUI... with ${SCENARIO_NAME}"
#${SUMO_GUI} -c ${SUMO_CONFIG_FILE} -Q false --verbose true -S true &
#printf " \\--> done.\n"


# vim:set ts=2 sw=2 et:
