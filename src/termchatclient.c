/* this project is created as a school assignment by Endre Palinkas */
/* the client uses network code from class */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define PORT "2233"

int main(int argc, char* argv[])
{
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	int csock;
	char buf[1024];
	int len;
	
	// did we get a server as parameter
	if(argc != 2) {
		printf("Usage: %s <chat server IP>\n", argv[0]);
		return -1;
	}
	
	// support both IPv4 and IPv6
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	// resolve address, and print any errors to stderr
	err = getaddrinfo(argv[1], PORT, &hints, &res);
	if(err != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -1;
	}
	
	if(res == NULL) 
		return -1;

	// create the client socket
	csock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(csock < 0) {
		perror("Error creating while creating the client socket.");
		return -1;
	}

	// connect
	if(connect(csock, res->ai_addr, res->ai_addrlen) < 0) {
		perror("Error occured while connecting to the server.");
		return -1;
	}
	
	// write the input coming on stdin to the socket
	while((len = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
		send(csock, buf, len, 0);
	}

	// free the addrinfo struct
	freeaddrinfo(res);

	// close the socket
	close(csock);
	return 0;
}