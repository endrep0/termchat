/* this project is created as a school assignment by Endre Palinkas */
/* the server uses network code from the class literature "Linux Programozas" */
/* written by lecturer Gabor Banyasz & Tihamer Levendovszky, 2003  */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define PORT "2233"
#define MAX_CHAT_CLIENTS 15
#define MAXBUF 80

int server_socket, csock;
struct addrinfo hints;
struct addrinfo* res;
int err;
struct sockaddr_in6 addr;
socklen_t addrlen;
char ips[NI_MAXHOST];
char servs[NI_MAXSERV];
char buffer[MAXBUF];
int len;
int reuse;
const char msg_server_full[]="Sorry, the chat server is currently full. Try again later.\n";

// sockets to give to select
fd_set socks_to_process;
// this array will hold the connected client sockets
int connected_client_socks[MAX_CHAT_CLIENTS];

// sets a socket to non-blocking
void SetNonblocking(int sock) {
	int opts = fcntl(sock, F_GETFL);
	opts = (opts | O_NONBLOCK);
	fcntl(sock, F_SETFL, opts);
}

// creates the set of sockets that select needs to iterate through
// to be called from the main loop
void BuildSelectList() {
	int i;
	// empty the set
	FD_ZERO(&socks_to_process);
	
	// add the server socket
	FD_SET(server_socket, &socks_to_process);
	
	// add the client sockets which are connected
	for(i=0; i<MAX_CHAT_CLIENTS; i++) 
		if (0 != connected_client_socks[i]) 
			FD_SET(connected_client_socks[i],&socks_to_process);
}

// if we detect a new incoming connection, let's accept it if we have an empty slot
void HandleNewConnection(void) {
	int i;
	int client_socket;
	int getnameinfo_error;
	short connection_accepted = 0;
	
	client_socket = accept(server_socket, (struct sockaddr*)&addr, &addrlen);
	
	if (client_socket == -1)
		printf("Error occured while trying to accept connection\n");
	
	else {
		SetNonblocking(client_socket);
		// try to get client's ip:port string in a protocol-independent way, using getnameinfo()
		// we need the size of addr
		addrlen = sizeof(addr);
		// TODO: for some reason first one fails
		getnameinfo_error = getnameinfo((struct sockaddr*)&addr, addrlen, ips, sizeof(ips), servs, sizeof(servs), 0);
		// check if there's room for our socket
		for (i=0; (i < MAX_CHAT_CLIENTS) && (0 == connection_accepted); i++)
			if (0 == connected_client_socks[i]) {
				// we found a free slot, let's accept the client connection
				connected_client_socks[i]=client_socket;
				if (0 == getnameinfo_error) 
					printf("Chat client connection accepted from: %s:%s. Socket descriptor: %d, Socket index: %d\n", ips, servs, client_socket, i);
				else 
					printf("Chat client connection accepted. Cannot display client address, an error occured while looking it up.\n");
				connection_accepted = 1;
			}
		}
		
		if (0 == connection_accepted) {
			// went through all slots, and we couldn't put the new connection in any
			// we need to reject this client then
			if (0 == getnameinfo_error) 
				printf("Rejecting client connection from %s:%s, no free slots.\n", ips, servs);
			else 
				printf("Rejecting client connection. Cannot display client address, an error occured while looking it up.\n");
			send(client_socket, msg_server_full, strlen(msg_server_full), 0);
			close(client_socket);
		}
}

// ProcessPendingRead() to be called when we already know that one client has data to transfer
// Data is read & sent to the other chat clients
void ProcessPendingRead(int clientindex)
{
	int bytes_read;
	int i;
	
	do {
		// fill buffer with zeros
		bzero(buffer, MAXBUF);
		// receive the data
		bytes_read = recv(connected_client_socks[clientindex], buffer, MAXBUF, 0);
		
		if (0 == bytes_read) {
			// got disconnected from this client
			// there was an EOF, and this is read as 0 byte by recv()
			printf("Disconnected from a client. Socket descriptor: %d, Socket index: %d\n", connected_client_socks[clientindex], clientindex);
			close(connected_client_socks[clientindex]);
			connected_client_socks[clientindex] = 0;
			break;
		}
		
		if (bytes_read > 0) {
			// the connection is healthy
			// let's read the data, and send it to the other clients too
			printf("A client has sent: %s\n", buffer);
			for (i=0; i < MAX_CHAT_CLIENTS; i++) {
				// don't send it back to the source
				// TODO: i think 0 needs to rcv it too, so don't put clientindex>0 in if
				if (i!= clientindex)
					send(connected_client_socks[i], buffer, strlen(buffer), 0);
			}
		}
	} while (bytes_read > 0);
}

// to be called from the main loop
// in the case select reports that there is at least 1 socket that needs to be read
void ProcessSocketsToRead() {
	int i;
	// if there's a new connection request, select() will mark the socket as readable
	if (FD_ISSET(server_socket, &socks_to_process)) 
		HandleNewConnection();
	
	// let's iterate through the sockets
	// if a socket's file descriptor is in the socks_to_process set, then it needs to be read
	for(i=0; i<MAX_CHAT_CLIENTS; i++)
		if (FD_ISSET(connected_client_socks[i], &socks_to_process))
			ProcessPendingRead(i);
}

int main() {

	// timeout for select
	struct timeval select_timeout;
	int num_of_sockets_to_read;
	
	// let's assemble the local address, which is needed for the binding. we will use getaddrinfo() for this
	// AI_PASSIVE, so the addresses will be INADDR_ANY or IN6ADDR_ANY
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	
	// int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
	err = getaddrinfo(NULL, PORT, &hints, &res);
	if(err != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -1;
	}

	// TODO error msg
	if(res == NULL) return -1;
	
	// creating the server socket now
	// int socket(int domain, int type, int protocol);
	server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (server_socket < 0) {
	  perror("socket");
	  // TODO error msg
	  return -1;
	}

	// we allow reusing of sockets (SO_REUSEADDR). Socket level (SOL_SOCKET)
	// int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	reuse = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	// set the socket to non-blocking
	SetNonblocking(server_socket);
	
	// bind the server socket to the address, based on the reply of getaddrinfo()
	// int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	if (bind(server_socket, res->ai_addr, res->ai_addrlen) < 0) {
	  perror("bind");
	  return -1;
	}

	// let's listen for a connection
	// int listen(int sockfd, int backlog);
	if(listen(server_socket, 5) < 0) {
		perror("listen");
		return 1;
	}
	
	// we don't need the address linked list generated by getadrrinfo() anymore
	freeaddrinfo(res);
	
	// allocate memory for the client socket list
	memset((char *) &connected_client_socks, 0, sizeof(connected_client_socks));

	// main loop, we iterate through the sockets
	// accept connections if needed, read them if needed, giving them a small timeout	
	while (1)
	{
		BuildSelectList();
		select_timeout.tv_sec = 1;
		select_timeout.tv_usec = 0;
		// run the select. it will return if 
		// a) it can read from the set of sockets in socks_to_process (or EOF if disconnected)
		// b) after timeout
		num_of_sockets_to_read = select(FD_SETSIZE, &socks_to_process, (fd_set *) 0, (fd_set *) 0, &select_timeout);
		
		// select has modified socks_to_process, only those remain which can be read without blocking
		if (0 == num_of_sockets_to_read) {
			printf("No sockets to read.\n");
			fflush(stdout);
		}
		else {
			ProcessSocketsToRead();
		}
	}
	
	// maybe some graceful quit when keypressed Q
	// close(server_socket);
	return 0;
}
