/* this project is created as a school assignment by Endre Palinkas */
/* ncurses5 has to be installed to be able to compile this */

#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define PORT "2233"
#define MAX_MSG_LENGTH 80
#define MAX_SOCKET_BUF 1024
#define MAX_NICK_LENGTH 12

WINDOW *create_newwin(int height, int width, int starty, int startx);
void SetNonblocking(int sock);
int StrBegins(const char *haystack, const char *beginning);

int main(int argc, char *argv[]) {
	// network variables
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	int csock;
	char buffromserver[MAX_SOCKET_BUF];
	int lenfromserver;
	char tmp_msg[MAX_MSG_LENGTH];
	char tmp_nick[MAX_NICK_LENGTH];
	char tmp_buf[MAX_SOCKET_BUF];
	// for printing time for msgs
	time_t time_now;
	char tmp_time[8];

	// UI variables, windows paramaters
	WINDOW *nicklist_win;
	WINDOW *input_win;
	WINDOW *chat_win;
	int nicklist_win_startx, nicklist_win_starty, nicklist_win_width, nicklist_win_height;
	int input_win_startx, input_win_starty, input_win_width, input_win_height;
	int chat_win_startx, chat_win_starty, chat_win_width, chat_win_height, chat_win_currenty, chat_win_currentx;
	int user_input_char;
	// protocol limit for the message length
	char user_input_str[MAX_MSG_LENGTH];
	// todo think this through	
	// input limit may be reduced during runtime, if the user's terminal is too small
	int current_char;
	int max_input_length = MAX_MSG_LENGTH;
	// saved coordinates
	int saved_x, saved_y;

	// did we get a server as parameter
	if(argc != 2) {
		printf("Usage: %s <chat server IP> [nick] [pass]\n", argv[0]);
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
	// set the socket to non-blocking, we can read it even if its empty, without blocking
	SetNonblocking(csock);
	
	// ok we are connected

	
	// start curses mode
	initscr();
	// line buffering disabled, pass on every key press
	cbreak();
	// also handle function keys, like F10 for exit
	keypad(stdscr, TRUE);

	// set size for windows
	nicklist_win_height = LINES-3;
	nicklist_win_width = 14;
	nicklist_win_starty = 0;
	nicklist_win_startx = COLS-14;
	input_win_height = 3;
	input_win_width = COLS;
	input_win_starty = LINES-3;
	input_win_startx = 0;
	chat_win_height = LINES-3;
	chat_win_width = COLS-14;
	chat_win_starty = 0;
	chat_win_startx = 0;

	chat_win_currenty=1;
	chat_win_currentx=1;

	// we will count input characters, and only save them & write them to display MAX_MSG_LENGTH isn't reached yet
	noecho();
	current_char=0;


	refresh();
	nicklist_win = create_newwin(nicklist_win_height, nicklist_win_width, nicklist_win_starty, nicklist_win_startx);
	input_win = create_newwin(input_win_height, input_win_width, input_win_starty, input_win_startx);
	chat_win = create_newwin(chat_win_height, chat_win_width, chat_win_starty, chat_win_startx);


	// greeting msg
	mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "Welcome to the termchat client!");
	chat_win_currenty++;
	mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "To list other available commands: type /help. To exit: /exit.");
	chat_win_currenty++;
	wrefresh(chat_win);

	// if the users terminal size is too small, don't allow him to type messages too long	
	if (COLS-14<MAX_MSG_LENGTH) max_input_length=COLS-3;

	//move(LINES-2,1);
	wmove(input_win,1,1);
	wrefresh(input_win);

	// main loop, let's read user input character by character, until it's too long or we get a NewLine
	// wgetch should time out after 50ms, giving a chance to the main loop to read network data, even if there's no user interaction
	wtimeout(input_win, 50);

	while(1)
	{	
		if ((user_input_char=wgetch(input_win)) != ERR) {

			// handle backspace
			if (user_input_char == KEY_BACKSPACE || user_input_char == 127) {
				if (current_char > 0) {
					current_char--;
					getyx(input_win, saved_y, saved_x);
					mvwprintw(input_win, saved_y, saved_x-1, " ");
					wmove(input_win, saved_y, saved_x-1);
					wrefresh(input_win);
				}
				continue;
			}

			// if it is a newline character, the user finished typing a command/msg
			// it's time to evaluate the input
			if (user_input_char == '\n')
			{
				// let's close the string
				user_input_str[current_char]='\0';
				
				if (strstr(user_input_str,"/exit") || strstr(user_input_str,"/quit")  )
					break;
				if (strstr(user_input_str,"/help")) {
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "Showing help:");
					chat_win_currenty++;
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, " protecting your nick on this server with a password: /pass <password>");
					chat_win_currenty++;
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, " changing your nick: /nick <newnick> [password]");
					chat_win_currenty++;
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, " changing channel: /channel <newchannel>");
					chat_win_currenty++;
					}
					
				if ( !StrBegins(user_input_str, "/nick ")) {
					sscanf(user_input_str, "/nick %s", tmp_nick);
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "CMDNICK %s", tmp_nick);
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}
				
				else {
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "%s", user_input_str);
					if (chat_win_currenty < chat_win_height-2) 
						chat_win_currenty++;
					// todo: else scroll
					// send it to the server
					// let's prepare the message in the needed format
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "MSG %s", user_input_str);					
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}

				// reset input window with a horizontal line made of ' ' characters
				mvwhline(input_win, 1, 1, ' ', input_win_width-2);
				wmove(input_win,1,1);
				wrefresh(chat_win);
				wrefresh(input_win);
				// reset input string, because we got an NL
				bzero(user_input_str, MAX_MSG_LENGTH);
				current_char = 0;
				continue;
			}			

			// it is not a backspace or newline character
			// check the msg limit, don't allow user to write more
			if (current_char==MAX_MSG_LENGTH-1) {
				beep();
				continue;
			}

			// ok, not a newline, not a backspace, and limit is not reached yet
			// let's save the input character into our input string
			if (user_input_char!='\n') {
				user_input_str[current_char]=user_input_char;
				current_char++;
				wprintw(input_win, "%c", user_input_char);
			}
		}
		
		// we handled the keyboard input, now let's check if we have anything from the chat server
		
		// reset the previous buffer state, in which we will read the server msg
		bzero(buffromserver, MAX_SOCKET_BUF);
		// we can recv() without blocking, as csock is set to non-blocking
		if ((lenfromserver = recv(csock, buffromserver, MAX_SOCKET_BUF, 0)) > 0) {
			getyx(input_win, saved_y, saved_x);
			bzero(tmp_msg, MAX_MSG_LENGTH);

			
			// get current time into time_now, and then print into tmp_time string
			time_now = time (0);
			strftime(tmp_time, 9, "%H:%M:%S", localtime (&time_now));			
			
			if (!StrBegins(buffromserver, "MSGFROM ")) {
				// we process the message from the server
				// NewLines are never sent by the server, we use %[^\n] to read whitespaces in the string too
				sscanf(buffromserver, "MSGFROM %s %[^\n]", tmp_nick, tmp_msg);
				// print the received message on the screen in the finalized format
				mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "[%s] <%s> %s", tmp_time, tmp_nick, tmp_msg);
			}
			
			if (!StrBegins(buffromserver, "CMDOK ")) {
				sscanf(buffromserver, "CMDOK %[^\n]", tmp_msg);
				// print the received message on the screen in the finalized format
				mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "[%s] %s", tmp_time, tmp_msg);
			}
			
			if (!StrBegins(buffromserver, "CMDERROR ")) {
				sscanf(buffromserver, "CMDERROR %[^\n]", tmp_msg);			
				// print the received message on the screen in the finalized format
				mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "[%s] %s", tmp_time, tmp_msg);
			}			
			
			if (chat_win_currenty < chat_win_height-2) 
			chat_win_currenty++;
			// todo is there an overflow here? box redraws which solves the problem
			box(chat_win, 0 , 0);
			wrefresh(chat_win);
			wmove(input_win, saved_y, saved_x);
			wrefresh(input_win);
		}

	}

	// free windows from memory
	delwin(nicklist_win);
	delwin(input_win);
	delwin(chat_win);
	//end curses mode
	endwin();
	// free the addrinfo struct
	freeaddrinfo(res);
	// TODO: add free also if we get term signal

	// close the socket
	close(csock);	
	return 0;
}

WINDOW *create_newwin(int height, int width, int starty, int startx)
{	WINDOW *local_win;

	local_win = newwin(height, width, starty, startx);
	box(local_win, 0 , 0);		/* 0, 0 gives default characters 
					 * for the vertical and horizontal
					 * lines			*/
	wrefresh(local_win);		/* Show that box 		*/

	return local_win;
}

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
