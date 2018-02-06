#!/bin/bash
#
#

#Making sure everything is clean
./configure
make clean
make MODE=release -j4


cd /home/felipe/Simulation/BaconNet/scenario/unified

# Scenario Selection
if [[ -n "${1}" ]]; then
  SCENARIO="${1}"
else
  SCENARIO="Ottawa_150"
fi
#
# Run Size Selection
if [[ -n "${2}" ]]; then
  RUN_GROUP="${2}"
else
  RUN_GROUP="0..29"
fi
#
# Run CPU Count
if [[ -n "${3}" ]]; then
  THREAD_COUNT="${3}"
else
  THREAD_COUNT="-j"$(nproc)
  echo "Note: opp_runall will run with process count set to: ${THREAD_COUNT}"
fi

RUN_CODE="/home/felipe/Simulation/omnetpp-5.0/bin/opp_run"

if [ "OptiPlex-70X0" = "${HOSTNAME}" ]; then
	RUN_CODE="/home/shared/omnet/bin/opp_run"
fi

if [ "OptiPlex-70XX" = "${HOSTNAME}" ]; then
	RUN_CODE="/home/shared/omnet/bin/opp_run"
fi

if [ "OptiPlex-70K0" = "${HOSTNAME}" ]; then
	RUN_CODE="-b1 ~/Simulation/omnetpp-5.2.1/bin/opp_run"
fi

if [ "ubuntu" = "${HOSTNAME}" ]; then
	RUN_CODE="-b1 /home/felipe/Simulation/omnetpp-5.2/bin/opp_run"
fi

if [ "bulbasaur" = "${HOSTNAME}" ]; then
	RUN_CODE="-b1 /home/felipe/Simulation/omnetpp-5.2/bin/opp_run"
fi

LINK_CODE="-u Cmdenv -n ..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini"
RUN_COMMAND="${THREAD_COUNT} ${RUN_CODE} -r ${RUN_GROUP} -c ${SCENARIO} ${LINK_CODE}"

opp_runall $RUN_COMMAND
