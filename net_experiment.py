from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import OVSBridge, OVSSwitch, RemoteController
from mininet.link import TCLink
from mininet.clean import Cleanup
import time

class SingleSwitchTopo(Topo):
    "Single switch connected to n hosts."
    def build(self, n=2, bw_up=1000, bw_down=1000, loss1=0, loss2=0, delay1=0, delay2=0):
        switch = self.addSwitch('s1', cls=OVSBridge)

        host = self.addHost('h1')
        self.addLink(host, switch, bw=bw_up, loss=loss1, delay=delay1)

        host = self.addHost('h2')
        self.addLink(host, switch, bw=bw_down, loss=loss2, delay=delay2)

def test(bw_up, bw_down, loss, delay, iters):
	server_port = 35600

	topo = SingleSwitchTopo(bw_up=bw_up, bw_down=bw_down, loss1=loss, loss2=0, 
		                    delay1=str(delay/2)+"ms", delay2=str(delay/2)+"ms")
	net = Mininet(topo, link=TCLink)#, controller = RemoteController("c0", ip="127.0.0.1", port=6633))

	net.start()

	h1 = net["h1"]
	h2 = net["h2"]

	# Test standard TCP for comparison
	avg_tcp_time = 0

	for i in range(iters):
		h1.sendCmd("while ! ./oneway_client {} 10240 {} < test_files/world192.txt; do sleep 0.05; done &".format(h2.IP(), server_port + i))
		avg_tcp_time += float(h2.cmd("./oneway_server 10240 {} 2>&1 > copy".format(server_port + i)))
		h1.waitOutput()

	avg_tcp_time /= iters

	# Test my library
	avg_comp_time = 0

	for i in range(iters):
		h1.sendCmd("while ! LD_PRELOAD=$PWD/mytcp.so ./oneway_client {} 10240 {} < test_files/world192.txt; do sleep 0.05; done &".format(h2.IP(), server_port + i + iters))
		avg_comp_time += float(h2.cmd("LD_PRELOAD=$PWD/mytcp.so ./oneway_server 10240 {} 2>&1 > copy".format(server_port + i + iters)))
		h1.waitOutput()

	avg_comp_time /= iters

	net.stop()

	return avg_tcp_time, avg_comp_time

if __name__ == "__main__":
	print("delay, bandwidth (Mbps), loss rate (%), tcp avg time (sec), lib avg time (sec)", flush = True)	

	# Realistic parameters

	# 4G: 20-30 ms, avg 20Mbps (download) 5-6Mbps (upload), 0.5% avg (εώς 40% αν 1000 users)
	# 5G:  avg 13 ms, avg 300Mbps (dl) 64 Mbps (ul) , avg packet loss rate = 0.1% (εώς 10% αν 1000 users)
	# Gbps Ethernet: 1Gbps, 0.1ms, 0% packet loss
	# 802.11n (WiFi): 40-50 Mbps, 10ms, 1% packet loss

	# # Gbps Eth
	# avg_tcp_time, avg_lib_time = test(1000, 1000, 0, 0.1, 10)
	# print("Gbps Eth, 0.1ms, 1000, 1000, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # 802.11n (WiFi)
	# avg_tcp_time, avg_lib_time = test(40, 40, 1, 10, 10)
	# print("WiFi, 10ms, 40, 40, 1%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # 4G
	# avg_tcp_time, avg_lib_time = test(20, 6, 0, 25, 10)
	# print("4G, 25ms, 20, 6, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # 5G
	# avg_tcp_time, avg_lib_time = test(300, 64, 0, 13, 10)
	# print("5G, 13ms, 300, 64, 0%, {}, {}".format(avg_tcp_time, avg_lib_time), flush = True)

	# # Delay plot measurements (perfect conditions)
	# bw = 1000
	# lr = 0
	# for d in [0, 1, 5, 10, 20, 40, 60, 100]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
	# 	print("{}, {}, {}".format(d, avg_tcp_time, avg_lib_time), flush = True)

	# # Bandwidth plot measurements (perfect conditions)
	# d = 0
	# lr = 0
	# for bw in [5, 10, 20, 40, 100, 200, 500, 800, 1000]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
	# 	print("{}, {}, {}".format(bw, avg_tcp_time, avg_lib_time), flush = True)

	# # Packet loss plot measurements (perfect conditions)
	# bw = 1000
	# d = 0
	# for lr in [0, 1, 2, 5, 8, 10]:
	# 	avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
	# 	print("{}, {}, {}".format(lr, avg_tcp_time, avg_lib_time), flush = True)

	# lr = 20
	# avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 1)
	# print("{}, {}, {}".format(lr, avg_tcp_time, avg_lib_time), flush = True)

	# Delay plot measurements (loss, bw)
	bw = 40
	lr = 2
	for d in [0, 1, 5, 10, 20, 40, 60, 100]:
		avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
		print("{}, {}, {}".format(d, avg_tcp_time, avg_lib_time), flush = True)

	# Bandwidth plot measurements (delay, loss)
	d = 20
	lr = 2  
	for bw in [5, 10, 20, 40, 100, 200, 500, 800, 1000]:
		avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
		print("{}, {}, {}".format(bw, avg_tcp_time, avg_lib_time), flush = True)	

	# Packet loss plot measurements (bw, delay)
	bw = 40
	d = 20
	for lr in [0, 1, 2, 5, 8, 10]:
		avg_tcp_time, avg_lib_time = test(bw, bw, lr, d, 10)
		print("{}, {}, {}".format(lr, avg_tcp_time, avg_lib_time), flush = True)


