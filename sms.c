#include "sms.h"
//these are all fairly obvious helper/wrapper functions

int smartWrite(int sock, void* data, int dataSize){
	int writeSize = write(sock, data, dataSize);
	if (writeSize<=0){
		char* string = (writeSize? "write" : "Connection closed write.");
		perror(string);
		if (writeSize){
			if(close(sock)<0){
				perror("socket closing");
			}
		}
	}
	else if (writeSize!=dataSize){
		return smartWrite(sock, data, dataSize);
	}
	return writeSize;
}

int smartSendto(int socket, void* data, int dataSize, struct sockaddr* addr, socklen_t addrlen){
	int sendSize = sendto(socket, data, dataSize, 0, addr, addrlen);
    if (sendSize<=0){
        char* string = (sendSize? "send" : "Connection closed sendto.");
		perror(string);
		if (sendSize){
			if(close(socket)<0){
				perror("socket closing");
			}
		}
    }
    else if (sendSize!=dataSize){
    	return smartSendto(socket, data, dataSize, addr, addrlen);
    }
    return sendSize;
}

int smartRead(int sock, void* data, int dataSize){
	int sizeRead = read(sock, data, dataSize);
	if (sizeRead<=0){
		char* string = (sizeRead? "read" : "Connection closed read.");
		perror(string);
		if (sizeRead){
			if(close(sock)<0){
				perror("socket closing");
			}
		}
	}
	else if(sizeRead !=dataSize){
		return smartRead(sock, data, dataSize);
	}
	return sizeRead;
}
int smartRecv(int sock, void* data, int dataSize){
	return smartRecvfrom(sock, data, dataSize,0,0);
}

int smartRecvfrom(int sock, void* data, int dataSize, struct sockaddr* addr, socklen_t addrlen){
	int recvSize = recvfrom(sock, data, dataSize,0,addr,&addrlen);
	if (recvSize<=0){
		char* string = (recvSize? "read" : "Connection closed recvfrom.");
		perror(string);
		if (recvSize){
			if(close(sock)<0){
				perror("socket closing");
			}
		}
	}
	else  if (recvSize !=dataSize){
//		return smartRecvfrom(sock, data, dataSize, addr, addrlen);
	}
	return recvSize;
}


//TODO: actual random crypto

void generate_keys(rsa_key_t* public_key, rsa_key_t* private_key){
	uint32_t p, q, n, totient;
	p = generate_large_prime(0);
	q = generate_large_prime(1);
	n = p*q;
	public_key->exponent = generate_public_exponent(p, q);
	public_key->mod = n;
	totient = (p-1)*(q-1);
	private_key->exponent = calculate_private_exponent(totient, public_key->exponent);
	private_key->mod = n;
}

uint32_t generate_large_prime(int seed){
	return (seed ? 13:31);
}

uint32_t generate_public_exponent(uint32_t p, uint32_t q){
	return 7;
}
//not trivial to convert this to uint do to mods
uint32_t calculate_private_exponent(int totient, int public_exponent){
	//solve private_exponent*public_exponent = 1 mod totient, i.e. totient*n+1 = d*e
	int current, divisor, r, p_prev, q_prev, p_cur, q_cur, temp;
	current = totient;
	divisor = public_exponent;
	q_prev = current / divisor;
	r = current - divisor*q_prev;
	p_prev = 0;
	current = divisor;
	divisor = r;
	q_cur = current / divisor;
	r = current - divisor*q_cur;
	p_cur = 1;
	current = divisor;
	divisor = r;
	while(r){
		temp = p_cur;
		p_cur = (((p_prev - p_cur * q_prev) % totient) + totient) %totient;
		p_prev = temp;
		q_prev = q_cur;
		q_cur = current / divisor;
		r = current - divisor*q_cur;
		current = divisor;
		divisor = r;
	}
	p_cur = (((p_prev - p_cur * q_prev) % totient) + totient) %totient;
	return p_cur;
}

