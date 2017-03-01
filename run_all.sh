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
  RUN_GROUP="0..39"
fi
#
# Run CPU Count
if [[ -n "${3}" ]]; then
  THREAD_COUNT="${3}"
else
  THREAD_COUNT="-j8"
fi

RUN_CODE="-j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run"
LINK_CODE="-u Cmdenv -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini"
RUN_COMMAND="${RUN_CODE} -r ${RUN_GROUP} -c ${SCENARIO} ${LINK_CODE}"

#RUN RELEASE = LINE_EDGE
#opp_run_release -r 0 -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini
#OPP RUN ALL = LINE EDGE
#opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r * -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini
#OPP RUN ALL = OTTAWA 450
#opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0..39 -u Cmdenv -c Ottawa_450 -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini

opp_runall $THREAD_COUNT /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r $RUN_GROUP -u Cmdenv -c $SCENARIO -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini
