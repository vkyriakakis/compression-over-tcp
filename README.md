# thesis

## Setup
1. Install Mininet using the instructions at
http://mininet.org/download/.

2. Compile the test programs (needed for the evaluation platforms):
```
gcc oneway_client.c -o oneway_client
gcc oneway_server.c -o oneway_server
```

## To use with your programs:
1. Compile the library:
```
gcc -DBUF_SIZE=204800 -shared -fPIC comp_tcp_lib.c -o comp_tcp_lib.so -ldl -lz
```

2. Run the application with the library being loaded first:
```
LD_PRELOAD=$PWD/comp_tcp_lib.so <application-command>
```

## Compression library metrics:
```
./comp_metrics.sh <iters> <filename1> <filename2> ... <filenameN>
```
where
- **iters**: The amount of times each measurement is repeated to obtain an average
- **filename1 ... filenameN**: A list of test files to be used

The measurments are in CSV format and are output in stdout.

## Network measurements:
```
python3.8 net_experiment.py <filename> <chunk_size> <iters> <buf_size>
```
where
- **filename**: The name of the test file 
- **chunk_size**: The chunk size used by oneway_client and oneway_server to read/write from the disk
- **iters**: The amount of times each measurement is repeated to obtain an average
- **buf_size**: The library internal buffer size (BUF_SIZE in the text) 


