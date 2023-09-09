#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>

// Echos each chunk of data it receives back to the client
// Run with ./server <data-chunk-bytes>

int main(int argc, char *argv[]) {
	int listen_fd, cli_fd, control_fd;
	struct sockaddr_in server_addr, server_addr_control;
	char *chunk;
	ssize_t bytes_recv;
	int chunk_size;
	int server_port;
	char should_measure;
	struct timeval end;

	if (argc != 4) {
		fprintf(stderr, "Run with ./server <data-chunk-bytes> <data port> <measure [y/n]>\n");
		return 1;
	}
	chunk_size = atoi(argv[1]);
	server_port = atoi(argv[2]);
	should_measure = argv[3][0] == 'y' ? 1 : 0;

	chunk = malloc(chunk_size);
	if (chunk == NULL) {
		perror("malloc");
		return 1;
	}

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1) {
		perror("socket");
		return 1;
	}

	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);

   	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
   		perror("bind1");
   		return 1;
   	}

   	if (listen(listen_fd, 5) == -1) {
   		perror("listen");
   		return 1;
   	}

   	cli_fd = accept(listen_fd, NULL, NULL);
   	if (cli_fd == -1) {
   		perror("accept");
   		return 1;
   	}

    do {
    	bytes_recv = recv(cli_fd, chunk, chunk_size, 0);
    	if (bytes_recv == -1) {
    		perror("recv");
    		return 1;
    	}
    	else if (bytes_recv == 0) {
    		break;
    	}

    	if (fwrite(chunk, 1, bytes_recv, stdout) < 0) {
    		perror("fwrite");
    		return 1;
    	}
    } while (1);

    if (should_measure) {
    	gettimeofday(&end, NULL);
    	fprintf(stderr, "%lu\n", end.tv_sec*1000000 + end.tv_usec);
    }

    close(cli_fd);
	close(listen_fd);
	free(chunk);

	return 0;
}
