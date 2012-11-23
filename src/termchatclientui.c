/* this project is created as a school assignment by Endre Palinkas */
/* ncurses5 has to be installed to be able to compile this */

#include <ncurses.h>

WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[])
{	WINDOW *main_win;
	int main_win_startx, main_win_starty, main_win_width, main_win_height;
	int ch;
	// start curses mode
	initscr();
	// line buffering disabled, pass on every key press
	cbreak();
	keypad(stdscr, TRUE);

	// set size for main window
	main_win_height = LINES;
	main_win_width = COLS;
	main_win_starty = 0;
	main_win_startx = 0;

	refresh();
	main_win = create_newwin(main_win_height, main_win_width, main_win_starty, main_win_startx);

	while((ch = getch()) != KEY_F(10))
	{	switch(ch)
		{	case KEY_LEFT:
				destroy_win(main_win);
				main_win = create_newwin(main_win_height, main_win_width, main_win_starty,--main_win_startx);
				break;
			case KEY_RIGHT:
				destroy_win(main_win);
				main_win = create_newwin(main_win_height, main_win_width, main_win_starty,++main_win_startx);
				break;
			case KEY_UP:
				destroy_win(main_win);
				main_win = create_newwin(main_win_height, main_win_width, --main_win_starty,main_win_startx);
				break;
			case KEY_DOWN:
				destroy_win(main_win);
				main_win = create_newwin(main_win_height, main_win_width, ++main_win_starty,main_win_startx);
				break;	
		}
	}
		
	// end curses mode
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
