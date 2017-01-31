#!/usr/bin/env bash
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
RANDOM_TRIP_FILE=$(which randomTrips.py)
ROUTE_TRIP_FILE=$(which route2trips.py)

# Defining the input map file.
if [[ -n "${1}" ]]; then
  OSM_FILE="${1}"
else
  OSM_FILE="map.osm"
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
  NUMBER_OF_TRIPS=20
fi

# Based on the input map file.
SCENARIO_NAME=$(basename ${OSM_FILE} .osm | tr ' ' _)".${NUMBER_OF_TRIPS}"

# Where we are going to generate stuff.
OUTPUT_DIR=./topology
#OUTPUT_DIR=.
mkdir -p "${OUTPUT_DIR}"

#CUBE SIMULATION CENARIO
#declare -i NUMBER_OF_TRIPS
#declare -i SIMULATION_LENGTH
#declare -i VEHICLE_SPAWN_START_TIME
#declare -i VEHICLE_SPAWN_END_TIME


FRINGE_FACTOR=0         #PROPORTION OF TRIPS THAT START IN AN EDGE/LEAF

SIMULATION_LENGTH=1100  #SIMULATION LENGTH
MIN_TRIP_LENGTH=0     	#MINIMUM STRAIGHT LINE DISTANCE
MIN_EDGES=2      		    #MINIMUM NUMBER OF EDGES VEHICLE HAS TO TRAVEL
TIME_STEP=0.1      		  #SIMULATION TIMESTEP
INTER_VEHICLE_SPAWN_PERIOD=0.1

OSM_CLEAN_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".clean.osm"
NET_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".roads.xml"
ROUTE_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".routes.xml"
OVERLAY_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".poly.xml"

SUMO_LAUNCH_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".launchd.xml"
SUMO_CONFIG_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".sumo.cfg"
OMNET_CONFIG_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".launchd.xml"

TRIP_CONFIG_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".randomTrips.xml"
OUTPUT_TRIP_FILE=${OUTPUT_DIR}/${SCENARIO_NAME}".trips.xml"

VEHICLE_SPAWN_START_TIME=0
VEHICLE_SPAWN_END_TIME=$(awk "BEGIN {printf \"%.2f\",${VEHICLE_SPAWN_START_TIME}+${NUMBER_OF_TRIPS}*${INTER_VEHICLE_SPAWN_PERIOD}}")

EXCLUDED_VEHICLE_CLASSES="truck,tram,rail_urban,rail,rail_electric,hov,taxi,bus,delivery,motorcycle,bicycle,pedestrian"

TRIP_FILE="$RANDOM_TRIP_FILE"

####################################################
#### Actual Script stuff

echo "Current Folder : $PWD"

#### CONFIGURATIONS FOR MACHINE-SPECIFIC USAGE WITH GENERIC FALLBACK (Necessary if machine has SUMO 0.24.0 set on path)
if [ -e /etc/arch-release ]; then
    # ArchLinux. Sane system, so assume everything is in PATH already.
    echo "Environment set to [    ArchLinux    ]"
elif [ "fedora" = "$(hostname)" ]; then
  echo "Environment set to [    Fedora    ]"
  PYTHON_PATH="/home/felipe/Simulation/sumo-0.25.0/tools/"
elif [ "OptiPlex" = "$(hostname)" ]; then
  echo "Environment set to [    Ubuntu    ]"
  PYTHON_PATH="/home/felipe/Simulation/sumo-0.25.0/tools"
  RANDOM_TRIP_FILE=${PYTHON_PATH}"/randomTrips.py"
  ROUTE_TRIP_FILE=${PYTHON_PATH}"/route2trips.py"
  SUMO_GUI="sumo-guiD"
  POLYCONVERT="polyconvertD"
  NETCONVERT="netconvertD"
elif [ "MacBook-Air.local" = "$(hostname)" ]; then
  echo "Environment set to [   Mac OS X   ]"
  PYTHON_PATH="/Applications/sumo-0.25.0/tools"
  RANDOM_TRIP_FILE=${PYTHON_PATH}"/randomTrips.py"
  ROUTE_TRIP_FILE=${PYTHON_PATH}"/route2trips.py"
  SUMO_GUI="sumo-guiD"
  POLYCONVERT="polyconvertD"
  NETCONVERT="netconvertD"
else
  echo "Envorinment set to [   Unknown    ]"
  echo "  \\--> Current Hostname is: <$(hostname)>"
  #"${SUMO_HOMEX:?WARNING, SUMO_HOME is not set, abording simulation.}"
  if [ -z "$SUMO_HOME" ]; then
    echo "SUMO_HOME is not set, script cannot proceed until you get your shit together."
    exit 1
  else
    PYTHON_PATH="$SUMO_HOME/tools"
    RANDOM_TRIP_FILE=${PYTHON_PATH}"/trip/randomTrips.py"
    ROUTE_TRIP_FILE=${PYTHON_PATH}"/trip/route2trips.py"
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

