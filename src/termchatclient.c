/* this project is created as a school assignment by Endre Palinkas */
/* ncurses5 & openssl (libssl-dev) has to be installed to be able to compile this */

#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/evp.h>
#include "termchatcommon.h"
#include "termchatclient.h"

// UI variables, windows paramaters
WINDOW *nicklist_win;
WINDOW *input_win;
WINDOW *chat_win;

int nicklist_win_startx, nicklist_win_starty, nicklist_win_width, nicklist_win_height;
int input_win_startx, input_win_starty, input_win_width, input_win_height;
int chat_win_startx, chat_win_starty, chat_win_width, chat_win_height, chat_win_currenty, chat_win_currentx;

char chat_window_buffer[CHAT_WINDOW_BUFFER_MAX_LINES][MAX_MSG_LENGTH];
int chat_window_buffer_last_element_index;
int chat_window_currently_showing_first;
int chat_window_currently_showing_last;

// other stuff
int csock;
char ignored_nicks[MAX_IGNORES][MAX_NICK_LENGTH+1];
	
int main(int argc, char *argv[]) {
	int i;
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	// protocol limit for the message length
	char user_command[MAX_MSG_LENGTH+1];	
	int user_input_char;
	char buffromserver[MAX_SOCKET_BUF];
	int lenfromserver;

	// initialize chat window buffer
	chat_window_buffer_last_element_index=-1;
	// for scrolling with keyboard
	
	// for saving what position of the chat_window_buffer we show; this is needed for scrolling
	// we visualize different parts of the buffer when scrolling up/down
	// they won't be used until first line is added to the chat_window_buffer, and then we will be showing index[0]-index[0]
	chat_window_currently_showing_first=0;
	chat_window_currently_showing_last=0;
		
	
	//reset ignored nicks array
	for (i=0; i<MAX_IGNORES; i++)
		bzero(ignored_nicks[i],MAX_NICK_LENGTH);

	// did we get a server as parameter
	if(argc != 3) {
		printf("Usage: %s <chat server IP> <port>\n", argv[0]);
		return -1;
	}

	// support both IPv4 and IPv6
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// resolve address, and print any errors to stderr
	err = getaddrinfo(argv[1], argv[2], &hints, &res);
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
	
	// ok we are connected, let's init the display
	// quit if failed
	if (InitCursesDisplay()) {
		// free the addrinfo struct
		freeaddrinfo(res);
		// close the socket
		close(csock);			
		return -1;
	}


	// main loop, let's read user input character by character, until it's too long or we get a NewLine
	// wgetch should time out after 50ms, giving a chance to the main loop to read network data, even if there's no user interaction
	wtimeout(input_win, 50);
	
	// reset user command string
	bzero(user_command,MAX_MSG_LENGTH);

	while(1)
	{	
		if ((user_input_char=wgetch(input_win)) != ERR) {
			HandleKeypress(user_input_char, user_command);
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
				HandleMessageFromServer(next_msg);
				// done with this token (message), let's move on to the next one
				next_msg = strtok(NULL, "\n");
			}
		}

	}
	// free curses stuff
	EndCursesDisplay();
	// free the addrinfo struct
	freeaddrinfo(res);
	// close the socket
	close(csock);	
	return 0;
}

// init the display
// will return -1 on error
int InitCursesDisplay(void) {
	// start curses mode
	initscr();
	// line buffering disabled, pass on every key press
	cbreak();
	// also handle function keys, like F10 for exit
	keypad(stdscr, TRUE);

	// set size for windows
	// input window at the bottom
	input_win_height = 3;
	input_win_width = COLS;
	input_win_starty = LINES-input_win_height;
	input_win_startx = 0;
	
	// nicklist window, above the input window, right side
	nicklist_win_height = LINES-input_win_height;
	nicklist_win_width = MAX_NICK_LENGTH+2;
	nicklist_win_starty = 0;
	nicklist_win_startx = COLS-nicklist_win_width;

	// chat window, above the input window, left side
	chat_win_height = LINES-input_win_height;
	chat_win_width = COLS-nicklist_win_width;
	chat_win_starty = 0;
	chat_win_startx = 0;
	
	if (CHAT_WINDOW_BUFFER_MAX_LINES < chat_win_height-2) {
		endwin();
		fprintf(stderr, "Error, please set a higher CHAT_WINDOW_BUFFER_MAX_LINES.\n");
		return -1;
	}


	// we will count input characters, and only save them & write them to display MAX_MSG_LENGTH isn't reached yet
	noecho();
	refresh();
	nicklist_win = create_newwin(nicklist_win_height, nicklist_win_width, nicklist_win_starty, nicklist_win_startx);
	input_win = create_newwin(input_win_height, input_win_width, input_win_starty, input_win_startx);
	chat_win = create_newwin(chat_win_height, chat_win_width, chat_win_starty, chat_win_startx);
	// greeting msg
	AddMsgToChatWindow("Welcome to the termchat client!", false);
	AddMsgToChatWindow("To exit: /exit, /help for available commands.", false);	

	wmove(input_win,1,1);
	wrefresh(input_win);
	return 0;
}

