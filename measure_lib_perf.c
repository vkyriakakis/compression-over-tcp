#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <zlib.h>

// Compile with gcc -DBUF_SIZE=204800 -shared -fPIC measure_lib_perf.c -o measure_lib_perf.so -ldl -lz

#define MAX_SOCKETS 1024

Bytef *send_buf[MAX_SOCKETS] = {NULL};
Bytef *send_comp_buf[MAX_SOCKETS] = {NULL};
Bytef *recv_comp_buf[MAX_SOCKETS] = {NULL};
Bytef *recv_buf[MAX_SOCKETS] = {NULL};
uLong send_len[MAX_SOCKETS] = {0}, recv_pos[MAX_SOCKETS] = {0}, recv_len[MAX_SOCKETS] = {0};

// For performance measurements
FILE *comp_buf_log = NULL;

#ifdef COMP_METR

FILE *metrics_log = NULL;

#endif

#ifdef CPU_USAGE

struct timeval real_start, real_end;
clock_t cpu_start, cpu_end;

#endif

// For the mininet tests
#ifdef NET_PERF

FILE *comp_time_log = NULL;
uLong *recv_times = NULL;
int recv_times_pos, recv_times_len;
uLong time_diff;

#endif

void _print_zlib_error(char *msg, int code) {
    fprintf(stderr, "%s: ", msg);
    switch(code) {
        case Z_MEM_ERROR:
            fprintf(stderr, "MEM ERROR\n");
            break;
        case Z_BUF_ERROR: 
            fprintf(stderr, "BUF ERROR\n");
            break;
        case Z_DATA_ERROR:
            fprintf(stderr, "DATA ERROR\n");
            break;
        default:
            fprintf(stderr, "??????\n");
    }
}

int _init_client_measurements(void) {
    comp_buf_log = fopen("comp_bufs.log", "w");
    if (comp_buf_log == NULL) {
        perror("fopen comp_buf_log");
        return -1;
    }

    #ifdef NET_PERF

    struct timeval start_time;

    comp_time_log = fopen("comp_times.log", "w");
    if (comp_time_log == NULL) {
        perror("fopen comp_time_log");
        return -1;
    }

    // Write the reference time (moment client connects) to the comp times log for
    // the rest of the measurement programs to access
    gettimeofday(&start_time, NULL);
    fprintf(comp_time_log, "%lu 0\n", start_time.tv_sec*1000000 + start_time.tv_usec);

    #endif

    #ifdef COMP_METR

    metrics_log = fopen("comp_metrics.log", "w");
    if (metrics_log == NULL) {
        perror("fopen comp_metrics_log");
        fclose(comp_buf_log);
        return -1;
    }

    #endif

    #ifdef CPU_USAGE

    // Start the CPU util measurements
    gettimeofday(&real_start, NULL);
    cpu_start = clock();

    #endif

    return 0;
}

int _init_server_measurements(void) {
    comp_buf_log = fopen("comp_bufs.log", "r");
    if (comp_buf_log == NULL) {
        perror("fopen comp_buf_log");
        return -1;
    }

    #ifdef NET_PERF

    FILE *recv_time_log;
    struct timeval cur_time;

    recv_time_log = fopen("recv_times.log", "r");
    if (recv_time_log == NULL) {
        perror("fopen recv_time_log");
        fclose(comp_buf_log);
        return -1;
    }

    // Read the number of recv times
    fscanf(recv_time_log, "%d", &recv_times_len);

    // Allocate the recv times array and fill it
    recv_times = malloc(recv_times_len * sizeof(*recv_times));
    if (recv_times == NULL) {
        return -1;
    }

    for (int i = 0 ; i < recv_times_len ; i++) {
        fscanf(recv_time_log, "%lu", &recv_times[i]);
    }

    recv_times_pos = 1;

    // Compute the time difference between now and the "start time"
    gettimeofday(&cur_time, NULL);
    time_diff = cur_time.tv_sec*1000000 + cur_time.tv_usec - recv_times[0];

    fclose(recv_time_log);

    #endif

    #ifdef COMP_METR

    metrics_log = fopen("decomp_metrics.log", "w");
    if (metrics_log == NULL) {
        perror("fopen decomp_metrics_log");
        fclose(comp_buf_log);
        return -1;
    }

    #endif

    #ifdef CPU_USAGE

    // Start the CPU util measurements
    gettimeofday(&real_start, NULL);
    cpu_start = clock();

    #endif

    return 0;
}

