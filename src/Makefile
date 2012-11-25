all: termchatserver termchatclient

termchatserver: termchatserver.c
	gcc -Wall -ggdb -o termchatserver termchatserver.c

termchatclient: termchatclient.c
	gcc -Wall -ggdb -lncurses -o termchatclient termchatclient.c

clean:
	rm -rf termchatserver termchatclient
