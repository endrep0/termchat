/* this project is created as a school assignment by Endre Palinkas */
/* ncurses5 has to be installed to be able to compile this */

#include <ncurses.h>
#include <string.h>
#define MAX_MSG_LENGTH 80
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[]) {
	// the UI windows, and their parameters
	WINDOW *nicklist_win;
	WINDOW *input_win;
	WINDOW *chat_win;

	int nicklist_win_startx, nicklist_win_starty, nicklist_win_width, nicklist_win_height;
	int input_win_startx, input_win_starty, input_win_width, input_win_height;
	int chat_win_startx, chat_win_starty, chat_win_width, chat_win_height, chat_win_currenty, chat_win_currentx;

	int inputchar;
	// protocol limit	
	char inputstr[MAX_MSG_LENGTH];
	// input limit may be reduced during runtime, if the users terminal is too small
	int curchar;
	int max_input_length = MAX_MSG_LENGTH;
	
	// saved coordinates
	int saved_x;
	int saved_y;	

	// start curses mode
	initscr();
	// line buffering disabled, pass on every key press
	//cbreak();
	// also handle function keys, like F10 for exit
	//keypad(stdscr, TRUE);

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
		if ((inputchar=wgetch(input_win)) != ERR) {

			// let's see if we allow one more character
			if (curchar<MAX_MSG_LENGTH && inputchar!=(int)('\n')) {
				inputstr[curchar]=inputchar;
				curchar++;
			}
				
			if (curchar<MAX_MSG_LENGTH && inputchar==(int)('\n')) {
				if (strstr(inputstr,"/exit") || strstr(inputstr,"/quit")  )
					break;
				if (strstr(inputstr,"/help")) {
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
					mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "%s", inputstr);
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
				bzero(inputstr, MAX_MSG_LENGTH);
				curchar = 0;
			}

		}
		else {
			getyx(input_win, saved_y, saved_x);
			mvwprintw(chat_win, chat_win_currenty, chat_win_currentx, "nothing happened. old positions: %d %d", saved_y, saved_x);
			if (chat_win_currenty < chat_win_height-2) 
				chat_win_currenty++;
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
