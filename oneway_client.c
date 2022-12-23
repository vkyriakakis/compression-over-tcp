#include <stdio.h>
#include <netinet/in.h> 
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

// Sends the stdin input to the server, which stores it in a file
// Run with ./client <chunk-size-bytes>

int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in server_addr;
	size_t bytes_read, total_read = 0;
	int is_eof = 0;
	char *chunk;
	int chunk_size;
	int ret;
	int SERVER_PORT;
	char should_measure;
	struct timeval start;

	if (argc != 5) {
		fprintf(stderr, "Run with ./client <server-ip> <chunk-size-bytes> <server-port> <measure [y/n]>\n");
		return 1;
	}

	chunk_size = atoi(argv[2]);
	SERVER_PORT = atoi(argv[3]);
	should_measure = argv[4][0] == 'y' ? 1 : 0;

	chunk = malloc(chunk_size);
	if (chunk == NULL) {
		perror("malloc");
		return 1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}

	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
	server_addr.sin_port = htons(SERVER_PORT);

	ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret == -1) {
		perror("connect");
		return 1;
	}

	if (should_measure) {
		gettimeofday(&start, NULL);
    	printf("%lu\n", start.tv_sec*1000000 + start.tv_usec);
	}

    while (!is_eof) {
    	bytes_read = fread(chunk, 1, chunk_size, stdin);
    	if (bytes_read < chunk_size) {
    		if (feof(stdin)) {
    			is_eof = 1;
    		}
    		else {
    			perror("fread");
    			return 1;
    		}
    	}

    	if (send(sockfd, chunk, bytes_read, MSG_NOSIGNAL) == -1) {
    		perror("send");
    		return 1;
    	}
    }
    
    free(chunk);
	close(sockfd);

	return 0;
}