void EndCursesDisplay(void) {
	// free windows from memory
	delwin(nicklist_win);
	delwin(input_win);
	delwin(chat_win);
	//end curses mode
	endwin();
}
	

void HandleKeypress(int input_character, char *user_command) {
	int i;
	char tmp_msg[MAX_MSG_LENGTH+1];
	char tmp_nick1[MAX_NICK_LENGTH+1];
	char tmp_pass[MAX_PASS_LENGTH+1];
	char tmp_buf[MAX_SOCKET_BUF];
	char tmp_chan[MAX_CHANNEL_LENGTH+1];
	// saved coordinates
	int saved_x, saved_y;	
	int current_char = strlen(user_command);

	// handle backspace
	if (input_character == KEY_BACKSPACE || input_character == 127) {
		if (current_char > 0) {
			getyx(input_win, saved_y, saved_x);
			mvwprintw(input_win, saved_y, saved_x-1, " ");
			user_command[current_char-1]='\0';
			wmove(input_win, saved_y, saved_x-1);
			wrefresh(input_win);
		}
		return;
	}
	
	// handle up arrow key for scrolling chat window
	if (input_character == KEY_UP ) {
		ScrollChatWindow(SCROLL_DIRECTION_UP);
		return;
	}
	
	// handle down arrow key for scrolling chat window
	if (input_character == KEY_DOWN) {
		ScrollChatWindow(SCROLL_DIRECTION_DOWN);
		return;
	}

	// if we get an escape key, we need to get 2 more chars to see if it's up or down arrow
	if (input_character == 27) {
		input_character=wgetch(input_win);
		input_character=wgetch(input_win);
		
		// handle up arrow key for scrolling chat window
		if (input_character == 65 ) {
			ScrollChatWindow(SCROLL_DIRECTION_UP);
			return;
		}
		
		// handle down arrow key for scrolling chat window
		if (input_character == 66) {
			ScrollChatWindow(SCROLL_DIRECTION_DOWN);
			return;
		}
	}

	// if it is a newline character, the user finished typing a command/msg
	// it's time to evaluate the input
	if (input_character == '\n')
	{
		// let's close the string
		user_command[current_char]='\0';
		
		if (strstr(user_command,"/exit") || strstr(user_command,"/quit")  )
			return;
		if (strstr(user_command,"/help")) {
			AddMsgToChatWindow("Showing help:", false);
			AddMsgToChatWindow(" protecting your nick on this server with a password: /pass <password>", false);
			AddMsgToChatWindow(" changing your nick: /nick <newnick> [password]", false);
			AddMsgToChatWindow(" changing channel: /channel <newchannel>", false);
			AddMsgToChatWindow(" private message: /msg <nick> <message>", false);
			AddMsgToChatWindow(" ignoring someone: /ignore nick", false);
			AddMsgToChatWindow(" to exit: /exit", false);					
			}
			
		else if ( !StrBegins(user_command, "/nick ")) {
			if (CountParams(user_command) == 1) {
				sscanf(user_command, "/nick %s", tmp_nick1);
				bzero(tmp_buf, MAX_SOCKET_BUF);
				sprintf(tmp_buf, "CHANGENICK %s\n", tmp_nick1);
				send(csock, tmp_buf, sizeof(tmp_buf), 0);
			}
			else if (CountParams(user_command) == 2) {
				sscanf(user_command, "/nick %s %s", tmp_nick1, tmp_pass);
				// calculate the hash of the password
				SHA512(tmp_pass, password_sha512);						
				bzero(tmp_buf, MAX_SOCKET_BUF);
				sprintf(tmp_buf, "CHANGENICK %s %s\n", tmp_nick1, password_sha512);
				send(csock, tmp_buf, sizeof(tmp_buf), 0);
				bzero(password_sha512, 129);
			}
			else
				AddMsgToChatWindow("usage: /nick <newnick> or /nick <newnick> <password>", false);
		}
		
		else if ( !StrBegins(user_command, "/pass ")) {
			bzero(tmp_pass, MAX_PASS_LENGTH);
			sscanf(user_command, "/pass %s", tmp_pass);
			bzero(tmp_buf, MAX_SOCKET_BUF);
			// calculate the hash of the password
			SHA512(tmp_pass, password_sha512);
			
			
			sprintf(tmp_buf, "CHANGEPASS %s\n", password_sha512);
			send(csock, tmp_buf, sizeof(tmp_buf), 0);
			bzero(password_sha512, 129);
		}				
		
		else if ( !StrBegins(user_command, "/channel ") ) {
			sscanf(user_command, "/channel %s", tmp_chan);
			bzero(tmp_buf, MAX_SOCKET_BUF);
			sprintf(tmp_buf, "CHANGECHANNEL %s\n", tmp_chan);
			send(csock, tmp_buf, sizeof(tmp_buf), 0);
		}

		// just an alternative command to /channel
		else if ( !StrBegins(user_command, "/join ") ) {
			sscanf(user_command, "/join %s", tmp_chan);
			bzero(tmp_buf, MAX_SOCKET_BUF);
			sprintf(tmp_buf, "CHANGECHANNEL %s\n", tmp_chan);
			send(csock, tmp_buf, sizeof(tmp_buf), 0);
		}	
		
		// send a private message, /msg <targetnick> <message>
		else if ( !StrBegins(user_command, "/msg ") ) {
			sscanf(user_command, "/msg %s %[^\n]", tmp_nick1, tmp_msg);
			bzero(tmp_buf, MAX_SOCKET_BUF);
			sprintf(tmp_buf, "PRIVMSG %s %s\n", tmp_nick1, tmp_msg);
			send(csock, tmp_buf, sizeof(tmp_buf), 0);
		}

		// add someone to the ignore list
		else if ( !StrBegins(user_command, "/ignore ") ) {
			sscanf(user_command, "/ignore %s", tmp_nick1);
			// cycle through ignore slots, to see if there's any free
			for (i=0; i<MAX_IGNORES && ignored_nicks[i]=='\0'; i++);
			if (i==MAX_IGNORES) {
				AddMsgToChatWindow("Ignore list full.", false);			
			}
			// todo unignore

			else {
				// ok we have a free slot on our hands, add the ignore
				strcpy(ignored_nicks[i],tmp_nick1);
				AddMsgToChatWindow("Ignore list updated.", false);		
			}
		}	
		
		else {
			// if it didn't match any of the commands, it is a plain message to the channel
			// let's prepare it in the needed format, and send it to the server
			bzero(tmp_buf, MAX_SOCKET_BUF);
			sprintf(tmp_buf, "CHANMSG %s\n", user_command);					
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
		bzero(user_command, MAX_MSG_LENGTH);
		current_char = 0;
		return;
	}			

	// it is not a backspace or newline character
	// check the msg limit, don't allow user to write more
	if (current_char==MAX_MSG_LENGTH) {
		beep();
		return;
	}

	// ok, not a newline, not a backspace, and limit is not reached yet
	// let's save the input character into our input string
	if (input_character!='\n') {
		user_command[current_char]=input_character;
		wprintw(input_win, "%c", input_character);
	}
}

// handle a message from server
// uses AddMsgToChatWindow() to add messages to the chat window
// and UpdateNicklist() to refresh nicklist when needed
void HandleMessageFromServer(char *message_from_server) {
	int i;
	char tmp_msg[MAX_MSG_LENGTH+1];
	char msg_for_window[MAX_PRINTABLE_MSG_LENGTH+1];
	char tmp_nick1[MAX_NICK_LENGTH+1];
	char tmp_nick2[MAX_NICK_LENGTH+1];
	char tmp_buf[MAX_SOCKET_BUF];
	char tmp_chan[MAX_CHANNEL_LENGTH+1];

	if (!StrBegins(message_from_server, "CHANMSGFROM ")) {
		// we process the message from the server
		// NewLines are never sent by the server, we use %[^\n] to read whitespaces in the string too
		sscanf(message_from_server, "CHANMSGFROM %s %[^\n]", tmp_nick1, tmp_msg);
		for (i=0; i<MAX_IGNORES ; i++) {
			if (!strcmp(ignored_nicks[i],tmp_nick1)) {
				#ifdef DEBUG
				AddMsgToChatWindow("Ignored a channel message.", true);
				#endif
				return;
			}
		}
		
		// if sender wasn't on ignore, print the received message on the screen in the finalized format
		// this is where we print accepted channel messages
		if (i==MAX_IGNORES) {
			sprintf(msg_for_window, "%s %s", tmp_nick1, tmp_msg);
			AddMsgToChatWindow(msg_for_window, true);
			return;
		}
	}
	
	if (!StrBegins(message_from_server, "PRIVMSGFROM ")) {
		// we process the message from the server
		sscanf(message_from_server, "PRIVMSGFROM %s %[^\n]", tmp_nick1, tmp_msg);
		for (i=0; i<MAX_IGNORES ; i++) {
			if (!strcmp(ignored_nicks[i],tmp_nick1)) {
				#ifdef DEBUG
				AddMsgToChatWindow("Ignored a private message.", true);
				#endif
				return;
			}
		}				
		// if sender wasn't on ignore, print the received message on the screen in the finalized format
		if (i==MAX_IGNORES)	{
			sprintf(msg_for_window, "%s has sent you a private message:", tmp_nick1);
			AddMsgToChatWindow(msg_for_window, true);
			sprintf(msg_for_window, "          %s", tmp_msg);
			AddMsgToChatWindow(msg_for_window, false);
		}
	}
	
	if (!StrBegins(message_from_server, "PRIVMSGOK ")) {
		sscanf(message_from_server, "PRIVMSGOK %s %[^\n]", tmp_nick1, tmp_msg);
		sprintf(msg_for_window, "you sent a private message to %s:", tmp_nick1);
		AddMsgToChatWindow(msg_for_window, true);
		sprintf(msg_for_window, "          %s", tmp_msg);
		AddMsgToChatWindow(msg_for_window, false);					
	}				
	
	if (!StrBegins(message_from_server, "CHANGENICKOK ")) {
		sscanf(message_from_server, "CHANGENICKOK %[^\n]", tmp_nick1);
		sprintf(msg_for_window, "Your nick is now %s.", tmp_nick1);
		AddMsgToChatWindow(msg_for_window, true);
	}				
	
	if (!StrBegins(message_from_server, "CHANUPDATECHANGENICK ")) {
		sscanf(message_from_server, "CHANUPDATECHANGENICK %s %[^\n]", tmp_nick1, tmp_nick2);
		sprintf(msg_for_window, "%s is now known as %s", tmp_nick1, tmp_nick2);
		AddMsgToChatWindow(msg_for_window, true);
	}	
	
	if (!StrBegins(message_from_server, "CHANGECHANNELOK ")) {
		sscanf(message_from_server, "CHANGECHANNELOK %[^\n]", tmp_chan);
		sprintf(msg_for_window, "You are now chatting in channel %s.", tmp_chan);
		AddMsgToChatWindow(msg_for_window, true);
	}
	
	if (!StrBegins(message_from_server, "CHANUPDATEJOIN ")) {
		sscanf(message_from_server, "CHANUPDATEJOIN %[^\n]", tmp_nick1);
		sprintf(msg_for_window, "%s has joined the channel.", tmp_nick1);
		AddMsgToChatWindow(msg_for_window, true);
	}
	
	if (!StrBegins(message_from_server, "CHANUPDATELEAVE ")) {
		sscanf(message_from_server, "CHANUPDATELEAVE %[^\n]", tmp_nick1);
		sprintf(msg_for_window, "%s has left the channel.", tmp_nick1);
		AddMsgToChatWindow(msg_for_window, true);
	}
	
	if (!StrBegins(message_from_server, "CHANGEPASSOK ")) {
		sscanf(message_from_server, "CHANGEPASSOK %[^\n]", tmp_nick1);
		sprintf(msg_for_window, "Password for %s has been saved.", tmp_nick1);
		AddMsgToChatWindow(msg_for_window, true);
	}

	if (!StrBegins(message_from_server, "CHANGEPASSERROR ")) {
		sscanf(message_from_server, "CHANGEPASSERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}						
	
	if (!StrBegins(message_from_server, "CHANGECHANNELERROR ")) {
		sscanf(message_from_server, "CHANGECHANNELERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}				
	
	if (!StrBegins(message_from_server, "CHANGENICKERROR ")) {
		sscanf(message_from_server, "CHANGENICKERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}				

	if (!StrBegins(message_from_server, "CHANMSGERROR ")) {
		sscanf(message_from_server, "CHANMSGERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}
	
	if (!StrBegins(message_from_server, "PRIVMSGERROR ")) {
		sscanf(message_from_server, "PRIVMSGERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}
	
	if (!StrBegins(message_from_server, "CMDERROR ")) {
		sscanf(message_from_server, "CMDERROR %[^\n]", tmp_msg);
		AddMsgToChatWindow(tmp_msg, true);
	}
	
	if (!StrBegins(message_from_server, "CHANUPDATEALLNICKS ")) {
		sscanf(message_from_server, "CHANUPDATEALLNICKS %[^\n]", tmp_buf);
		UpdateNicklist(tmp_buf);
	}	

	
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

// adds a message to chat window
// if timestamp == TRUE, it will add timestamp, otherwise it won't
void AddMsgToChatWindow(const char* msg, int timestamped) {
	// for saving the current input window coordinates
	int saved_x, saved_y;

	// for printing time for msgs
	time_t time_now;
	char tmp_time[8];
	char msg_to_print[MAX_PRINTABLE_MSG_LENGTH+1];
	int i;
	
	// saving the current input window coordinates, to remember where the cursor was
	getyx(input_win, saved_y, saved_x);
	
	if (timestamped) {
		// get current time into time_now, and then print into tmp_time string
		time_now = time (0);
		strftime(tmp_time, 9, "%H:%M:%S", localtime (&time_now));
		sprintf(msg_to_print, "%s %s", tmp_time, msg);
	}
	else {
		sprintf(msg_to_print, "%s", msg);

	}

	// increase chat_window_buffer_last_element_index
	chat_window_buffer_last_element_index++;
	if (chat_window_buffer_last_element_index < CHAT_WINDOW_BUFFER_MAX_LINES) {
		// if we aren't past the maximum, just write the line into the current position
		strcpy(chat_window_buffer[chat_window_buffer_last_element_index], msg_to_print);
	}
	else {
		// we are now at the maximum; we need to rotate the chat_window_buffer
		for (i=0; i < CHAT_WINDOW_BUFFER_MAX_LINES-1; i++) {
			// line 0 <=== line 1, line 1 <=== line 2, etc
			bzero(chat_window_buffer[chat_window_buffer_last_element_index], MAX_MSG_LENGTH);
			strcpy(chat_window_buffer[chat_window_buffer_last_element_index], chat_window_buffer[chat_window_buffer_last_element_index+1]);
		}
		// we have rotated the chat_window_buffer
		// add the new line to the last position
		strcpy(chat_window_buffer[CHAT_WINDOW_BUFFER_MAX_LINES-1], msg_to_print);
		// reset the current buffer last element
		chat_window_buffer_last_element_index = CHAT_WINDOW_BUFFER_MAX_LINES-1;
	}
	
	// now the chat_window_buffer is updated in the memory
	// we now need to redraw the chat window contents based on the chat_window_buffer
	chat_win_currenty=1; 
	chat_win_currentx=1;	
	
	// if current line position is smaller than the window height (minus borders)
	// we can print all existing lines from line 0 on the screen
	// first line (chat_window_currently_showing_last) will be the same as before
	// chat_window_currently_showing_last is incremented by 1
	if (chat_window_buffer_last_element_index < (chat_win_height-2)) {
		chat_window_currently_showing_last = chat_window_buffer_last_element_index;
		for (i=0; i<=chat_window_buffer_last_element_index; i++) {
			// print the new line to the screen
			mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, chat_window_buffer[i]);		
			chat_win_currenty++;
		}
	}
	
	// otherwise we need to print the last (chat_win_height-2) lines from the chat_window_buffer
	else {
		// let's update the first line which should be shown from the buffer
		chat_window_currently_showing_first = chat_window_buffer_last_element_index-(chat_win_height-3);
		chat_window_currently_showing_last = chat_window_buffer_last_element_index;
		for (i = chat_window_currently_showing_first; i<=chat_window_buffer_last_element_index; i++) {
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

// scroll chat window
// direction == SCROLL_DIRECTION_UP / SCROLL_DIRECTION_DOWN
// it updates the chat_window_currently_showing_first & chat_window_currently_showing_last indexes,
// and shows this interval of the chat_window_buffer
void ScrollChatWindow(int direction) {
	int i;
	// for saving the current input window coordinates
	int saved_x, saved_y;

	// if we are showing messages from the top of the buffer already, scrolling up isn't possible
	if (direction == SCROLL_DIRECTION_UP && 0 == chat_window_currently_showing_first) {
		beep();
		return;
	}
	// if we are showing the last messages from the buffer already, scrolling down isn't possible
	if (direction == SCROLL_DIRECTION_DOWN && 
			chat_window_currently_showing_last == chat_window_buffer_last_element_index ) {
		beep();
		return;
	}
	
	// we got this far, let's do the scrolling
	
	// saving the current input window coordinates, to remember where the cursor was
	getyx(input_win, saved_y, saved_x);	
	
	// let's shift the interval that we will show from chat_window_buffer
	chat_window_currently_showing_first += direction;
	chat_window_currently_showing_last += direction;
	
	// we now need to redraw the chat window contents based on the chat_window_buffer
	
	// we will start drawing at the top of the chat window (taking the borders in consideration)
	chat_win_currenty=1; 
	chat_win_currentx=1;	

	// this segment of the buffer to be visualized:
	// chat_window_buffer[chat_window_currently_showing_first] - chat_window_buffer[chat_window_currently_showing_last]
	for (i = chat_window_currently_showing_first; i<=chat_window_currently_showing_last; i++) {
		// reset the line to make sure there won't be any junk left, if we overwrite a longer line
		mvwhline(chat_win, chat_win_currenty, chat_win_currentx, ' ', MAX_MSG_LENGTH);
		mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, chat_window_buffer[i]);
		chat_win_currenty++;			
	}
	
	// draw the borders of the chat window
	box(chat_win, 0, 0);
	// cursor should go back to where it was in the input window
	wmove(input_win, saved_y, saved_x);
	// redraw the chat & input windows
	wrefresh(chat_win);
	wrefresh(input_win);	
}

// will draw the nicklist window from the nicklist we got from the server
void UpdateNicklist(char* nicklist) {
	int i;
	
	// where to put first nick; not on the borders, (1,1) is the correct position
	int nicklist_win_currenty=1;
	int nicklist_win_currentx=1;
		
	// saving the current input window coordinates, to remember where the cursor was
	int saved_x, saved_y;
	getyx(input_win, saved_y, saved_x);
	
	// reset nick window
	for (i = 1; i<=nicklist_win_height-2; i++) {
		// reset the line to make sure there won't be any junk left, if we overwrite a longer line
		mvwhline(nicklist_win, i, 1, ' ', MAX_NICK_LENGTH);
	}	
	
	// nicklist should be tokenized, separator character is ' '
	// we print each nick in a new line of the nicklist window
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


// calculates the hash of source_string
// writes the hex representation into hash_in_hex_string
void SHA512(char *source_string, char *hash_in_hex_string) {
	EVP_MD_CTX *mdctx;
	OpenSSL_add_all_digests();
	const EVP_MD *md = EVP_get_digestbyname("sha512");
	unsigned int md_len;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	int i;
	char buf[32];

	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
	EVP_DigestUpdate(mdctx, source_string, strlen(source_string));
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);
	
	// represent the hash as a hex string
	for(i = 0; i < md_len; i++) {
		sprintf(buf, "%02x", md_value[i]);
		strcat(hash_in_hex_string, buf);
	}
}
