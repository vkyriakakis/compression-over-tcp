#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

// Sends the stdin input to the server and receives it back, then it prints it to stdout
// Run with ./client <chunk-size-bytes>

#define SERVER_PORT 25659

int main(int argc, char *argv[]) {
	int sockfd;
	struct sockaddr_in server_addr;
	size_t bytes_read, total_read = 0;
	ssize_t bytes_recv, total_recv = 0;
	int is_eof = 0;
	char *chunk;
	int chunk_size;

	if (argc != 3) {
		fprintf(stderr, "Run with ./client <server-ip> <chunk-size-bytes>\n");
		return 1;
	}

	//fprintf(stderr, "Client started!\n");

	chunk_size = atoi(argv[2]);

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

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    	perror("connect");
    	return 1;
    }

    //fprintf(stderr, "Server accepted connection\n");

    while (!is_eof || total_recv < total_read) {
    	if (!is_eof) {
	    	bytes_read = fread(chunk, 1, chunk_size, stdin);
	    	if (bytes_read < chunk_size) {
	    		if (feof(stdin)) {
	    			is_eof = 1;

	    			// For the special case where the file size is a multiple of CHUNK_SIZE
	    			// and EOF is detected after all bytes have been received
	    			if (bytes_read == 0 && total_read == total_recv) {
	    				break;
	    			}
	    		}
	    		else {
	    			perror("fread");
	    			return 1;
	    		}
	    	}
	    	total_read += bytes_read;

	    	if (send(sockfd, chunk, bytes_read, 0) == -1) {
	    		perror("send");
	    		return 1;
	    	}
	    }

    	bytes_recv = recv(sockfd, chunk, chunk_size, 0);
    	if (bytes_recv == -1) {
    		perror("recv");
    		return 1;
    	}
    	total_recv += bytes_recv;

    	if (fwrite(chunk, 1, bytes_recv, stdout) != bytes_recv) {
    		perror("fwrite");
    		return 1;
    	}
    }

	// Read from stdin and send to server until EOF
    free(chunk);
	close(sockfd);

	return 0;
}