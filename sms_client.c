#include "sms.h"
#include <openssl/rsa.h>

int server_socket;
rsa_key_t private_key, public_key, server_public_key;
RSA public
char* username;
uint32_t name_length;
char buf[1024];
//TODO: should be a hashtaable
connected_user_t users[256];

void handler (int signum);
void handle_user_input();
void read_command(int read_socket, int write_socket);
void loop(int num_stations, int sock);
int handshake();
int setup_socket(char* servername, int serverport);
void send_messages();

int main(int argc, char **argv) {
	if (argc!=4){
		perror("Usage: servername serverport username\n");
		exit(1);
	}
	char* servername = argv[1];
	int serverport = atoi(argv[2]);
	if ((serverport = ((serverport>USHRT_MAX)? -1:serverport))<0){
		return -1;
	}
	username = argv[3];
	name_length = strlen(username);
	server_socket = setup_socket(servername, serverport);
	generate_keys(&public_key, &private_key);
	public_key.mod = htonl(public_key.mod);
	public_key.exponent = htonl(public_key.exponent);
	if (handshake()<0){
		exit(1);
	}
	send_messages();
	//signal(SIGINT, handler);
	//loop(num_stations, server_socket);
	return 0;
}
/*
setups the socket from the servername and port
connects to the address
returns the socket
*/
int setup_socket(char* servername, int serverport){ // set up socket for TCP
	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	struct hostent *hostinfo;
	//find the internet address of the server
	if ((hostinfo = gethostbyname(servername)) == 0) {
		fprintf(stderr, "Host %s does not exist\n", servername);
		exit(1);
	}
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy(&server_addr.sin_addr, hostinfo->h_addr_list[0],
		hostinfo->h_length);
	server_addr.sin_port = htons(serverport);
	// connect to the server
	if (connect(sock, (struct sockaddr *)&server_addr,
		sizeof(server_addr)) < 0) {
		perror("connect");
		exit(1);
	}
	return sock;
}

/*
performs the handshake with the socket
returns the number of stations
*/
int handshake(){
	command_t command;
	command.command_type = CLIENT_SERVER_HANDSHAKE;
	command.num_messages = htonl(2);
	if (smartWrite(server_socket, &command, sizeof(command_t))<=0){
		return -1;
	}
	int sizes[2] = {htonl(name_length), htonl(sizeof(rsa_key_t))};
	if (smartWrite(server_socket, sizes, 2*sizeof(uint32_t))<=0){
		return -1;
	}
	if (smartWrite(server_socket, username, name_length)<=0){
		return -1;
	}
	if (smartWrite(server_socket, &public_key, sizeof(rsa_key_t))<=0){
		return -1;
	}
	if (smartRead(server_socket, &command, sizeof(command_t))<=0){
		return -1;
	}
	if (ntohl(command.num_messages)==1){
		int message_size;
		if (smartRead(server_socket, &message_size, sizeof(int))<=0 || ntohl(message_size)!=sizeof(rsa_key_t)){
			return -1;
		}
		if (smartRead(server_socket, &server_public_key, sizeof(rsa_key_t))<=0){
			return -1;
		}
		server_public_key.mod = ntohl(server_public_key.mod);
		server_public_key.exponent = ntohl(server_public_key.exponent);
		return 0;
	}
	return -1;
}



/*
use select to read commands from stdin and responses from the server
*/
void send_messages(){
	fd_set read_set;
	int maxFD = 1 + MAX(MAX(MAX(STDERR_FILENO, STDOUT_FILENO), STDIN_FILENO), server_socket);
	while(1) {
		FD_ZERO(&read_set);	
		FD_SET(STDIN_FILENO, &read_set);
		FD_SET(server_socket, &read_set);
		select(maxFD, &read_set, 0, 0, 0);
		if (FD_ISSET(STDIN_FILENO, &read_set)){
			handle_user_input();
		}
		if (FD_ISSET(server_socket, &read_set)){
			//read_command(server_socket, STDOUT_FILENO);
		}
	}
}

uint32_t encrypt_message(rsa_key_t key, char* buf, uint32_t buf_len){
	//TODO: encrypt the message
	return buf_len;
}

