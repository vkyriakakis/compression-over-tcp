from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import OVSBridge, OVSSwitch, RemoteController
from mininet.link import TCLink
from mininet.clean import Cleanup
import random 
import time
import os
import subprocess
import sys

class SingleSwitchTopo(Topo):
    "Single switch connected to n hosts."
    def build(self, n=2, bw_up=1000, bw_down=1000, loss1=0, loss2=0, delay1=0, delay2=0):
        switch = self.addSwitch('s1', cls=OVSBridge)

        host = self.addHost('h1')
        self.addLink(host, switch, bw=bw_up, loss=loss1, delay=delay1)

        host = self.addHost('h2')
        self.addLink(host, switch, bw=bw_down, loss=loss2, delay=delay2)

def test(bw_up, bw_down, loss, delay, iters, filename, chunk_size):
	server_port = 35600

	topo = SingleSwitchTopo(bw_up=bw_up, bw_down=bw_down, loss1=loss, loss2=0, 
		                    delay1=str(delay/2)+"ms", delay2=str(delay/2)+"ms")
	net = Mininet(topo, link=TCLink)#, controller = RemoteController("c0", ip="127.0.0.1", port=6633))

	net.start()

	h1 = net["h1"]
	h2 = net["h2"]

	# Test plain TCP with the sequential execution method
	avg_tcp_time = 0

	for i in range(iters):
		subprocess.run("LD_PRELOAD=$PWD/measure_tcp_net.so ./oneway_client 127.0.0.1 {} 60000 n < test_files/{}".format(chunk_size, filename), shell=True)

		h1.sendCmd("while ! python3.8 mininet_client.py {} {} ; do sleep 0.05; done &".format(h2.IP(), server_port + i + iters))
		h2.cmd("python3.8 mininet_server.py {}".format(server_port + i + iters))
		h1.waitOutput()

		p = subprocess.run("LD_PRELOAD=$PWD/measure_tcp_net.so ./oneway_server {} 60000 n 2>&1 > copy".format(chunk_size), capture_output=True, shell=True)
		avg_tcp_time += float(p.stdout)

		subprocess.run("rm *.log", shell=True)

	avg_tcp_time /= iters

	# Test my library with the new way of measuring
	avg_comp_time = 0

	for i in range(iters):
		subprocess.run("LD_PRELOAD=$PWD/measure_lib_perf.so ./oneway_client 127.0.0.1 {} 60000 n < test_files/{}".format(chunk_size, filename), shell=True)

		h1.sendCmd("while ! python3.8 mininet_client.py {} {} ; do sleep 0.05; done &".format(h2.IP(), server_port + i + iters))
		h2.cmd("python3.8 mininet_server.py {}".format(server_port + i + iters))
		h1.waitOutput()

		p = subprocess.run("LD_PRELOAD=$PWD/measure_lib_perf.so ./oneway_server {} 60000 n 2>&1 > copy".format(chunk_size), capture_output=True, shell=True)
		avg_comp_time += float(p.stdout)

		subprocess.run("rm *.log", shell=True)

	avg_comp_time /= iters

	net.stop()

	return avg_tcp_time, avg_comp_time

if __name__ == "__main__":
	print("delay, bandwidth (Mbps), loss rate (%), tcp avg time (sec), lib avg time (sec)", flush = True)

	if len(sys.argv) != 5:
		print("Run with python3.8 net_experiment.py <filename> <chunk_size> <iters> <buf_size>")
		sys.exit(1)

	filename = sys.argv[1]
	chunk_size = int(sys.argv[2])
	iters = int(sys.argv[3])
	buf_size = sys.argv[4]

	os.system("gcc -DBUF_SIZE={} -DNET_PERF -shared -fPIC measure_lib_perf.c -o measure_lib_perf.so -ldl -lz".format(buf_size))

	# Realistic parameters

	# 4G: 20-30 ms, avg 20Mbps (download) 5-6Mbps (upload), 0.5% avg (εώς 40% αν 1000 users)
	# 5G:  avg 13 ms, avg 300Mbps (dl) 64 Mbps (ul) , avg packet loss rate = 0.1% (εώς 10% αν 1000 users)
	# Gbps Ethernet: 1Gbps, 0.1ms, 0% packet loss
	# 802.11n (WiFi): 40-50 Mbps, 10ms, 1% packet loss

	# # # Gbps Eth
	# avg_tcp_time, avg_lib_time = test(1000, 1000, 0, 0.1, iters, filename, chunk_size)
	# print("Gbps Eth, 0.1ms, 1000, 1000, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # 802.11n (WiFi)
	# avg_tcp_time, avg_lib_time = test(40, 40, 1, 10, iters, filename, chunk_size)
	# print("WiFi, 10ms, 40, 40, 1%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# 4G
	avg_tcp_time, avg_lib_time = test(6, 20, 0, 25, iters, filename, chunk_size)
	print("4G, 25ms, 20, 6, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# 5G
	avg_tcp_time, avg_lib_time = test(64, 300, 0, 13, iters, filename, chunk_size)
	print("5G, 13ms, 300, 64, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # Delay plot measurements (perfect conditions)
	# bw = 1000
	# lr = 0
	# for d in [0, 1, 5, 10, 20, 40, 60, 100]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(d, avg_tcp_time, avg_lib_time), flush = True)

	# # Bandwidth plot measurements (perfect conditions)
	# d = 0
	# lr = 0
	# for bw in [5, 10, 20, 40, 100, 200, 500, 800, 1000]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(bw, avg_tcp_time, avg_lib_time), flush = True)

	# # Packet loss plot measurements (perfect conditions)
	# bw = 1000
	# d = 0
	# for lr in [0, 1, 2, 5, 8, 10]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(lr, avg_tcp_time, avg_lib_time), flush = True)

	# # Delay plot measurements (loss, bw)
	# bw = 40
	# lr = 2
	# for d in [0, 1, 5, 10, 20, 40, 60, 100]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(d, avg_tcp_time, avg_lib_time), flush = True)

	# # Bandwidth plot measurements (delay, loss)
	# d = 20
	# lr = 2  
	# for bw in [5, 10, 20, 40, 100, 200, 500, 800, 1000]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(bw, avg_tcp_time, avg_lib_time), flush = True)	

	# # Packet loss plot measurements (bw, delay)
	# bw = 40
	# d = 20
	# for lr in [0, 1, 2, 5, 8, 10]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, iters, filename, chunk_size)
	# 	print("{}, {}, {}".format(lr, avg_tcp_time, avg_lib_time), flush = True)


