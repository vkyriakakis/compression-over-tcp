import socket
import time
import sys

# Run with <server_port>

def recvall(sock, n):
    # Helper function to recv n bytes or return None if EOF is hit
    data = bytearray()
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            return None

        data.extend(packet)

    return data

if __name__ == "__main__":
	recv_times = []
	comp_buf_sizes = []
	comp_times = []

	# Create the recv time log
	recv_times_log = open("recv_times.log", "w")

	# Read the (comp_ts, comp_buf_size) pairs from the log left by the actual client
	with open("comp_times.log") as comp_times_log:
		for line in comp_times_log:
			comp_time, comp_buf_size = line.split()
			comp_times.append(int(comp_time))
			comp_buf_sizes.append(int(comp_buf_size))

	#print((comp_times[-1] - comp_times[0]) / 1000000, file = sys.stderr)

	# Connect to the client
	listen_sock = socket.socket()
	listen_sock.bind(("0.0.0.0", int(sys.argv[1])))
	listen_sock.listen()
	cli_sock, _ = listen_sock.accept()	

	# Compute the time difference between the actual client start time and
	# the mininet server start time (after connection)
	time_diff = int(time.time()*1000000) - comp_times[0]

	# Add the start time to the receive times to be written
	recv_times.append(comp_times[0])

	# Read every buffer from the client
	for comp_buf_size in comp_buf_sizes[1:]:
		comp_buf = recvall(cli_sock, comp_buf_size)
		recv_times.append(int(time.time()*1000000 - time_diff))

	# Print the recv_times into the log
	print(len(recv_times), file = recv_times_log)
	for recv_ts in recv_times:
		print(recv_ts, file = recv_times_log)

	# Print the total time elapsed so far
	#print((recv_times[-1] - recv_times[0]) / 1000000, file = sys.stderr)

	cli_sock.close()
	listen_sock.close()