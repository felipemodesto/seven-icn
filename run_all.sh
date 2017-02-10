#!/bin/bash
#
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#
#OPP RUN ALL = LINE EDGE
#opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r * -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini
#OPP RUN ALL = OTTAWA 450
opp_runall -j8 /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0..39 -u Cmdenv -c Ottawa_450 -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini 
