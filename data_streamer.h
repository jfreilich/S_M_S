#include "sms_server.h"
/*
streams data from file_name at a contstant rate bps
repeats forever if repeat is 1
first_client is a pointer to a client_t pointer
deferencing first_client gives the first client on the station associated with file_name
this actual first client is the head of a linked list of clients on the same station
this is done so that another thread can change what client first_client
which happens if that client disconnects or changes stations
the linked list allows for loading the data once into a buffer and finding each client in constant time
station_lock is used when accessing first_client
*/
int stream_data(char* file_name, client_t** first_client, pthread_rwlock_t* station_lock, double bps, int repeat);