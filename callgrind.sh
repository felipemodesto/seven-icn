#!/bin/bash
#
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#Callgrind - Call-graph tool & Cache + Branch Prediction Profiler
valgrind --tool=callgrind --log-file="../../callgrind.interactive.out"  /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 2 -c Ottawa_50 -u Cmdenv -n ..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini