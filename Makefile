CC = gcc
CFLAGS  = -g -Wall
RM = rm -rf

all: dropbox_server dropbox_client

dropbox_server:  server.o list.o utils.o
	$(CC) $(CFLAGS) -o exe/dropbox_server server.o list.o utils.o

dropbox_client:  client.o list.o utils.o
	$(CC) $(CFLAGS) -o exe/dropbox_client client.o list.o utils.o

server.o:  server/server.c
	$(CC) $(CFLAGS) -c server/server.c

client.o:  client/client.c
	$(CC) $(CFLAGS) -c client/client.c

list.o:  list/list.c
	$(CC) $(CFLAGS) -c list/list.c

# file_list.o:  file_list/file_list.c
# 	$(CC) $(CFLAGS) -c file_list/file_list.c

utils.o:  utils/utils.c
	$(CC) $(CFLAGS) -c utils/utils.c
	

clean: 
	$(RM) *.o exe/*