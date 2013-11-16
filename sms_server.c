#include "sms.h"
#include "data_streamer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
fd_set fd_list;
char** song_names;
socklen_t addr_len = sizeof(struct sockaddr_in);
//TODO: should be a hashtaable
connected_user_t users[256];
char buf[MTU_SIZE];
rsa_key_t private_key, public_key;


void init_global_vars(char** argv);
int init_connection(char* port_string, int socktype, struct sockaddr* addr);
void connect_to_clients(int socket_fd);
int new_client(int socket_fd);
void set_client_defaults(client_t* client);
void process_command(int socket);
int handshake(int socket_fd);
int add_udp_to_client(client_t* client, uint16_t udp_port);
void process_input();
void cleanup_and_exit();
void send_command(int socket, char* message, uint8_t type);
client_t* find_client(client_t* current,int socket);
void set_station(int station, client_t* client);
void announce_station(int socket, int station);
void* stream_music(void* arg);
void send_invalid_command(int socket, char* message);
void add_client_to_station(client_t* client, int newstation);
void add_client(client_t* client, client_t** first, client_t** last);
void move_client_from_station(client_t* client, int oldstation, int newstation);
void move_client(client_t* client, client_t** oldfirst, client_t** oldlast, client_t** newfirst, client_t** newlast);
void terminate_client(client_t* client);
void remove_client(client_t* client);
void remove_client_from_list(client_t* client);
void remove_client_from_station(client_t* client, client_t** first, client_t** last);



int main(int argc, char **argv) {
    if (argc!=2){
        perror("Usage: tcpport");
        exit(1);
    }
    char* tcp_string = argv[1];
   // init_global_vars(argv);
    generate_keys(&public_key, &private_key);
    public_key.mod = htonl(public_key.mod);
    public_key.exponent = htonl(public_key.exponent);
    int socket_fd = init_connection(tcp_string, SOCK_STREAM, 0);
    if (listen(socket_fd, 10) < 0) {
        perror("listen failure");
        exit(1);
    }
    connect_to_clients(socket_fd);
   // cleanup_and_exit();
    return 0;
}
#if 0
/*
malloc the global variables with dynamic size
start the UDP streaming threads for each station
store the threads
*/
void init_global_vars(char** argv){
    song_names = (char**) malloc(num_stations*sizeof(char*));
    station_threads = (pthread_t**) malloc(num_stations*sizeof(pthread_t*));
    station_locks = (pthread_rwlock_t**) malloc(num_stations*(sizeof(pthread_rwlock_t*)));
    station_first_clients = (client_t**) malloc(num_stations*sizeof(client_t*));
    station_last_clients = (client_t**) malloc(num_stations*sizeof(client_t*));
    int i;

    for (i=0;i<num_stations;i++){
        station_thread_t* station_thread = (station_thread_t*) malloc(sizeof(station_thread_t));
        song_names[i] = argv[i+2];
        pthread_t* thread = (pthread_t*) malloc(sizeof(pthread_t));
        station_thread->songName = argv[i+2];
        station_thread->first = &(station_first_clients[i]);
        station_thread->station_lock = (pthread_rwlock_t*) malloc(sizeof(pthread_rwlock_t));
        if (pthread_rwlock_init(station_thread->station_lock,0)<0){
            perror("station rwlock create error");
            exit(1);
        }
        station_locks[i] = station_thread->station_lock;
        int err = pthread_create(thread,0,stream_music,(void*) station_thread);
        if (err<0){
            perror("thread create error");
            exit(1);
        }
        station_threads[i] = thread;
    }
}
#endif

/*
inits the connection to the given port string with socktype
returns the file descriptor for the socket
addr is used to copy the address info if requested
*/
int init_connection(char* port_string, int socktype, struct sockaddr* addr){
    int rv, socket_fd;
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = socktype;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port_string, &hints, &servinfo)) != 0) {
        fprintf(stderr, "\n getaddrinfo error: %s", gai_strerror(rv));
        return -1;
    }
    if ((socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) < 0) {
        perror("\n Failed to create socket");
        return -1;
    }
    if (addr){
        memcpy(addr, servinfo->ai_addr, sizeof(struct sockaddr));
    }
    int yes = 1;
    if (setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    } 
    if (bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("server error: bind failure.\n");
        close(socket_fd);
        return -1;
    }
    freeaddrinfo(servinfo);
    return socket_fd;
}

