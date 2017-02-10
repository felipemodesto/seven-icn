#!/bin/bash
#
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#Memcheck - Memory Leak & Profile
valgrind --tool=memcheck --leak-check=yes --leak-check=full --leak-resolution=high --log-file="../../memcheck.log.out" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 10 -u Cmdenv -c Line_Edge -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false baconnet.ini