import time
import socket
import sys

# Run with <server_ip> <server_port>

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
	sock = socket.socket()
	sock.connect((sys.argv[1], int(sys.argv[2])))

	# Compute the difference from the reference time
	time_diff = int(time.time()*1000000) - comp_times[0]

   	# Send data to the server only when the compression time in the log has been reached
	for k in range(1, len(comp_times)):
		cur_time = int(time.time()*1000000 - time_diff)
		if cur_time  < comp_times[k]:
			time.sleep((comp_times[k] - cur_time) / 1000000)

		sock.sendall(bytearray(comp_buf_sizes[k]))

	sock.close()