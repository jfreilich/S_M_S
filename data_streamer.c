#include "sms.h"
#include "data_streamer.h"
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#define BUFS_PER_READ 2
#define SEND_SIZE 1024


/*
streams the data in buf with size send_size to the list of clients
described by station_next starting with client
assumes the client is unlocked and returns the final client unlocked
*/
void stream_data_to_clients(client_t* client, char* buf, int send_size, socklen_t socklen);

/*
causes the calling thread to sleep for the specified number of seconds from tv1
*/
void thread_sleep(double seconds, struct timeval tv1);

int stream_data(char* file_name, client_t** first_client, pthread_rwlock_t* station_lock, double bps, int repeat){
    pthread_rwlock_rdlock(station_lock);
    int songfd = open(file_name, O_RDONLY);
    pthread_rwlock_unlock(station_lock);
    int size = SEND_SIZE*BUFS_PER_READ;
    char buf[size];
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct timeval tv1;
    client_t* client = 0;
    int readSize, offset, i, size_left, send_size;
    while(1){
        memset(buf,0, size);
        readSize = size;
        while(readSize == size){
            gettimeofday(&tv1, 0);
            readSize = read(songfd, buf, size);
            offset = 0;
            while(offset<readSize){
                for (i=0;i<BUFS_PER_READ;i++){
                    pthread_rwlock_rdlock(station_lock);
                    client = *first_client;
                    pthread_rwlock_unlock(station_lock);
                    if (client){
                        size_left = readSize - offset;
                        if (size_left <= 0){
                            break;
                        }
                        send_size = MIN(size_left, SEND_SIZE);
                        stream_data_to_clients(client, buf+offset, send_size, socklen);
                    }
                    offset+=MUSIC_SIZE;
                }    
            }
            double seconds = readSize/bps;
            thread_sleep(seconds, tv1);
        }
        if(repeat){
	        lseek(songfd, 0, SEEK_SET);
	        client = *first_client;
	        while (client){
                pthread_rwlock_rdlock(&(client->lock));
                pthread_rwlock_rdlock(station_lock);
	            announce_song(client->socket, file_name);
                pthread_rwlock_unlock(station_lock);
	            client_t* temp = client->station_next;
                pthread_rwlock_unlock(&(client->lock));
                client = temp;
	        }
	    }
	    else{
	    	return 1;
	    }
    }
    return 1;
}

void stream_data_to_clients(client_t* client,char* buf, int send_size, socklen_t socklen){
    while (client){
        pthread_rwlock_rdlock(&(client->lock));
        struct sockaddr* addr = (struct sockaddr*) &(client->addr);
        int size_sent = smartSendto(client->udp_sock, buf, send_size, addr, socklen);
        client_t* temp = client->station_next;
        if (size_sent<0){
            delete_client_from_lists(client);
        }
        else{
            pthread_rwlock_unlock(&(client->lock));
        }
        client = temp;                      
    }
}

void thread_sleep(double seconds, struct timeval tv1){
    struct timeval tv2;
    struct timespec req;
    gettimeofday(&tv2, 0);
    int microdiff = 1000000*(tv2.tv_sec - tv1.tv_sec) + tv2.tv_usec - tv1.tv_usec;
    long total = (1000000*seconds - (microdiff))*1000;
    req.tv_sec = total/1000000000;
    req.tv_nsec = total % 1000000000;
    nanosleep(&req, 0);
}
