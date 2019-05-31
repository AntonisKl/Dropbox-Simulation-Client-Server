CC = gcc
CFLAGS  = -g -Wall
RM = rm -rf

all: createExe exe dropbox_server dropbox_client

createExe:
	[ -d exe ] || mkdir exe

dropbox_server:  server.o list.o utils.o
	$(CC) $(CFLAGS) -o exe/dropbox_server server.o list.o utils.o -pthread

dropbox_client:  client.o cyclic_buffer.o list.o utils.o
	$(CC) $(CFLAGS) -o exe/dropbox_client client.o cyclic_buffer.o list.o utils.o -pthread

server.o:  server/server.c
	$(CC) $(CFLAGS) -c server/server.c

client.o:  client/client.c
	$(CC) $(CFLAGS) -c client/client.c

cyclic_buffer.o:  cyclic_buffer/cyclic_buffer.c
	$(CC) $(CFLAGS) -c cyclic_buffer/cyclic_buffer.c

list.o:  list/list.c
	$(CC) $(CFLAGS) -c list/list.c

# file_list.o:  file_list/file_list.c
# 	$(CC) $(CFLAGS) -c file_list/file_list.c

utils.o:  utils/utils.c
	$(CC) $(CFLAGS) -c utils/utils.c
	

clean: 
	$(RM) *.o exe/*