echo "  \\--> Inter-Vehicle Spawn Time is set to: $INTER_VEHICLE_SPAWN_PERIOD"
echo "  \\--> Vehicle Instantiation period is [$VEHICLE_SPAWN_START_TIME ; $VEHICLE_SPAWN_END_TIME]"


# Not actually cleaning the document for now, so let's just copy it to the clean file position, keeping the original safe
cp ${OSM_FILE} ${OSM_CLEAN_FILE}

#### Cleaning File
#### REMOVING ALL NAME VARIATIONS! name:xx (NON ASCII MESSED UP TEXT)
printf "Removing utf8-extension characters from name fields..."
while read -r line; do
  [[ ! ${line} = *"name:"* ]] && echo "${line}"
done < ${OSM_FILE} > ${OSM_CLEAN_FILE}
echo " \\--> done."

#### Building the restriction document for the random trip python script
#### (This is not really being used, but in theory is passed as an argument to the RandomTrips script)
printf "Writing Restriction Class Document..."
#echo "<?xml version=\"1.0\"?>" > $TRIP_CONFIG_FILE
echo "<additional>" > ${TRIP_CONFIG_FILE}
echo "  <vType id=\"myType\" maxSpeed=\"27\" vClass=\"passenger\"/>" >> ${TRIP_CONFIG_FILE}
echo "</additional>" >> ${TRIP_CONFIG_FILE}
echo " \\--> done."


#### Convert OSM to SUMO Map:
printf "Creating Road Network XML File From OSM..."
${NETCONVERT} --osm-files ${OSM_CLEAN_FILE} --tls.guess-signals --remove-edges.by-vclass ${EXCLUDED_VEHICLE_CLASSES} --geometry.remove --tls.join --remove-edges.isolated --no-turnarounds -o ${NET_FILE}
echo " \\--> done."


#### ADD Polygon Features to Map (copy typemap.xml file from somewhere, possibly Wiki if you don't have one)
printf "Creating Feature Map XML File From OSM..."
#${POLYCONVERT} --net-file ${NET_FILE} --osm-files ${OSM_CLEAN_FILE} --ignore-errors true --type-file osmNetconvertUrbanDe.typ.xml -o ${OVERLAY_FILE} --save-configuration ${SUMO_CONFIG_FILE}
${POLYCONVERT} --net-file ${NET_FILE} --osm-files ${OSM_CLEAN_FILE} --ignore-errors true --type-file typemap.xml -o ${OVERLAY_FILE}
echo " \\--> done."


#### Running SUMO's python script that generates random paths
#### (-r ==  Random Trips, -s == Random Seed, -e == EndTime, -l == Weight Edge Probability)
printf "Generating Random Route System for Road Network XML File..."
python2 ${RANDOM_TRIP_FILE} -n ${NET_FILE}  --r ${ROUTE_FILE} -a ${TRIP_CONFIG_FILE} --begin ${VEHICLE_SPAWN_START_TIME} --end ${VEHICLE_SPAWN_END_TIME} --period ${INTER_VEHICLE_SPAWN_PERIOD} --min-distance ${MIN_TRIP_LENGTH} --fringe-factor ${FRINGE_FACTOR} --intermediate ${MIN_EDGES} -l -o ${OUTPUT_TRIP_FILE}
echo " \\--> done."

#### Generating config file.
cat > ${SUMO_CONFIG_FILE} << EOF
<?xml version="1.0" encoding="iso-8859-1"?>

<configuration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://sumo.sf.net/xsd/sumoConfiguration.xsd">

  <input>
    <net-file value="${SCENARIO_NAME}.roads.xml"/>
    <route-files value="${SCENARIO_NAME}.routes.xml"/>
    <additional-files value="${SCENARIO_NAME}.poly.xml"/>
  </input>

  <time>
    <begin value="0"/>
    <end value="${SIMULATION_LENGTH}"/>
    <step-length value="0.1"/>
  </time>

  <gui_only>
  	<start value="true"/>
  </gui_only>

</configuration>
EOF


#### Generating launch file.
cat > ${SUMO_LAUNCH_FILE} << EOF
<?xml version="1.0"?>
<launch>
	<copy file="${SCENARIO_NAME}.roads.xml"/>
	<copy file="${SCENARIO_NAME}.routes.xml"/>
	<copy file="${SCENARIO_NAME}.poly.xml"/>
	<copy file="${SCENARIO_NAME}.sumo.cfg" type="config"/>
</launch>
EOF


#### Displaying Trips in Route FIle
printf "Printing Routes to Trips..."
python2 ${ROUTE_TRIP_FILE} ${ROUTE_FILE}
echo " \\--> done."


echo "Opening Simulation Scenario in SUMO GUI..."
#### Run SUMO-GUI
#${SUMO_GUI} -c ${SUMO_CONFIG_FILE} -Q false -S true &
echo " \\--> done."

# vim:set ts=2 sw=2 et:
