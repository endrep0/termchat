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

#define DEBUG

#define PORT "2233"
#define MAX_MSG_LENGTH 80
#define MAX_SOCKET_BUF 1024
#define MAX_NICK_LENGTH 12
#define MAX_CHANNEL_LENGTH 12
#define MAX_IGNORES 10
#define CHAT_WINDOW_BUFFER_MAX_LINES 100

#define TRUE 1
#define FALSE 0

WINDOW *create_newwin(int height, int width, int starty, int startx);
void SetNonblocking(int sock);
int StrBegins(const char *haystack, const char *beginning);
void AddMsgToChatWindow(const char* msg, int timestamped);
void UpdateNicklist(char* nicklist);


// UI variables, windows paramaters
WINDOW *nicklist_win;
WINDOW *input_win;
WINDOW *chat_win;

int nicklist_win_startx, nicklist_win_starty, nicklist_win_width, nicklist_win_height;
int input_win_startx, input_win_starty, input_win_width, input_win_height;
int chat_win_startx, chat_win_starty, chat_win_width, chat_win_height, chat_win_currenty, chat_win_currentx;

char chat_window_buffer[CHAT_WINDOW_BUFFER_MAX_LINES][MAX_MSG_LENGTH];
int chat_window_buffer_position;


int main(int argc, char *argv[]) {
	int i;
	// network variables
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	int csock;
	char buffromserver[MAX_SOCKET_BUF];
	int lenfromserver;
	char tmp_msg[MAX_MSG_LENGTH];
	char msg_for_window[MAX_MSG_LENGTH];
	char tmp_nick1[MAX_NICK_LENGTH];
	char tmp_nick2[MAX_NICK_LENGTH];
	char tmp_buf[MAX_SOCKET_BUF];
	char tmp_chan[MAX_CHANNEL_LENGTH];
	char ignored_nicks[MAX_IGNORES][MAX_NICK_LENGTH];

	int user_input_char;
	// protocol limit for the message length
	char user_input_str[MAX_MSG_LENGTH];
	// todo think this through	
	// input limit may be reduced during runtime, if the user's terminal is too small
	int current_char;
	int max_input_length = MAX_MSG_LENGTH;
	// saved coordinates
	int saved_x, saved_y;
	// initialize chat window buffer
	chat_window_buffer_position=-1;
		
	
	//reset ignored nicks array
	for (i=0; i<MAX_IGNORES; i++)
		bzero(ignored_nicks[i],MAX_NICK_LENGTH);

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
		perror("Error creating while creating the client socket.\n");
		return -1;
	}

	// connect
	if(connect(csock, res->ai_addr, res->ai_addrlen) < 0) {
		freeaddrinfo(res);
		perror("Error occured while connecting to the server.\n");
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
	
	if (CHAT_WINDOW_BUFFER_MAX_LINES < chat_win_height-2) {
		freeaddrinfo(res);
		//end curses mode
		endwin();
		// close the socket
		close(csock);			
		fprintf(stderr, "Error, please set a higher CHAT_WINDOW_BUFFER_MAX_LINES.\n");
		return -1;
	}


	// we will count input characters, and only save them & write them to display MAX_MSG_LENGTH isn't reached yet
	noecho();
	current_char=0;


	refresh();
	nicklist_win = create_newwin(nicklist_win_height, nicklist_win_width, nicklist_win_starty, nicklist_win_startx);
	input_win = create_newwin(input_win_height, input_win_width, input_win_starty, input_win_startx);
	chat_win = create_newwin(chat_win_height, chat_win_width, chat_win_starty, chat_win_startx);


	// greeting msg
	AddMsgToChatWindow("Welcome to the termchat client!", false);
	AddMsgToChatWindow("To list other available commands: type /help. To exit: /exit.", false);	

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
					AddMsgToChatWindow("Showing help:", false);
					AddMsgToChatWindow(" protecting your nick on this server with a password: /pass <password>", false);
					AddMsgToChatWindow(" changing your nick: /nick <newnick> [password]", false);
					AddMsgToChatWindow(" changing channel: /channel <newchannel>", false);
					AddMsgToChatWindow(" private message: /msg <nick> <message>", false);
					AddMsgToChatWindow(" ignoring someone: /ignore nick", false);
					AddMsgToChatWindow(" to exit: /exit", false);					
					}
					
				else if ( !StrBegins(user_input_str, "/nick ")) {
					sscanf(user_input_str, "/nick %s", tmp_nick1);
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "CHANGENICK %s\n", tmp_nick1);
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}
				
				else if ( !StrBegins(user_input_str, "/channel ") ) {
					sscanf(user_input_str, "/channel %s", tmp_chan);
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "CHANGECHANNEL %s\n", tmp_chan);
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}

				// just an alternative command to /channel
				else if ( !StrBegins(user_input_str, "/join ") ) {
					sscanf(user_input_str, "/join %s", tmp_chan);
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "CHANGECHANNEL %s\n", tmp_chan);
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}	
				
				// send a private message, /msg <targetnick> <message>
				else if ( !StrBegins(user_input_str, "/msg ") ) {
					sscanf(user_input_str, "/msg %s %[^\n]", tmp_nick1, tmp_msg);
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "PRIVMSG %s %s\n", tmp_nick1, tmp_msg);
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
				}

				// add someone to the ignore list
				else if ( !StrBegins(user_input_str, "/ignore ") ) {
					sscanf(user_input_str, "/ignore %s", tmp_nick1);
					// cycle through ignore slots, to see if there's any free
					for (i=0; i<MAX_IGNORES && ignored_nicks[i]=='\0'; i++);
					if (i==MAX_IGNORES) {
						AddMsgToChatWindow("Ignore list full. You could remove an existing ignore using /unignore <nick>.", false);			
					}
					// todo unignore

					else {
						// ok we have a free slot on our hands, add the ignore
						strcpy(ignored_nicks[i],tmp_nick1);
						mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "Ignore list updated.");
						chat_win_currenty++;							
					}
				}	
				
				else {
					// if it didn't match any of the commands, it is a plain message to the channel
					// let's prepare it in the needed format, and send it to the server
					bzero(tmp_buf, MAX_SOCKET_BUF);
					sprintf(tmp_buf, "CHANMSG %s\n", user_input_str);					
					send(csock, tmp_buf, sizeof(tmp_buf), 0);
					// we don't print the message on the UI;
					// protocol says that we will get our own message back, which serves as an ACK by itself
					// so we will only print our own message if we get it from the server					
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
			
			// because of the stream behavior, buffromserver can have more than one messages from the server
			// these are separated by '\n', as per the protocol specification
			// we will tokenize buffromserver with separator '\n', and process each message one by one
			char *next_msg;
			next_msg = strtok(buffromserver, "\n");
			while (next_msg != NULL) {

				if (!StrBegins(next_msg, "CHANMSGFROM ")) {
					// we process the message from the server
					// NewLines are never sent by the server, we use %[^\n] to read whitespaces in the string too
					sscanf(next_msg, "CHANMSGFROM %s %[^\n]", tmp_nick1, tmp_msg);
					for (i=0; i<MAX_IGNORES ; i++) {
						if (!strcmp(ignored_nicks[i],tmp_nick1)) {
							#ifdef DEBUG
							AddMsgToChatWindow("Ignored a channel message.", true);
							#endif
							break;
						}
					}
					
					// if sender wasn't on ignore, print the received message on the screen in the finalized format
					// this is where we print accepted channel messages
					if (i==MAX_IGNORES) {
						sprintf(msg_for_window, "<%s> %s", tmp_nick1, tmp_msg);
						AddMsgToChatWindow(msg_for_window, true);
						break;
					}
				}
				
				if (!StrBegins(next_msg, "PRIVMSGFROM ")) {
					// we process the message from the server
					sscanf(next_msg, "PRIVMSGFROM %s %[^\n]", tmp_nick1, tmp_msg);
					for (i=0; i<MAX_IGNORES ; i++) {
						if (!strcmp(ignored_nicks[i],tmp_nick1)) {
							#ifdef DEBUG
							AddMsgToChatWindow("Ignored a private message.", true);
							#endif							
							break;
						}
					}				
					// if sender wasn't on ignore, print the received message on the screen in the finalized format
					if (i==MAX_IGNORES)	{
						sprintf(msg_for_window, "%s has sent you a private message: %s", tmp_nick1, tmp_msg);
						AddMsgToChatWindow(msg_for_window, true);
					}
				}
				
				if (!StrBegins(next_msg, "PRIVMSGOK ")) {
					sscanf(next_msg, "PRIVMSGOK %s %[^\n]", tmp_nick1, tmp_msg);
					sprintf(msg_for_window, "you sent a private message to %s: %s", tmp_nick1, tmp_msg);
					AddMsgToChatWindow(msg_for_window, true);
				}				
				
				if (!StrBegins(next_msg, "CHANGENICKOK ")) {
					sscanf(next_msg, "CHANGENICKOK %[^\n]", tmp_nick1);
					sprintf(msg_for_window, "Your nick is now %s.", tmp_nick1);
					AddMsgToChatWindow(msg_for_window, true);
				}
				
				if (!StrBegins(next_msg, "CHANUPDATECHANGENICK ")) {
					sscanf(next_msg, "CHANUPDATECHANGENICK %s %[^\n]", tmp_nick1, tmp_nick2);
					sprintf(msg_for_window, "%s is now known as %s", tmp_nick1, tmp_nick2);
					AddMsgToChatWindow(msg_for_window, true);					
				}					
				
				if (!StrBegins(next_msg, "CHANGECHANNELOK ")) {
					sscanf(next_msg, "CHANGECHANNELNELOK %[^\n]", tmp_chan);
					sprintf(msg_for_window, "You are now chatting in channel %s.", tmp_chan);
					AddMsgToChatWindow(msg_for_window, true);				
				}
				
				if (!StrBegins(next_msg, "CHANUPDATEJOIN ")) {
					sscanf(next_msg, "CHANUPDATEJOIN %[^\n]", tmp_nick1);
					sprintf(msg_for_window, "%s has joined the channel.", tmp_nick1);
					AddMsgToChatWindow(msg_for_window, true);					
				}
				

				if (!StrBegins(next_msg, "CHANUPDATELEAVE ")) {
					sscanf(next_msg, "CHANUPDATELEAVE %[^\n]", tmp_nick1);
					sprintf(msg_for_window, "%s has left the channel.", tmp_nick1);
					AddMsgToChatWindow(msg_for_window, true);							
				}
				
				if (!StrBegins(next_msg, "CHANUPDATEALLNICKS ")) {
					sscanf(next_msg, "CHANUPDATEALLNICKS %[^\n]", tmp_buf);
					sprintf(msg_for_window, "People in this channel: %s", tmp_buf);
					UpdateNicklist(tmp_buf);
					AddMsgToChatWindow(msg_for_window, true);						
				}						
				
				if (!StrBegins(next_msg, "CMDERROR ")) {
					sscanf(next_msg, "CMDERROR %[^\n]", tmp_msg);			
					AddMsgToChatWindow(tmp_msg, true);						
				}
				
				if (!StrBegins(next_msg, "CHANGECHANNELERROR ")) {
					sscanf(next_msg, "CHANGECHANNELERROR %[^\n]", tmp_msg);			
					AddMsgToChatWindow(tmp_msg, true);
				}				
				
				if (!StrBegins(next_msg, "CHANGENICKERROR ")) {
					sscanf(next_msg, "CHANGENICKERROR %[^\n]", tmp_msg);			
					AddMsgToChatWindow(tmp_msg, true);
				}				
			
				if (!StrBegins(next_msg, "CHANMSGERROR ")) {
					sscanf(next_msg, "CHANMSGERROR %[^\n]", tmp_msg);			
					AddMsgToChatWindow(tmp_msg, true);
				}
				
				if (!StrBegins(next_msg, "PRIVMSGERROR ")) {
					sscanf(next_msg, "PRIVMSGERROR %[^\n]", tmp_msg);			
					AddMsgToChatWindow(tmp_msg, true);
				}					
							
				// done with this token (message), let's move on to the next one
				next_msg = strtok(NULL, "\n");
			}
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
	// 0, 0: default border characters
	box(local_win, 0, 0);
	// show the box
	wrefresh(local_win);
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

// adds a message to chat window
// if timestamp == TRUE, it will add timestamp, otherwise it won't
void AddMsgToChatWindow(const char* msg, int timestamped) {
	// for saving the current input window coordinates
	int saved_x, saved_y;

	// for printing time for msgs
	time_t time_now;
	char tmp_time[8];	
	char tmp_msg[MAX_MSG_LENGTH];
	char msg_to_print[MAX_MSG_LENGTH];
	int i;
	
	getyx(input_win, saved_y, saved_x);
	bzero(tmp_msg, MAX_MSG_LENGTH);	
	
	if (timestamped) {
		// get current time into time_now, and then print into tmp_time string
		time_now = time (0);
		strftime(tmp_time, 9, "%H:%M:%S", localtime (&time_now));
		sprintf(msg_to_print, "[%s] %s", tmp_time, msg);
	}
	else {
		sprintf(msg_to_print, "%s", msg);

	}

	// increase chat_window_buffer_position
	chat_window_buffer_position++;
	if (chat_window_buffer_position < CHAT_WINDOW_BUFFER_MAX_LINES) {
		// if we aren't past the maximum, just write the line into the current position
		strcpy(chat_window_buffer[chat_window_buffer_position], msg_to_print);
	}
	else {
		// we are now at the maximum; we need to rotate the chat_window_buffer
		for (i=0; i < CHAT_WINDOW_BUFFER_MAX_LINES-1; i++) {
			// line 0 <--- line 1, line 1 <--- line 2, etc
			bzero(chat_window_buffer[chat_window_buffer_position], MAX_MSG_LENGTH);
			strcpy(chat_window_buffer[chat_window_buffer_position], chat_window_buffer[chat_window_buffer_position+1]);
		}
		// we have rotated the chat_window_buffer
		// add the new line to the last position
		strcpy(chat_window_buffer[CHAT_WINDOW_BUFFER_MAX_LINES-1], msg_to_print);
		// set the current buffer position the last line
		chat_window_buffer_position = CHAT_WINDOW_BUFFER_MAX_LINES-1;
	}
	
	// now the chat_window_buffer is updated in the memory
	// we now need to redraw the chat window contents based on the chat_window_buffer
	chat_win_currenty=1; 
	chat_win_currentx=1;	
	
	// if current line position is smaller than the window height (minus borders)
	// we can print all existing lines from line 0 on the screen
	if (chat_window_buffer_position < (chat_win_height-2)) {
		for (i=0; i<=chat_window_buffer_position; i++) {
			// print the new line to the screen
			mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, chat_window_buffer[i]);		
			chat_win_currenty++;
		}
	}
	
	// otherwise we need to print the last (chat_win_height-2) lines from the chat_window_buffe
	else {
		for (i = (chat_window_buffer_position - (chat_win_height-3)); i<=chat_window_buffer_position; i++) {
			// reset the line to make sure there won't be any junk left, if we overwrite a longer line
			mvwhline(chat_win, chat_win_currenty, chat_win_currentx, ' ', MAX_MSG_LENGTH);
			mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, chat_window_buffer[i]);
			chat_win_currenty++;			
		}
	}
	
	// draw the borders of the chat window
	box(chat_win, 0 , 0);
	// cursor should go back to where it was in the input window
	wmove(input_win, saved_y, saved_x);
	// redraw the chat & input windows
	wrefresh(chat_win);
	wrefresh(input_win);			
}

void UpdateNicklist(char* nicklist) {
	// for saving the current input window coordinates
	int saved_x, saved_y;
	// where to put first nick; not on the borders, (1,1) is the correct position
	int nicklist_win_currenty=1;
	int nicklist_win_currentx=1;

	getyx(input_win, saved_y, saved_x);	
	char *next_nick;
	next_nick = strtok(nicklist, " ");
	while (next_nick != NULL) {
		mvwprintw(nicklist_win, nicklist_win_currenty, nicklist_win_currentx, "%s", next_nick);
		nicklist_win_currenty++;
		// done with this token (nick), let's move on to the next one
		next_nick = strtok(NULL, "\n");	
	}
	wmove(input_win, saved_y, saved_x);
	wrefresh(nicklist_win);
	wrefresh(input_win);
}