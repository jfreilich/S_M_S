//data_streamer calls these functions and uses these structs

/*
stores all of the data for a client
the client is part of two independent linked lists
prev/next are pointers for the entire list of clients
station_prev/station_next are pointers for the list of clients on the same station
*/
typedef struct client {
	int socket;
	int udp_sock;
	struct sockaddr_in addr;
	int station;
	struct client* prev;
	struct client* next;
	struct client* station_prev;
	struct client* station_next;
	pthread_rwlock_t lock;
}client_t;

/*
struct for data passed to stream_music udp thread
one thread per station
*/
typedef struct station_thread{
	client_t** first;
	char* songName;
	pthread_rwlock_t* station_lock;
}station_thread_t;

/*
announces the song to the specified socket
*/
void announce_song(int socket, char* songName);

/*
deletes the specified client entirely
removes it from main list and station list
terminates the client which frees its memory and destroys the lock
assumes the client is already locked
*/
void delete_client_from_lists(client_t* client);