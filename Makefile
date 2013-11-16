CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -std=c99 -D_XOPEN_SOURCE=500
LDFLAGS = -pthread -lpthread

all: sms_client sms_server

sms_client:  sms_client.c sms.c
sms_server:   sms_server.c sms.c
clean:
	rm -f sms_client sms_server
