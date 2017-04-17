#!/bin/bash
#
make MODE=debug
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#Callgrind - Call-graph tool & Cache + Branch Prediction Profiler
valgrind --tool=callgrind --log-file="../../callgrind.interactive.out" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 20 -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini