#!/bin/bash
#
#
make MODE=release

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

if [ "OptiPlex-70XX" = "${HOSTNAME}" ]; then
	RUN_CODE="/home/shared/omnet/bin/opp_run"
fi

if [ "ubuntu" = "${HOSTNAME}" ]; then
	RUN_CODE="/home/felipe/Simulation/omnetpp-5.2/bin/opp_run"
fi

LINK_CODE="-u Cmdenv -n ..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini"
RUN_COMMAND="${THREAD_COUNT} -b1 ${RUN_CODE} -r ${RUN_GROUP} -c ${SCENARIO} ${LINK_CODE}"

#RUN RELEASE = LINE_EDGE
#opp_run_release -r 0 -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini
#OPP RUN ALL = LINE EDGE
#opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r * -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini
#OPP RUN ALL = OTTAWA 450
#opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0..39 -u Cmdenv -c Ottawa_450 -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini

opp_runall $RUN_COMMAND
