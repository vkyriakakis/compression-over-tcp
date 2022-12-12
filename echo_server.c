#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Echos each chunk of data it receives back to the client
// Run with ./server <data-chunk-bytes>

#define SERVER_PORT 25659

int main(int argc, char *argv[]) {
	int listen_fd, cli_fd;
	struct sockaddr_in server_addr;
	char *chunk;
	ssize_t bytes_recv;
	int CHUNK_SIZE;

	if (argc != 2) {
		fprintf(stderr, "Run with ./server <data-chunk-bytes>\n");
		exit(1);
	}
	CHUNK_SIZE = atoi(argv[1]);

	chunk = malloc(CHUNK_SIZE);
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
    server_addr.sin_port = htons(SERVER_PORT);

   	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
   		perror("bind");
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

   	fprintf(stderr, "Connection was made\n");

    do {
    	bytes_recv = recv(cli_fd, chunk, CHUNK_SIZE, 0);
    	if (bytes_recv == -1) {
    		perror("recv");
    		return 1;
    	}
    	else if (bytes_recv == 0) {
    		 break;
    	}

    	if (send(cli_fd, chunk, bytes_recv, 0) == -1) {
    		perror("send");
    		return 1;
    	}
    } while (1);

	close(cli_fd);
	close(listen_fd);
	free(chunk);

	return 0;
}