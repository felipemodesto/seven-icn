#!/bin/bash
#
make MODE=debug
#
cd /home/felipe/Simulation/BaconNet/scenario/unified
#
#Memory Allocation mapping
valgrind --tool=massif --massif-out-file="../../massif.log.out" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0 -c Ottawa_50 -u Cmdenv -n ..:../../src/paradise:../../src/veins --image-path=../../images -l ../../src/veins --record-eventlog=false --debug-on-errors=false -f baconnet.ini