/*
uses select to connect to clients and read their commands
also reads from stdin for direct server commands
*/
void connect_to_clients(int socket_fd){
    fd_set fd_list_temp;
    int fd_max, new_socket;
    FD_ZERO(&fd_list);
    FD_ZERO(&fd_list_temp);
    fd_max = socket_fd;
    FD_SET(socket_fd, &fd_list);
    FD_SET(STDIN_FILENO, &fd_list);
    while (1) {
        fd_list_temp = fd_list;
        if (select(fd_max + 1, &fd_list_temp, NULL, NULL, NULL) == -1) {
            perror("error on select call");
            return;
        }
        int socket;
        for (socket = 0; socket <= fd_max; socket++) {
            if (FD_ISSET(socket, &fd_list_temp)) {
                if (socket == STDIN_FILENO){
                    //process_input();
                }
                else if (socket == socket_fd) { //new connection
                    new_socket = new_client(socket_fd);
                    if (new_socket>0){
                        FD_SET(new_socket, &fd_list);
                        fd_max = MAX(fd_max, new_socket);
                    }
                }
                else { // recv from client
                    process_command(socket);
                }
            }
            fflush(stdout);
        }
    }
}

/*
Creates a new client by reading from the given file descriptor
returns the TCP socket for the new client
*/
int new_client(int socket_fd){
    struct sockaddr_in remote_addr;
    int new_socket = accept(socket_fd, (struct sockaddr*)&remote_addr, &addr_len);
    if (new_socket < 0) {
        perror("error on accept call");
        return -1;
    }
    else {
        printf("session id %d: new client connected; expecting user info\n", new_socket);
        return handshake(new_socket);
    }
}

int handshake(int socket_fd){
    printf("handshake\n");
    command_t read_command;
    if (smartRead(socket_fd, &read_command, sizeof(command_t))<=0){
        return -1;
    }
    if (read_command.command_type != CLIENT_SERVER_HANDSHAKE || ntohl(read_command.num_messages) != 2){
        return -1;
    }
    int sizes[2];
    if (smartRead(socket_fd, (void*) sizes, 2*sizeof(uint32_t))<=0){
        return -1;
    }
    int name_length = ntohl(sizes[0]);
    char* username = (char*) malloc(name_length);
    if (smartRead(socket_fd, (void*) username, name_length)<=0){
        return -1;
    }
    connected_user_t connected_user;
    if (smartRead(socket_fd,(void*) &(connected_user.public_key), sizeof(rsa_key_t))<=0){
        return -1;
    }
    connected_user.public_key.mod = ntohl(connected_user.public_key.mod);
    connected_user.public_key.exponent = ntohl(connected_user.public_key.exponent);
    connected_user.name = username;
    connected_user.socket_fd = socket_fd;
    read_command.num_messages = htonl(1);
    if (smartWrite(socket_fd, &read_command, sizeof(read_command))<=0){
        return -1;
    }
    int message_size = htonl(sizeof(rsa_key_t));
    if (smartWrite(socket_fd, &message_size, sizeof(int))<=0){
        return -1;
    }
    if (smartWrite(socket_fd,(void*) &(public_key), sizeof(rsa_key_t))<=0){
        return -1;
    }
    printf("user: %s connected\n", connected_user.name);
    users[socket_fd] = connected_user;
    return socket_fd;
}

void remove_user(int socket_fd){
    FD_CLR(socket_fd, &fd_list);
    connected_user_t user = users[socket_fd];
    free(user.name);
    connected_user_t empty_user;
    users[socket_fd] = empty_user;
}

