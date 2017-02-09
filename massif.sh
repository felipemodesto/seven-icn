#!/bin/bash
#
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#Memory Allocation mapping
valgrind --tool=massif --massif-out-file="../../massif.log.out" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0 -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini