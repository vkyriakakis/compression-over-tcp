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

// Compile with gcc -DBUF_SIZE=204800 -shared -fPIC mytcp.c -o mytcp.so -ldl -lz

//#define BUF_SIZE 1024 // 126 KB
#define MAX_SOCKETS 1024

// Function pointer typedefs
typedef int (*orig_socket_t)(int domain, int type, int protocol);
typedef int (*orig_listen_t)(int sockfd, int backlog);
typedef int (*orig_accept_t)(int sockfd, struct sockaddr *addr, socklen_t *socklen);
typedef int (*orig_connect_t)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*orig_send_t)(int sockfd, const void *buf, size_t len, int flags);
typedef int (*orig_recv_t)(int sockfd, void *buf, size_t len, int flags);
typedef int (*orig_close_t)(int fd);

Bytef *send_buf[MAX_SOCKETS] = {NULL};
Bytef *send_comp_buf[MAX_SOCKETS] = {NULL};
Bytef *recv_comp_buf[MAX_SOCKETS] = {NULL};
Bytef *recv_buf[MAX_SOCKETS] = {NULL};
uLong send_len[MAX_SOCKETS] = {0}, recv_pos[MAX_SOCKETS] = {0}, recv_len[MAX_SOCKETS] = {0};

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
    int sockfd;
    int ret;
    orig_socket_t orig_socket = (orig_socket_t)dlsym(RTLD_NEXT,"socket");
    orig_close_t orig_close = (orig_close_t)dlsym(RTLD_NEXT,"close");

    sockfd = orig_socket(domain, type, protocol);
    if (sockfd < 0) {
        return -1;
    }

    if (domain == AF_INET && type == SOCK_STREAM) {
        if (_allocate_buffers(sockfd) < 0) {
            orig_close(sockfd);
            return -1;
        }
    }

    return sockfd;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *socklen) {
    orig_accept_t orig_accept = (orig_accept_t)dlsym(RTLD_NEXT,"accept");
    orig_close_t orig_close = (orig_close_t)dlsym(RTLD_NEXT,"close");
    int acceptfd;

    acceptfd = orig_accept(sockfd, addr, socklen);
    if (acceptfd < 0) {
        return -1;
    }

    if (_allocate_buffers(acceptfd) < 0) {
        orig_close(acceptfd);
        return -1;
    }

    return acceptfd;
}

int _send_comp_data(int sockfd, int comp_len, int flags) {
    orig_send_t orig_send = (orig_send_t)dlsym(RTLD_NEXT,"send");
    size_t total_sent = 0;
    ssize_t ret;
    int net_comp_len;

    // Convert to network byte order
    net_comp_len = htonl(comp_len);

    // Send the size of the compressed data, then the compressed data (make sure all of it gets sent)
    do {
        ret = orig_send(sockfd, (char*)&net_comp_len + total_sent, sizeof(int) - total_sent, flags);
        if (ret < 0) {
            perror("orig send size");
            return -1;
        }

        total_sent += ret;
    } while (total_sent < sizeof(int));

    total_sent = 0;

    do {
        ret = orig_send(sockfd, send_comp_buf[sockfd] + total_sent, comp_len - total_sent, flags);
        if (ret < 0) {
            perror("orig send data");
            return -1;

        }

        total_sent += ret;
    } while (total_sent < comp_len);

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

        ret = compress2(send_comp_buf[sockfd], &comp_len, (Bytef*)send_buf[sockfd], BUF_SIZE, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

        if (_send_comp_data(sockfd, comp_len, flags) < 0) {
            return -1;
        }
    }

    // While we have more data than what fits in a send buffer, get the compression units
    // straight from the provided buffer
    while (rem_len >= BUF_SIZE) {
        // Perform the compression using zlib
        comp_len = compressBound(BUF_SIZE);

        ret = compress2(send_comp_buf[sockfd], &comp_len, (Bytef*)(buf + len - rem_len), BUF_SIZE, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

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

int _recv_comp_data(int sockfd, uLong *comp_len_p, int flags, int received_bytes) {  
    orig_recv_t orig_recv = (orig_recv_t)dlsym(RTLD_NEXT,"recv");
    ssize_t ret;
    size_t total_recv = 0;
    int comp_len;

    // If at least some bytes have been received, don't block waiting forever
    if (received_bytes) {
        flags |= MSG_DONTWAIT;
    }

    do {
        // Read the size of the next compressed block
        ret = orig_recv(sockfd, (char*)&comp_len + total_recv, sizeof(int) - total_recv, flags);
        if (ret < 0) {
            return -1;
        }
        // The other side closed the connection, so no more blocks
        else if (ret == 0) {
            *comp_len_p = 0;
            return 0;
        }

        // If we received at least one byte of the header, 
        // we will block until we read the rest
        if (total_recv == 0) {
            flags &= ~MSG_DONTWAIT;
        }

        total_recv += ret;
    } while (total_recv < sizeof(int));

    comp_len = ntohl(comp_len);

    total_recv = 0;

    // Read the whole of the compressed block into the decompression buffer
    do {
        ret = orig_recv(sockfd, recv_comp_buf[sockfd] + total_recv, comp_len - total_recv, flags);
        if (ret < 0) {
            perror("orig recv data");
            return -1;
        }

        total_recv += ret;
    } while (total_recv < comp_len);

    *comp_len_p = comp_len;

    return 0;
}

// Must be called in recv so as to ensure R-R works with small messages, and in close
// in order to make sure whatever leftover data exists gets to the receiver
int _flush(int fd) {
    uLong comp_len;
    int ret;
    clock_t start, end;

    if (send_len[fd] > 0) {
        comp_len = compressBound(send_len[fd]);

        ret = compress2(send_comp_buf[fd], &comp_len, (Bytef*)send_buf[fd], 
                        send_len[fd], Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK) {
             _print_zlib_error("compress2", ret);
            errno = ENOMEM;
            return -1;
        }

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

        // Perform the decompression using zlib
        decomp_len = len - total_recv;
        ret = uncompress(buf + total_recv, &decomp_len, recv_comp_buf[sockfd], comp_len);
        if (ret != Z_OK) {
            fprintf(stderr, "total_recv = %lu, len = %lu, comp_len = %lu\n", total_recv, len, comp_len);
            _print_zlib_error("uncompress I", ret);
            errno = ENOMEM;
            return -1;
        }

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

    ret = uncompress(recv_buf[sockfd], &recv_len[sockfd], recv_comp_buf[sockfd], comp_len);
    if (ret != Z_OK) {
        _print_zlib_error("uncompress II", ret);
        errno = ENOMEM;
        return -1;
    }

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

int close(int fd) {
    orig_close_t orig_close = (orig_close_t)dlsym(RTLD_NEXT,"close");

    // If a send buffer for this fd exists, it corresponds to a TCP socket
    if (send_buf[fd] != NULL) {
        // If any data has been left in the socket's send buffer, compress it and send it
        if (_flush(fd) < 0) {
            return -1;
        }

        _deallocate_buffers(fd);
    }

    return orig_close(fd);
}