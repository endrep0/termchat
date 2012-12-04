#include <string.h>
#include <fcntl.h>
#include "termchatcommon.h"

// sets a socket to non-blocking
void SetNonblocking(int sock) {
	int opts = fcntl(sock, F_GETFL);
	opts = (opts | O_NONBLOCK);
	fcntl(sock, F_SETFL, opts);
}

// returns 0 if haystack begins with beginning (case sensitive), -1 if not
int StrBegins(const char *haystack, const char *beginning) {
	int i;
	if (NULL == haystack || NULL == beginning) 
		return -1;
	if (sizeof(beginning) > sizeof(haystack))
		return -1;
	
	// let's compare until the end of beginning
	for (i=0; beginning[i]!='\0'; i++) {
		if (haystack[i]!=beginning[i]) return -1;
	}
	
	// we got this for, so they match
	return 0;
}

// returns the number of parameters of a command
// 0 if it's a plain command with no parameters
// -1 if string is NULL
int CountParams(const char *cmd) {
	if (NULL == cmd ) return -1;
	int count=0;
	char cmd_copy[MAX_SOCKET_BUF];
	strcpy(cmd_copy, cmd);
	char *next_token;
	
	next_token = strtok(cmd_copy, " ");
	while (next_token != NULL) {
		count++;
		next_token = strtok(NULL, " ");	
	}
	
	// nr of parameters is 1 less than number of tokens
	return count-1;
}
