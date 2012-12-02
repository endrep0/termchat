all: termchatserver termchatclient

termchatserver: termchatserver.c
	gcc -Wall -ggdb -lssl -o termchatserver termchatserver.c termchatcommon.c

termchatclient: termchatclient.c
	gcc -Wall -ggdb -lncurses -lssl -o termchatclient termchatclient.c termchatcommon.c

clean:
	rm -rf termchatserver termchatclient
