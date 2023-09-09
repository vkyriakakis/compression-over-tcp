import time
import socket
import sys

# Run with <server_ip> <server_port> <cpu time / cores>

if __name__ == "__main__":
	comp_times = []
	comp_buf_sizes = []

	# Read the (comp_ts, comp_buf_size) pairs from the log left by the actual client
	with open("comp_times.log") as comp_times_log:
		for line in comp_times_log:
			comp_ts, comp_buf_size = line.split()
			comp_times.append(int(comp_ts))
			comp_buf_sizes.append(int(comp_buf_size))

	# Connect to the mininet server
	try:
		sock = socket.socket()
		sock.connect((sys.argv[1], int(sys.argv[2])))
	except:
		sys.exit(1)

	# Start time for CPU load
	start_time = time.time()

   	# Send data to the server only when the compression time in the log has been reached
	for k in range(1, len(comp_times)):
		time.sleep((comp_times[k] - comp_times[k-1]) / 1000000)
		sock.sendall(bytearray(comp_buf_sizes[k]))

	# End time for CPU load
	end_time = time.time()

	# Output the CPU load for the compression side
	print((float(sys.argv[3]) / (end_time - start_time)) * 100)

	sock.close()
