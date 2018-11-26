#!/bin/bash
ITERATIONS=1
THREADS=20

export STM_STATS=1

echo "Started run_all_comparisons_stm"

python hope_config_writer.py -detection_mode 3
python hope_config_writer.py -heuristic_mode 15

for b in $(seq 1 $ITERATIONS)
do	
	echo "Running iteration $b..."

	python hope_config_writer.py -commits_round 6000
	numactl --physcpubind=+0-$(( $THREADS-1 )) stamp/vacation/./vacation -n4 -q60 -u90 -r1048576 -t14019430400 -c$THREADS     
	numactl --physcpubind=+0-$(( $THREADS-1 )) stamp/intruder/./intruder -a10 -l128 -n26214400 -s1 -t$THREADS
	python hope_config_writer.py -commits_round 3000
	numactl --physcpubind=+0-$(( $THREADS-1 )) stamp/genome/./genome -g56384 -s128 -n16777216000 -t$THREADS
	python hope_config_writer.py -commits_round 20000
	numactl --physcpubind=+0-$(( $THREADS-1 )) stamp/ssca2/./ssca2 -s22 -i1.0 -u1.0 -l3 -p3 -t$THREADS
done
