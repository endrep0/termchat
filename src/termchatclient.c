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
#define PORT "2233"


#define MAX_MSG_LENGTH 80
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[]) {
	// the UI windows, and their parameters
	WINDOW *nicklist_win;
	WINDOW *input_win;
	WINDOW *chat_win;
	struct addrinfo hints;
	struct addrinfo* res;
	int err;
	int csock;
	char buffromstdin[MAX_MSG_LENGTH];
	char buffromserver[MAX_MSG_LENGTH];
	//int lenfromstdin;
	int lenfromserver;
	int num_of_sockets_to_read;
	// sockets to give to select
	fd_set socks_to_process;
	// timeout for select
	struct timeval select_timeout;	

	int nicklist_win_startx, nicklist_win_starty, nicklist_win_width, nicklist_win_height;
	int input_win_startx, input_win_starty, input_win_width, input_win_height;
	int chat_win_startx, chat_win_starty, chat_win_width, chat_win_height, chat_win_currenty, chat_win_currentx;

	int user_input_char;
	// protocol limit	
	char user_input_str[MAX_MSG_LENGTH];
	// input limit may be reduced during runtime, if the users terminal is too small
	int curchar;
	int max_input_length = MAX_MSG_LENGTH;
	
	// saved coordinates
	int saved_x;
	int saved_y;	


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
	curchar=0;


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
	
	// main loop, let's read each user input, terminated by NL or CR
	// getnstr limits the number of chars they can type
	// getnstr should time out after 100ms, giving a chance to the main loop to read network data, even if there's no user interaction
	wtimeout(input_win, 100);
	//noqiflush();
	while(1)
	{	
		if ((user_input_char=wgetch(input_win)) != ERR) {

			// handle backspace
			if (user_input_char == KEY_BACKSPACE || user_input_char == 127) {
				if (curchar > 0) {
					curchar--;
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
				else {
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "%s", user_input_str);
					if (chat_win_currenty < chat_win_height-2) 
						chat_win_currenty++;
					// todo: else scroll
				}
				
				// reset input window with a horizontal line made of ' ' characters
				mvwhline(input_win, 1, 1, ' ', input_win_width-2);
				wmove(input_win,1,1);
				wrefresh(chat_win);
				wrefresh(input_win);
				// reset input string, because we got an NL
				bzero(user_input_str, MAX_MSG_LENGTH);
				curchar = 0;
				continue;
			}			
			
			// it is not a backspace or newline character
			// check the msg limit, don't allow user to write more
			if (curchar==MAX_MSG_LENGTH-1) {
				beep();
				continue;
			}

			// ok, not a newline, not a backspace, and limit is not reached yet
			// let's save the input character into our input string
			if (user_input_char!='\n') {
				user_input_str[curchar]=user_input_char;
				curchar++;
				wprintw(input_win, "%c", user_input_char);
			}
		}
		else {
		
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
						bzero(buffromserver, MAX_MSG_LENGTH);
						if ((lenfromserver = recv(csock, buffromserver, MAX_MSG_LENGTH, 0)) > 0) {
							getyx(input_win, saved_y, saved_x);
							
							mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "%s", buffromserver);
							if (chat_win_currenty < chat_win_height-2) 
								chat_win_currenty++;
							// todo is there an overflow here? box redraws which solves the problem
							box(chat_win, 0 , 0);
							wrefresh(chat_win);
							wmove(input_win, saved_y, saved_x);
							wrefresh(input_win);			
						}
					}
					
					// if stdin is in the FD set, we have local data to send to the chat server
					if (FD_ISSET(STDIN_FILENO, &socks_to_process)) {			
						// fill buffer with zeros
						bzero(buffromstdin, MAX_MSG_LENGTH);
						
						/* TODO
						if ((lenfromstdin = read(STDIN_FILENO, buffromstdin, sizeof(buffromstdin))) > 0 ) 
							send(csock, buffromstdin, lenfromstdin, 0);				
						*/
					}
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
	box(local_win, 0 , 0);		/* 0, 0 gives default characters 
					 * for the vertical and horizontal
					 * lines			*/
	wrefresh(local_win);		/* Show that box 		*/

	return local_win;
}

void destroy_win(WINDOW *local_win)
{	
	/* box(local_win, ' ', ' '); : This won't produce the desired
	 * result of erasing the window. It will leave it's four corners 
	 * and so an ugly remnant of window. 
	 */
	wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	/* The parameters taken are 
	 * 1. win: the window on which to operate
	 * 2. ls: character to be used for the left side of the window 
	 * 3. rs: character to be used for the right side of the window 
	 * 4. ts: character to be used for the top side of the window 
	 * 5. bs: character to be used for the bottom side of the window 
	 * 6. tl: character to be used for the top left corner of the window 
	 * 7. tr: character to be used for the top right corner of the window 
	 * 8. bl: character to be used for the bottom left corner of the window 
	 * 9. br: character to be used for the bottom right corner of the window
	 */
	wrefresh(local_win);
	delwin(local_win);
}