/*
processes a user command to determine a response
sends a response if the command is valid
kills the client if the command is invalid
*/
void process_command(int socket_fd){
    command_t read_command;
    if (smartRead(socket_fd, &read_command, sizeof(command_t))<=0){
        remove_user(socket_fd);
        return;
    }
    uint32_t num_messages = ntohl(read_command.num_messages);
    if (num_messages){
        uint32_t message_sizes[num_messages];
        uint32_t message_size, i;
        switch (read_command.command_type){
            case SEND_MESSAGE:
                //get the size of each message
                if (smartRead(socket_fd, (void*) message_sizes, num_messages*sizeof(uint32_t))<=0){
                    remove_user(socket_fd);
                    return;
                }
                for (i=0;i<num_messages;i++){
                    message_size = ntohl(message_sizes[i]);
                    if (smartRead(socket_fd, (void*) buf, message_size)<=0){
                        remove_user(socket_fd);
                        return;
                    }
                    //decrypt buf and print
                    buf[message_size]='\0';
                    printf("%s\n",buf );
                }
                // if (smartRead(socket_fd, &message, message_len)<=0){
                //     remove_user(socket_fd);
                //     return;
                // }
                // //TODO: lookup destination with hashtable and send it
               // printf("message: %s\n",message.message);
                break;
            case USER_INQUIRY:
                break;
            case CLIENT_SERVER_HANDSHAKE:
                remove_user(socket_fd);
                break;
            default:
                remove_user(socket_fd);
                break;
        }
    }
}

#if 0
/*
gets the udp_port from the client via the hello command
sends the number of stations via the welcome command
assumes client is locked, returns locked
*/
int handshake(client_t* client){
    int sock = client->socket;
    uint16_t udp_port = 0;
    if (smartRead(sock, &udp_port, sizeof(uint16_t))<0){
        return -1;
    }
    udp_port = ntohs(udp_port);
    printf("session id %d: HELLO received; sending WELCOME, expecting SET_STATION\n", sock);
    uint8_t replyType = HANDSHAKE_COMMAND;
    smartWrite(sock, &replyType, sizeof(uint8_t));
    int net_num_stations = htons(num_stations);
    smartWrite(sock, &net_num_stations, sizeof(uint16_t));
    return add_udp_to_client(client, udp_port);
}

/*
assumes the client has a lock and returns the client still locked
adds the udp socket and address to the client via the handshake command
*/
int add_udp_to_client(client_t* client, uint16_t udp_port){
    int udp_sock = socket(PF_INET,SOCK_DGRAM,0);
    client->udp_sock = udp_sock;
    client->addr.sin_port = htons(udp_port);
    if (udp_sock<0){
        perror("UDP");
        return -1;
    }
    return 0;
}

/*
processes terminal input
will print the stations and the clients if given a print command
will exit the program if given a quit command
*/
void process_input(){
    char buf[1024];
    int sizeRead = read(STDIN_FILENO, buf, 1024);
    if (sizeRead<0){
        cleanup_and_exit(first);
    }
    if (!strncmp(buf, "q\n",2)||!strncmp(buf, "quit\n",5)){
        cleanup_and_exit(first);
    }
    else if (!strncmp(buf, "\n",1)||!strncmp(buf, "",1)){
        //do nothing
    }
    else if (!strncmp(buf, "p\n",1)){
        int stations = num_stations;
        int i;
        for (i=0;i<stations;i++){
            printf("Station %d playing \"%s\", listening:", i, song_names[i]);
            pthread_rwlock_rdlock(station_locks[i]);
            client_t* client = station_first_clients[i];
            pthread_rwlock_unlock(station_locks[i]);
            while(client){
                pthread_rwlock_rdlock(&(client->lock));
                printf(" %d",ntohs(client->addr.sin_port));
                //printf("%d\n",ntohl(client->addr.sin_addr.s_addr) );
                pthread_rwlock_unlock(&(client->lock));
                client = client->station_next;
            }
            printf("\n");
        }
    }
}

/*
frees all malloc'd memory
closes all sockets
destroys all locks
assumes no locks are already held
exits the program
*/
void cleanup_and_exit(){
    int i;
    for (i=0;i<num_stations;i++){
        pthread_t* thread = station_threads[i];
        if(pthread_kill(*thread, SIGINT)){
            perror("kill thread");
        }
    }
    client_t* client = first;
    client_t* next;
    while(client){
        pthread_rwlock_wrlock(&(client->lock));
        next = client->next;
        terminate_client(client);
        client = next;
    }
    free(song_names);
    int stations = num_stations;
    for (i=0;i<stations;i++){
        free(station_locks[i]);
        free(station_threads[i]);
    }
    free(station_locks);
    free(station_threads);
    free(station_last_clients);
    free(station_first_clients);
    exit(0);
}

