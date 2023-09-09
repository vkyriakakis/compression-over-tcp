# thesis

A C library that allows applications to communicate through a compressed TCP stream in a transparent manner
by intercepting calls to the Unix socket API, that was developed during my thesis. I have also included
the scripts I used for validation and evaluation, as well as the thesis itself with all the
implementation details and the performance measurements.

## Setup
- Install Mininet using the instructions at
http://mininet.org/download/.

## To use with your programs:
- Compile the library:
```
gcc -DBUF_SIZE=204800 -shared -fPIC comp_tcp_lib.c -o comp_tcp_lib.so -ldl -lz
```

- Run the application with the library being loaded first:
```
LD_PRELOAD=$PWD/comp_tcp_lib.so <application-command>
```

## To run the validation+evaluation scripts:

* Compile the test programs (needed for the evaluation platforms):
```
gcc oneway_client.c -o oneway_client
gcc oneway_server.c -o oneway_server
```

### Compression library metrics:
```
./comp_metrics.sh <iters> <filename1> <filename2> ... <filenameN>
```
where
- **iters**: The amount of times each measurement is repeated to obtain an average
- **filename1 ... filenameN**: A list of test files to be used

The measurments are in CSV format and are output in stdout.

### Network measurements:
```
python3.8 net_experiment.py <filename> <chunk_size> <iters> <buf_size>
```
where
- **filename**: The name of the test file 
- **chunk_size**: The chunk size used by oneway_client and oneway_server to read/write from the disk
- **iters**: The amount of times each measurement is repeated to obtain an average
- **buf_size**: The library internal buffer size (BUF_SIZE in the text) 


