/* this project is created as a school assignment by Endre Palinkas */
/* the client is based on network code from class */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define PORT "2233"
#define MAXBUF 1024

// sets a socket to non-blocking
void SetNonblocking(int sock) {
	int opts = fcntl(sock, F_GETFL);
	opts = (opts | O_NONBLOCK);
	fcntl(sock, F_SETFL, opts);
}

int main(int argc, char* argv[])
{
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	int csock;
	char buffromstdin[MAXBUF];
	char buffromserver[MAXBUF];
	int lenfromstdin;
	int lenfromserver;
	int num_of_sockets_to_read;
	// sockets to give to select
	fd_set socks_to_process;
	// timeout for select
	struct timeval select_timeout;
	
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
		freeaddrinfo(res);
		return -1;
	}
	
	if(res == NULL) 
		return -1;

	// create the client socket
	csock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(csock < 0) {
		freeaddrinfo(res);
		perror("Error creating while creating the client socket.");
		return -1;
	}

	// connect
	if(connect(csock, res->ai_addr, res->ai_addrlen) < 0) {
		freeaddrinfo(res);
		perror("Error occured while connecting to the server.");
		return -1;
	}
	
	// ok we are connected

	
	// main loop
	// select() is used to determine in advance if there's data to read from csock and/or stdin
	// this way there's no blocking
	
	while(1) {	
		// we will monitor both stdin & the network socket with select
		// let's create the FD set to monitor
		// we need to do this in every iteration, as select() reduces the set if one FD is not ready to be read
		
		// empty the set
		FD_ZERO(&socks_to_process);
		// add the network socket
		FD_SET(csock, &socks_to_process);
		// add stdin
		FD_SET(STDIN_FILENO, &socks_to_process);		
		
		// let's check every 1 sec if there's anything on either stdin or the socket
		select_timeout.tv_sec = 1;
		select_timeout.tv_usec = 0;
		
		num_of_sockets_to_read = select(FD_SETSIZE, &socks_to_process, (fd_set *) 0, (fd_set *) 0, &select_timeout);
		
		// select has modified socks_to_process, only those remain which can be read without blocking
		
		#ifdef DEBUG
		if (0 == num_of_sockets_to_read) {
			printf("No input from stdin, no input from server.\n");
			fflush(stdout);
		}
		#endif /* DEBUG */
		
		if (0 != num_of_sockets_to_read) {
			
			// if csock is in the FD set, we have data from the chat server
			if (FD_ISSET(csock, &socks_to_process)) {
				// fill buffer with zeros
				bzero(buffromserver, MAXBUF);
				if ((lenfromserver = recv(csock, buffromserver, MAXBUF, 0)) > 0)
					printf("Chat server has sent: %s", buffromserver);
			}
			
			// if stdin is in the FD set, we have local data to send to the chat server
			if (FD_ISSET(STDIN_FILENO, &socks_to_process)) {			
				// fill buffer with zeros
				bzero(buffromstdin, MAXBUF);
				if ((lenfromstdin = read(STDIN_FILENO, buffromstdin, sizeof(buffromstdin))) > 0 ) 
					send(csock, buffromstdin, lenfromstdin, 0);				
			}
		}

	}

	// free the addrinfo struct
	freeaddrinfo(res);
	// TODO: add free also if we get term signal
	
	// close the socket
	close(csock);
	return 0;
	

}