//client_t* find_client(client_t* current,int socket){return(current?((current->socket==socket)?current:find_client(current->next,socket)):0);}

/*
find the client specified by the tcp socket
assumes current is already locks
the client return is also locked
*/
client_t* find_client(client_t* current,int socket){
    if (current){
        if (current->socket==socket){
            return current;
        }
        else{
            client_t* next = current->next;
            pthread_rwlock_rdlock(&(next->lock));
            pthread_rwlock_unlock(&(current->lock));
            return find_client(next,socket);
        }
    }
    return 0;
}

/*
deletes the specified client entirely
removes it from main list and station list
terminates the client which frees its memory and destroys the lock
assumes the client is already locked
*/
void delete_client_from_lists(client_t* client){
    printf("session id %d: client closed connection\n",client->socket);
    remove_client(client);
    FD_CLR(client->socket, &fd_list);
    terminate_client(client);
}

/*
closes the tcp and udp sockets
destroys the lock
frees the memory
assumes the client is already locked
*/
void terminate_client(client_t* client){
    if (close(client->socket)<0){
        perror("close tcp");
        exit(1);
    }
    if (client->udp_sock && (close(client->udp_sock)<0)){
        perror("close tcp");
        exit(1);
    }
    pthread_rwlock_unlock(&(client->lock));
    pthread_rwlock_destroy(&(client->lock));
    free(client);
}

/*
removes the client from main list and station list
assumes the client is already locked
the client remains locked at return
locks and unlocks the pointer to the first client pointer on the client's station
*/
void remove_client(client_t* client){
    remove_client_from_list(client);
    int station;
    if ((station = client->station)>=0){
        pthread_rwlock_unlock(&(client->lock));
        while (1){
            pthread_rwlock_wrlock(&(client->lock));
            if (!pthread_rwlock_trywrlock(station_locks[station])){
                break;
            }
            pthread_rwlock_unlock(&(client->lock));
        }
        remove_client_from_station(client, &(station_first_clients[station]), &(station_last_clients[station]));
        pthread_rwlock_unlock(station_locks[station]);
    }
}

/*
removes the client from the list of clients
assumes the client is already locked and the client remains locked upon return
*/
void remove_client_from_list(client_t* client){
    if(client->prev){
        pthread_rwlock_wrlock(&(client->prev->lock));
    }
    if (client->next){
        pthread_rwlock_wrlock(&(client->next->lock));
    }
    if (client != first && client != last){
        (client->prev)->next = client->next;
        (client->next)->prev = client->prev;
    }
    else{
        if (client==first){
            first = client->next;
            if(first){
                first->prev = 0;
            }
        }
        if (client==last){
            last = client->prev;
            if(last){
                last->next = 0;
            }
        }
    }
    if(client->prev){
        pthread_rwlock_unlock(&(client->prev->lock));
    }
    if (client->next){
        pthread_rwlock_unlock(&(client->next->lock));
    }
    client->prev = 0;
    client->next = 0;
}

/*
removes the client from the list of clients on its station
assumes the client is locked and returns with the client still locked
assumes the first and last client_t** are already locked and they return locked
*/
void remove_client_from_station(client_t* client, client_t** first, client_t** last){
    if (client != *first && client != *last){
        pthread_rwlock_unlock(&(client->lock));
        while(1){
            pthread_rwlock_wrlock(&(client->lock));
            if(!pthread_rwlock_trywrlock(&(client->station_prev->lock))){
                if(!pthread_rwlock_trywrlock(&(client->station_next->lock))){
                    break;
                }
                pthread_rwlock_unlock(&(client->station_prev->lock));
            }
            pthread_rwlock_unlock(&(client->lock));
        }
        (client->station_prev)->station_next = client->station_next;
        (client->station_next)->station_prev = client->station_prev;
        pthread_rwlock_unlock(&(client->station_prev->lock));
        pthread_rwlock_unlock(&(client->station_next->lock));
    }
    else{
        if (client==*first){
            *first = client->station_next;
            if(*first){
                (*first)->station_prev = 0;
            }
        }
        if (client==*last){
            *last = client->station_prev;
            if(*last){
                (*last)->station_next = 0;
            }
        }
    }
    client->station_prev = 0;
    client->station_next = 0;
}