/*
turns user input into an encrypted message and sends it
*/
void handle_user_input(){
	//turn input into encrypted username to be decrypted on server and encrypted message to be decrypted on client
	int sizeRead = read(STDIN_FILENO, buf, MTU_SIZE);
	if (sizeRead<=0){
		return;
	}
	char* dst_user = strtok(buf, " ");
	if (dst_user == NULL){
		return;
	}
	uint32_t dst_user_size = encrypt_message(server_public_key, dst_user, strlen(dst_user));
	char* message = strtok(NULL, "\n");
	if (message == NULL){
		return;
	}
	//TODO: lookup public key in hashtable
	uint32_t message_size = encrypt_message(users[0].public_key, message, strlen(message));
	command_t command;
	command.command_type = SEND_MESSAGE;
	command.num_messages = htonl(2);
	if (smartWrite(server_socket, &command, sizeof(command_t))<=0){
		return;
	}
	int sizes[2] = {htonl(dst_user_size), htonl(message_size)};
	if (smartWrite(server_socket, (void*) sizes, 2*sizeof(uint32_t))<=0){
		return;
	}
	if (smartWrite(server_socket, (void*) dst_user, dst_user_size)<=0){
		return;
	}
	if (smartWrite(server_socket, (void*) message, message_size)<=0){
		return;
	}
}



#if 0


/*
turns user input into a station
*/
int handle_user_input(int sock, int num_stations, int* sendCommand){
	int sizeRead = read(STDIN_FILENO, buf, 1024);
	if (sizeRead<=0){
		return -1;
	}
	int sendSize = encrypt_message(buf);
	command_t command;
	command.command_type = SEND_MESSAGE;
	command.num_messages = htonl(2);
	if (smartWrite(server_socket, &command, sizeof(command_t))<=0){
		return -1;
	}
	int sizes[2] = {htonl(name_length), htonl(sizeRead)};
	if (smartWrite(server_socket, sizes, 2*sizeof(uint32_t))<=0){
		return -1;
	}
	if (smartWrite(server_socket, username, name_length)<=0){
		return -1;
	}
	if (smartWrite(server_socket, (void*)buf, sizeRead)<=0){
		return -1;
	}
	return 0;
}


/*
handles SIGINT
this is also called when no data is read/written
doing so allows the server to not exit the application but for the client to exit
*/

void handler (int signum){
	printf("Quitting\n");
	if (close(server_socket)<0){
		perror("close");
	}
	exit(signum);
}

/*
use select to read commands from stdin and responses from the server
*/
void loop(int num_stations, int sock){
	fd_set read_set, write_set, except_set;
	uint16_t data = 0;
	int readFromSock = 0;
	int sendCommand = 0;
	int maxFD = 1 + MAX(MAX(MAX(STDERR_FILENO, STDOUT_FILENO),STDIN_FILENO),sock);
	while(1) {
		FD_ZERO(&write_set);FD_ZERO(&read_set);FD_ZERO(&except_set);	
		if (sendCommand){
			FD_SET(sock, &write_set);
		}
		if (readFromSock){
			FD_SET(sock, &read_set);
		}
		FD_SET(STDIN_FILENO, &read_set);
		FD_SET(STDOUT_FILENO, &write_set);
		select(maxFD, &read_set, &write_set, &except_set, 0);
		if (FD_ISSET(STDIN_FILENO, &read_set)){
			sendCommand = 0;
			data = handle_user_input(sock, num_stations, &sendCommand);
		}
		if (FD_ISSET(sock, &write_set)){
			uint8_t type = SONG_COMMAND;
			data = htons(data);
			printf("Waiting for an announce...\n");
			if(!smartWrite(sock, &type, sizeof(uint8_t))){
				handler(1);
			}
			if (!smartWrite(sock, &data, sizeof(uint16_t))){
				handler(1);
			}
			sendCommand = 0;
			readFromSock = 1;
		}
		if (FD_ISSET(sock, &read_set)){
			read_command(sock, STDOUT_FILENO);
		}
		if (FD_ISSET(STDERR_FILENO, &write_set)){
			if (data>num_stations){
				perror("Invalid station.");
				data = 0;
			}
		}
	}
}
/*
reads a command and writes its results/response
*/
void read_command(int read_socket, int write_socket){
	fflush(stdout);
	uint8_t replyType;
	if (!smartRead(read_socket, &replyType, sizeof(uint8_t))){
		handler(1);
	}
	if(replyType == SONG_COMMAND){
		printf("New song announced: ");
	}
	else{
		printf("INVALID_COMMAND_REPLY: ");
	}
	fflush(stdout);
	uint8_t replyStringSize;
	//printf("reading\n");
	if (!smartRead(read_socket, &replyStringSize, sizeof(uint8_t))){
		handler(1);
	}
	char replyString[replyStringSize+2];
	if (!smartRead(read_socket, &replyString, replyStringSize)){
		handler(1);
	}
	replyString[replyStringSize] = '\n';
	replyString[replyStringSize+1] = '\0';
	//problem?

	if (!smartWrite(write_socket, replyString, replyStringSize+2)){
		handler(1);
	}
	//printf("adadsf%s\n",replyString );
	//printf("done reading\n");
}


#endif