//this file does the following
//aggregate common importans
//define min/max
//define command types and data sizes
//declare helper functions

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>
#define MAX(X,Y) ((X) > (Y) ?  (X) : (Y))
#define MIN(X,Y) ((X) > (Y) ?  (Y) : (X))
#define MUSIC_SIZE 1024
#define HANDSHAKE_COMMAND 0
#define CLIENT_SERVER_HANDSHAKE 0
#define SONG_COMMAND 1
#define INVALID_COMMAND 2
#define SEND_MESSAGE 1
#define USER_INQUIRY 2
#define MTU_SIZE 1400
/*
helper methods to wrap perror calls
still return values for disconnecting from a client if there is an error
*/
int smartWrite(int sock, void* data, int dataSize);
int smartRead(int sock, void* data, int dataSize);
int smartRecv(int sock, void* data, int dataSize);
int smartRecvfrom(int sock, void* data, int dataSize, struct sockaddr* addr, socklen_t addrlen);
int smartSendto(int socket, void* data, int dataSize, struct sockaddr* addr, socklen_t addrlen);

typedef struct __attribute__((__packed__)) rsa_key {
	uint32_t mod;
	uint32_t exponent;
}rsa_key_t;

typedef struct connected_user {
	char* name;
	rsa_key_t public_key;
	int socket_fd;
	//address stuff?
}connected_user_t;

typedef struct __attribute__((__packed__)) command {
	uint8_t command_type;
	uint32_t num_messages;
} command_t;


//protocol is first send command
//then send a series of ints, one as the string size for each message
//first is username, then other messages

void generate_keys(rsa_key_t* public_key, rsa_key_t* private_key);
uint32_t calculate_private_exponent(int totient, int public_exponent);
uint32_t generate_public_exponent(uint32_t p, uint32_t q);
uint32_t generate_large_prime(int seed);