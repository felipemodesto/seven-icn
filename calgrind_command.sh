#!/bin/bash
#./configure
#make
#cd scenario
#valgrind --leak-check=yes --tool=memcheck
#opp_run_release -r 0 -u Cmdenv -c nodebug -n ../examples/veins:scenarios:../src/paradise:../src/veins --image-path=../images -l ../src/veins-5 --record-eventlog=false --debug-on-errors=false omnetpp.ini
#opp_run -r 0 -u Cmdenv -c nodebug -n ../examples/veins:scenarios:../src/paradise:../src/veins --image-path=../images -l ../src/veins-5 --record-eventlog=false --debug-on-errors=false omnetpp.ini
#Built Command:
#valgrind --leak-check=yes --tool=memcheck opp_run -r 0 -u Cmdenv -c nodebug -n ../examples/veins:scenarios:../src/paradise:../src/veins --image-path=../images -l ../src/veins-5 --record-eventlog=false --debug-on-errors=false omnetpp.ini
#valgrind --tool=callgrind --dump-instr=yes --trace-jump=yes --collect-jumps=yes --simulate-cache=yes opp_run -r 0 -u Cmdenv -c nodebug -n ../examples/veins:scenarios:../src/paradise:../src/veins --image-path=../images -l ../src/veins-5 --record-eventlog=false --debug-on-errors=false omnetpp.ini
#
#==13471== Memcheck, a memory error detector
#==13471== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
#==13471== Using Valgrind-3.11.0 and LibVEX; rerun with -h for copyright info
#==13471== Command: /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 0 -u Cmdenv -c Center -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../out/gcc-debug/src/veins --record-eventlog=false --debug-on-errors=false omnetpp_grid.ini
#==13471==
#
#
#valgrind --tool=massif --massif-out-file="massif.output.log" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 10 -u Cmdenv -c Center -n examples/veins:..:src/paradise:src/veins --image-path=images -l out/gcc-debug/src/veins --record-eventlog=false --debug-on-errors=false scenario/grid/omnetpp_grid.ini
#
cd /home/felipe/Simulation/BaconNet/scenario/grid
#
#Memory Leak & Profile
valgrind --tool=memcheck --leak-check=yes --leak-check=full --log-file="../../memcheck.output.log" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 10 -u Cmdenv -c Center -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../out/gcc-debug/src/veins --record-eventlog=false --debug-on-errors=false omnetpp_grid.ini
#
#Memory Allocation mapping
#valgrind --tool=massif --massif-out-file="../../massif.output.log" /home/felipe/Simulation/omnetpp-5.0/bin/opp_run -r 10 -u Cmdenv -c Center -n ../../examples/veins:..:../../src/paradise:../../src/veins --image-path=../../images -l ../../out/gcc-debug/src/veins --record-eventlog=false --debug-on-errors=false omnetpp_grid.ini