int _allocate_buffers(int sockfd) {
    uLong comp_bound;
    Bytef *send_buf_temp, *recv_buf_temp;
    Bytef *send_comp_buf_temp, *recv_comp_buf_temp;

    send_buf_temp = malloc(BUF_SIZE);
    if (send_buf_temp == NULL) {
        errno = ENOMEM;
        return -1;
    }

    recv_buf_temp = malloc(BUF_SIZE);
    if (recv_buf_temp == NULL) {
        errno = ENOMEM;
        free(send_buf_temp);
        return -1;
    }

    comp_bound = compressBound(BUF_SIZE);

    send_comp_buf_temp = malloc(comp_bound);
    if (send_comp_buf_temp == NULL) {
        errno = ENOMEM;
        free(send_buf_temp);
        free(recv_buf_temp);
        return -1;
    }

    recv_comp_buf_temp = malloc(comp_bound);
    if (recv_comp_buf_temp == NULL) {
        errno = ENOMEM;
        free(send_buf_temp);
        free(recv_buf_temp);
        free(send_comp_buf_temp);
        return -1;
    }

    send_buf[sockfd] = send_buf_temp;
    recv_buf[sockfd] = recv_buf_temp;
    send_comp_buf[sockfd] = send_comp_buf_temp;
    recv_comp_buf[sockfd] = recv_comp_buf_temp;

    return 0;
}

void _deallocate_buffers(int sockfd) {
    free(send_buf[sockfd]);
    free(recv_buf[sockfd]);
    free(send_comp_buf[sockfd]);
    free(recv_comp_buf[sockfd]);

    send_buf[sockfd] = NULL;
    recv_buf[sockfd] = NULL;
    send_comp_buf[sockfd] = NULL;
    recv_comp_buf[sockfd] = NULL;
}

int socket(int domain, int type, int protocol) {
    int sockfd = 4;
    int ret;

    if (_allocate_buffers(sockfd) < 0) {
        return -1;
    }

    return sockfd;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (_init_client_measurements() < 0) {
        return -1;
    }

    return 0;
}

int listen(int sockfd, int backlog) {
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *socklen) {
    int acceptfd = 5;

    if (_init_server_measurements() < 0) {
        return -1;
    }

    if (_allocate_buffers(acceptfd) < 0) {
        return -1;
    }

    return acceptfd;
}

