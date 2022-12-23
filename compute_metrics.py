import sys
import statistics

def my_stdev(values):
	if len(values) < 2:
		return 0
	
	return statistics.stdev(values)

ratios = []
comp_times = []
decomp_times = []

with open("comp_metrics.log", 'r') as comp_file, open("decomp_metrics.log", 'r') as decomp_file:
	for line in comp_file:
		comp_time, ratio = line.split()
		comp_times.append(float(comp_time))
		ratios.append(float(ratio))

	for line in decomp_file:
		decomp_times.append(float(line))

# Output ratio avg, ratio stdev, comp time total, 
# comp chunk time avg + stdev, decomp total, decomp chunk time avg + stdev
print("{:0.5f} {:0.5f} {:0.5f} {:0.5f} {:0.5f} {:0.5f} {:0.5f} {:0.5f}".format(statistics.mean(ratios),
								                                        my_stdev(ratios),
								                                        sum(comp_times),
								                                        statistics.mean(comp_times),
								                                        my_stdev(comp_times),
								                                        sum(decomp_times),
								                                        statistics.mean(decomp_times),
								                                        my_stdev(decomp_times)))