/*
adds the client to the list of clients on the specified station
assumes the cient is locked and the client remains locked upon return
assumes the first and last client_t** are locked for station and they return locked
*/
void add_client_to_station(client_t* client, int station){
    add_client(client, &(station_first_clients[station]), &(station_last_clients[station]));
}

/*
adds the client to the list of clients on the specified station by first and last
assumes the cient is locked and the client remains locked upon return
assumes the first and last client_t** are locked and they return locked
*/
void add_client(client_t* client, client_t** first, client_t** last){
    if (*last){
        pthread_rwlock_unlock(&(client->lock));
        while(1){
            pthread_rwlock_wrlock(&(client->lock));
            if(!pthread_rwlock_trywrlock(&((*last)->lock))){
                break;
            }
            pthread_rwlock_unlock(&(client->lock));
        }
        client->station_prev = *last;
        (*last)->station_next = client;
        pthread_rwlock_unlock(&((*last)->lock));
        *last = client;
    }
    else{
        *last = client;
        *first = client;
    }
}

/*
moves the client to the list of clients on newstation from oldstation
assumes the cient is locked and the client remains locked upon return
assumes the first and last client_t** for oldstation and newstation are locked and they remain locked
*/
void move_client_from_station(client_t* client, int oldstation, int newstation){
    move_client(client, &(station_first_clients[oldstation]),&(station_last_clients[oldstation]),&(station_first_clients[newstation]),&(station_last_clients[newstation]));
}

/*
moves the client to the list of clients on the specified station by newfirst and newlast
assumes the cient is locked and the client remains locked upon return
*/
void move_client(client_t* client, client_t** oldfirst, client_t** oldlast, client_t** newfirst, client_t** newlast){
    remove_client_from_station(client, oldfirst, oldlast);
    add_client(client, newfirst, newlast);
}

/*
sets the client's station to be the given station
announces the station tot he client
adds the client to the list of clients on that station
assumes the client is locked and the client remains locked at return
*/
void set_station(int station, client_t* client){
    printf("session id %d: received SET_STATION to station %d\n", client->socket, station);
    announce_station(client->socket, station);
    //open file then play music
    if (client->station==-1){
        pthread_rwlock_wrlock(station_locks[station]);
        add_client_to_station(client, station);
        pthread_rwlock_unlock(station_locks[station]);
    }
    else if (station!=client->station){
        while (1){
            if (! pthread_rwlock_trywrlock(station_locks[station])){
                if (! pthread_rwlock_trywrlock(station_locks[client->station])){
                    break;
                }
                pthread_rwlock_unlock(station_locks[station]);
            }
        }
        move_client_from_station(client, client->station, station);
        pthread_rwlock_unlock(station_locks[station]);
        pthread_rwlock_unlock(station_locks[client->station]);
    }
    client->station = station;
}

/*
announces the song/station to the specified socket
*/
void announce_station(int socket, int station){
    char* songName = song_names[station];
    send_command(socket, songName, SONG_COMMAND);
}

/*
announces the song to the specified socket
*/
void announce_song(int socket, char* songName){
    send_command(socket, songName, SONG_COMMAND);
}

/*
sends the invalid command specified by message to the specified socket
*/
void send_invalid_command(int socket, char* message){
    send_command(socket, message, INVALID_COMMAND);
}

/*
sends a message as a command with the given type to the specified socket
*/
void send_command(int socket, char* message, uint8_t type){
    uint8_t stringSize = strlen(message);
    smartWrite(socket, &type, sizeof(uint8_t));
    smartWrite(socket, &stringSize, sizeof(uint8_t));
    smartWrite(socket, message, stringSize);
}

/*
streams music on a new thread with the arguments specified in arg
arg is a station_thread_t
*/
void* stream_music(void* arg){
    station_thread_t* station_thread = (station_thread_t*) arg;
    client_t** first = station_thread->first;
    char* song_name = station_thread->songName;
    pthread_rwlock_t* station_lock = station_thread->station_lock;
    double bps = (double) BPS;
    free(station_thread);
    stream_data(song_name, first, station_lock, bps, 1);
    return 0;
}

#endif