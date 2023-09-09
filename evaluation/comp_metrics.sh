 #!/usr/bin/env bash

if [ $# -lt 2 ]; then
	echo "Run with $0 <iters> <list of at least one filename>"
	exit 1
fi

iters=$1

echo "buf_size, file, ratio mean, ratio stdev, total c_time, c_time mean, c_time stdev, total d_time, d_time mean, d_time stdev, c_cpu_load mean, d_cpu_load mean"

for size in 1024 5120 10240 20480 40960 81920 102400 204800 512000 1048576
do
	# Compile the library with a different buffer size
	gcc -DBUF_SIZE="$size" -DCOMP_METR -DNET_PERF -DCPU_USAGE -shared -fPIC measure_lib_perf.c -o measure_lib_perf.so -ldl -lz

	for f in "${@:2}"
	do
		metrics=(0 0 0 0 0 0 0 0 0 0)

		for ((i = 1 ; i <= $iters ; i++))
		do
			c_cpu_load=$(LD_PRELOAD=$PWD/measure_lib_perf.so ./oneway_client 127.0.0.1 10240 60000 n 2>&1 < "$f" | head -n1 | cut -d " " -f1)

			while ! python3.8 mininet_client.py 127.0.0.1 60000 0 > /dev/null ; do sleep 0.05; done &
			python3.8 mininet_server.py 60000

			d_cpu_load=$(LD_PRELOAD=$PWD/measure_lib_perf.so ./oneway_server 10240 60000 n 2>&1 > copy | cut -d " " -f1)

			new_metrics=( $(python3.8 compute_metrics.py) )
			new_metrics+=($c_cpu_load)
			new_metrics+=($d_cpu_load)

			if ! diff -q "$f" copy; then
				exit 1
			fi

			for k in {0..9}
			do
				metrics[k]=$(echo "${metrics[k]} + ${new_metrics[k]}" | bc)
			done

			rm *.log
		done

		echo -n "$size, $f"

		for k in {0..9}
		do
			metrics[k]=$(echo "scale=5; ${metrics[k]} / $iters" | bc)
			echo -n ", ${metrics[k]}"
		done

		echo
	done
done

rm copy

