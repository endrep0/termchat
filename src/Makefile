all: termchatserver termchatclient termchatclientui

termchatserver: termchatserver.c
	gcc -Wall -ggdb -o termchatserver termchatserver.c

termchatclient: termchatclient.c
	gcc -Wall -ggdb -o termchatclient termchatclient.c

termchatclientui: termchatclientui.c
	gcc -Wall -ggdb -lncurses -o termchatclientui termchatclientui.c

clean:
	rm -rf termchatserver termchatclient termchatclientui