int _send_comp_data(int sockfd, int comp_len, int flags) {
    #ifdef NET_PERF

    struct timeval comp_time;
    unsigned long comp_ts;

    gettimeofday(&comp_time, NULL);
    comp_ts = comp_time.tv_sec*1000000 + comp_time.tv_usec;

    // Write to the <comp_ts, buf_size> log
    fprintf(comp_time_log, "%lu %d\n", comp_ts, comp_len);

    #endif

    // Write to the <comp_buf_size, comp_buf> log
    fwrite(&comp_len, sizeof(comp_len), 1, comp_buf_log);
    fwrite(send_comp_buf[sockfd], comp_len, 1, comp_buf_log);
    
    if (ferror(comp_buf_log)) {
        perror("fwrite @ _send_comp_data");
        return -1;
    }

    return 0;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    uLong comp_len;
    size_t rem_len = len;
    int ret;
    clock_t start, end;

    // If the data wouldn't fill the send buffer, put it inside and return
    if (len + send_len[sockfd] < BUF_SIZE) {
        memcpy(send_buf[sockfd] + send_len[sockfd], buf, len);
        send_len[sockfd] += len;
        rem_len = 0;
    }
    // Else if the data would overflow and a send buffer is not empty, compress and send it
    else if (send_len[sockfd] > 0) {
        memcpy(send_buf[sockfd] + send_len[sockfd], buf, BUF_SIZE - send_len[sockfd]);
        rem_len = rem_len - BUF_SIZE + send_len[sockfd];
        send_len[sockfd] = 0;

        // Perform the compression using zlib
        comp_len = compressBound(BUF_SIZE);

        #ifdef COMP_METR

        start = clock();

        #endif

        ret = compress2(send_comp_buf[sockfd], &comp_len, (Bytef*)send_buf[sockfd], BUF_SIZE, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

        #ifdef COMP_METR

        end = clock();
        fprintf(metrics_log, "%lf %lf\n", (double)(end - start) / CLOCKS_PER_SEC, (double) BUF_SIZE / comp_len);

        #endif

        if (_send_comp_data(sockfd, comp_len, flags) < 0) {
            return -1;
        }
    }

    // While we have more data than what fits in a send buffer, get the compression units
    // straight from the provided buffer
    while (rem_len >= BUF_SIZE) {
        // Perform the compression using zlib
        comp_len = compressBound(BUF_SIZE);

        #ifdef COMP_METR

        start = clock();

        #endif

        ret = compress2(send_comp_buf[sockfd], &comp_len, (Bytef*)(buf + len - rem_len), BUF_SIZE, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

        #ifdef COMP_METR

        end = clock();
        fprintf(metrics_log, "%lf %lf\n", (double)(end - start) / CLOCKS_PER_SEC, (double) BUF_SIZE / comp_len);

        #endif

        if (_send_comp_data(sockfd, comp_len, flags) < 0) {
            return -1;
        }

        rem_len -= BUF_SIZE;
    }

    // If any data was left, put it in the buffer for future compression
    if (rem_len > 0) {
        memcpy(send_buf[sockfd], buf + len - rem_len, rem_len);
        send_len[sockfd] = rem_len;
    }

    // From the POV of the application, the send() call handled all the requested bytes
    return len;
}

// Read the compressed buffers from the compressed buffer log made by the client
// since in measuring mode the client and server run sequentially
int _read_comp_buf_log(int sockfd, uLong *comp_len_p) {
    int comp_len;

    #ifdef NET_PERF

    uLong cur_time;
    struct timeval cur_timeval, prev_timeval;

    // If the receive times have ended, we reached EOF so signal it outside
    if (recv_times_pos == recv_times_len) {
        *comp_len_p = 0;
        return 0;
    }

    gettimeofday(&cur_timeval, NULL);
    cur_time = cur_timeval.tv_sec * 1000000 + cur_timeval.tv_usec;
    cur_time -= time_diff;

    if (cur_time  < recv_times[recv_times_pos]) {
        // Sleep for the difference between two timestamps (the start timestamp counts as the first one)
        // we need to adjust for the time spent in the app or decompressing because in the actual implementation
        // the buffers would arrive in parallel
        usleep(recv_times[recv_times_pos] - cur_time);
    }

    // Read the compressed buffer size
    fread(&comp_len, sizeof(comp_len), 1, comp_buf_log);
    fread(recv_comp_buf[sockfd], comp_len, 1, comp_buf_log);

    recv_times_pos++;

    #else

    // Try to read the compressed buffer size
    fread(&comp_len, sizeof(comp_len), 1, comp_buf_log);

    // If the receive times have ended, we reached EOF so signal it outside
    if (feof(comp_buf_log)) {
        *comp_len_p = 0;
        return 0;
    }
    else if (ferror(comp_buf_log)) {
        perror("fread _read_comp_buf");
        return -1;
    }

    fread(recv_comp_buf[sockfd], comp_len, 1, comp_buf_log);

    #endif

    *comp_len_p = comp_len;

    return 0;
}

int _recv_comp_data(int sockfd, uLong *comp_len_p, int flags, int received_bytes) {  
    return _read_comp_buf_log(sockfd, comp_len_p);
}

// Must be called in recv so as to ensure R-R works with small messages, and in close
// in order to make sure whatever leftover data exists gets to the receiver
int _flush(int fd) {
    uLong comp_len;
    int ret;
    clock_t start, end;

    if (send_len[fd] > 0) {
        comp_len = compressBound(send_len[fd]);

        #ifdef COMP_METR

        start = clock();

        #endif

        ret = compress2(send_comp_buf[fd], &comp_len, (Bytef*)send_buf[fd], 
                        send_len[fd], Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

        #ifdef COMP_METR

        end = clock();
        fprintf(metrics_log, "%lf %lf\n", (double)(end - start) / CLOCKS_PER_SEC, (double) send_len[fd] / comp_len);

        #endif

        if (_send_comp_data(fd, comp_len, 0) < 0) {
            return -1;
        }

        send_len[fd] = 0;
    }
    
    return 0;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    uLong comp_len;
    uLong decomp_len;
    int ret;
    size_t total_recv = 0;
    clock_t start, end;

    // If the requested data is <= that in the receive buffer, just fetch the data
    if (len < recv_len[sockfd] - recv_pos[sockfd]) {
        memcpy(buf, recv_buf[sockfd] + recv_pos[sockfd], len);
        recv_pos[sockfd] += len;
        total_recv = len;
    }
    else {
        // Else get the remaining data in the buffer then move on to decompressing new data
        if (recv_len[sockfd] - recv_pos[sockfd] > 0) {
            memcpy(buf, recv_buf[sockfd] + recv_pos[sockfd], recv_len[sockfd] - recv_pos[sockfd]);
            total_recv += recv_len[sockfd] - recv_pos[sockfd];
            recv_pos[sockfd] = recv_len[sockfd] = 0;
        }

        // Flush the send buffer because a reply might never arrive otherwise
        if (_flush(sockfd) < 0) {
            return -1;
        }
    }

    // While the needed data isn't less than a receive buffer
    while (len - total_recv >= BUF_SIZE) {
        if (_recv_comp_data(sockfd, &comp_len, flags, total_recv > 0) < 0) {
            if (total_recv > 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                comp_len = 0;
            }
            else {
                return -1;
            }
        }

        // If the compressed data has 0 length, there is no more data to be received (EOF or non block)
        if (comp_len == 0) {
            return total_recv;
        }

        #ifdef COMP_METR

        start = clock();

        #endif

        // Perform the decompression using zlib
        decomp_len = len - total_recv;
        ret = uncompress(buf + total_recv, &decomp_len, recv_comp_buf[sockfd], comp_len);
        if (ret != Z_OK) {
            fprintf(stderr, "total_recv = %lu, len = %lu, comp_len = %lu\n", total_recv, len, comp_len);
            _print_zlib_error("uncompress I", ret);
            errno = ENOMEM;
            return -1;
        }

        #ifdef COMP_METR

        end = clock();
        fprintf(metrics_log, "%lf\n", (double)(end - start) / CLOCKS_PER_SEC);

        #endif

        total_recv += decomp_len;
    }

    if (total_recv == len) {
        return total_recv;
    }

    // Handle the remaining data by fetching a whole compression unit,
    // taking only what is needed, and storing the rest in the recv buffer for later
    if (_recv_comp_data(sockfd, &comp_len, flags, total_recv > 0) < 0) {
        if (total_recv > 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            comp_len = 0;
        }
        else {
            return -1;
        }    
    }

    // If the compressed data has 0 length, there is no more data to be received
    if (comp_len == 0) {
        return total_recv;
    }

    // Perform the decompression using zlib
    recv_len[sockfd] = BUF_SIZE;

    #ifdef COMP_METR

    start = clock();

    #endif

    ret = uncompress(recv_buf[sockfd], &recv_len[sockfd], recv_comp_buf[sockfd], comp_len);
    if (ret != Z_OK) {
        _print_zlib_error("uncompress II", ret);
        errno = ENOMEM;
        return -1;
    }

    #ifdef COMP_METR

    end = clock();
    fprintf(metrics_log, "%lf\n", (double)(end - start) / CLOCKS_PER_SEC);

    #endif

    // Copy the needed bytes to the buffer (a flush might have happened, so check if the data is enough)
    if (len - total_recv < recv_len[sockfd]) {
        memcpy(buf + total_recv, recv_buf[sockfd], len - total_recv);
        recv_pos[sockfd] = len - total_recv;
        total_recv = len;
    }
    else {
        memcpy(buf + total_recv, recv_buf[sockfd], recv_len[sockfd]);
        total_recv += recv_len[sockfd];
        recv_len[sockfd] = 0;
    }

    return total_recv;
}

void _end_measurements(int fd) {
    if (comp_buf_log != NULL) {
        #ifdef CPU_USAGE

        double cpu_time, real_time;
        long cores;

        // Print the percentage of the CPU usage with respect to all of the available processors (max 100%)
        gettimeofday(&real_end, NULL);
        real_time = (double)(real_end.tv_sec*1000000 + real_end.tv_usec - real_start.tv_sec*1000000 - real_start.tv_usec) / 1000000;
        
        cpu_end = clock();
        cpu_time = (double)(cpu_end - cpu_start) / CLOCKS_PER_SEC;

        cores = sysconf(_SC_NPROCESSORS_ONLN);

        fprintf(stderr, "%.2lf %lf\n", (cpu_time / cores / real_time) * 100, cpu_time / cores);

        #endif

        #ifdef COMP_METR

        fclose(metrics_log);
        metrics_log = NULL;

        #endif

        #ifdef NET_PERF

        // Server case, accept returns 5
        if (fd == 5) {
            struct timeval end_time;

            // Output the measurements (time from connection start to server termination and the CPU usage)
            gettimeofday(&end_time, NULL);
            fprintf(stderr, "%.5f\n", (double) (end_time.tv_sec*1000000 +end_time.tv_usec - time_diff - recv_times[0]) / 1000000);
        
            free(recv_times);
        }
        // Client case, the 4 output by socket() is used
        else if (fd == 4) {
            fclose(comp_time_log);
            comp_time_log = NULL;
        }

        #endif

        fclose(comp_buf_log);
        comp_buf_log = NULL;
    }
}

int close(int fd) {
    // If a send buffer for this fd exists, it corresponds to a TCP socket
    if (send_buf[fd] != NULL) {
        // If any data has been left in the socket's send buffer, compress it and send it
        if (_flush(fd) < 0) {
            return -1;
        }

        _end_measurements(fd);
        _deallocate_buffers(fd);
    }

    return 0;
}