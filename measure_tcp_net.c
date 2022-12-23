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

// Compile with gcc -shared -fPIC measure_tcp.c -o measure_tcp.so -ldl

// For measurements
FILE *comp_time_log = NULL, *comp_buf_log = NULL;
unsigned long *recv_times = NULL;
int recv_times_pos, recv_times_len;
unsigned long time_diff;

int _open_measurement_files(void) {
    // If the compression logs don't exist, create them
    if (access("comp_times.log", F_OK) != 0) {
        comp_time_log = fopen("comp_times.log", "w");
        if (comp_time_log == NULL) {
            perror("fopen comp_time_log");
            return -1;
        }

        comp_buf_log = fopen("comp_bufs.log", "w");
        if (comp_buf_log == NULL) {
            perror("fopen comp_buf_log");
            fclose(comp_time_log);
            return -1;
        }
    }
    // If they do, then the application is a server, so read the compressed buffers and the 
    // receive timestamps created by the mininet server
    else {
        comp_buf_log = fopen("comp_bufs.log", "r");
        if (comp_buf_log == NULL) {
            perror("fopen comp_buf_log");
            return -1;
        }
    }

    return 0;
}

int socket(int domain, int type, int protocol) {
    int ret;

    if (_open_measurement_files() < 0) {
        return -1;
    }

    return 4;
}

int _read_recv_times() {
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

    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return 0;
}

int listen(int sockfd, int backlog) {
    return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    struct timeval start_time;

    gettimeofday(&start_time, NULL);
    fprintf(comp_time_log, "%lu 0\n", start_time.tv_sec*1000000 + start_time.tv_usec);

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *socklen) {
    if (_read_recv_times() < 0) {
        return -1;
    }

    return 5;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    struct timeval comp_time;
    unsigned long comp_ts;

    gettimeofday(&comp_time, NULL);

    comp_ts = comp_time.tv_sec*1000000 + comp_time.tv_usec;

    // Write to the <comp_ts, buf_size> log
    fprintf(comp_time_log, "%lu %lu\n", comp_ts, len);

    // Write to the <comp_buf_size, comp_buf> log
    fwrite(&len, sizeof(len), 1, comp_buf_log);
    fwrite(buf, len, 1, comp_buf_log);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    unsigned long cur_time;
    struct timeval cur_timeval, prev_timeval;

    // If the receive times have ended, we reached EOF so signal it outside
    if (recv_times_pos == recv_times_len) {
        return 0;
    }

    gettimeofday(&cur_timeval, NULL);
    cur_time = cur_timeval.tv_sec * 1000000 + cur_timeval.tv_usec;
    cur_time -= time_diff;

    if (cur_time < recv_times[recv_times_pos]) {
        // Sleep for the difference between two timestamps (the start timestamp counts as the first one)
        // we need to adjust for the time spent in the app or decompressing because in the actual implementation
        // the buffers would arrive in parallel
        usleep(recv_times[recv_times_pos] - cur_time);
    }

    // then read a compressed buffer from the compressed buffer log
    fread(&len, sizeof(len), 1, comp_buf_log);
    fread(buf, len, 1, comp_buf_log);

    recv_times_pos++;

    return len;
}

int close(int fd) {
    // If a send buffer for this fd exists, it corresponds to a TCP socket
    if (fd == 5 || fd == 4) {
        if (comp_time_log != NULL) {
            fclose(comp_time_log);
            fclose(comp_buf_log);

            comp_time_log = comp_buf_log = NULL;
        }
        else if (comp_buf_log != NULL) {
            struct timeval end_time;

            // Output the measurement (time from connection start to server termination)
            gettimeofday(&end_time, NULL);
            fprintf(stderr, "%.5f\n", (double) (end_time.tv_sec*1000000 +end_time.tv_usec - time_diff - recv_times[0]) / 1000000);

            fclose(comp_buf_log);
            comp_buf_log = NULL;
            
            free(recv_times);
        }
    }

    return 0;
}