#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "client.h"

#define SERVPORT 8999


int main(int argc, char ** argv){

	printf("Starting\n");

	if(argc < 2){
		printf("Needs more arguments\n");
		exit(2);
	}

	//generate a random port number
	int cport = rand() % (60000 + 1 - 9500) + 9500;
	
	struct sockaddr_in dest;
	struct sockaddr_in src;
	int sock, reply, fd, size=1;
	request msg;
	void * buff;

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(1);
	}

	//set up server address
	memset((void *)&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl(INADDR_ANY);
	dest.sin_port = htons(SERVPORT);

	//establish connection
	if(connect(sock, (struct sockaddr *)&dest, sizeof(dest)) == -1){
		perror("connect");
		exit(1);
	}

	//set up client's addr
	memset((void *)&src, 0, sizeof(src));
	src.sin_family = AF_INET;
	src.sin_addr.s_addr = htonl(INADDR_ANY);
	src.sin_port = htons(cport);	

	//prepare request
	memset((void *)&msg, 0, sizeof(msg));
	msg.request_type = RETRIEVE;
	msg.client_addr = src;

	//send request
	if(write(sock, &msg, sizeof(msg)) == -1){
		perror("write");
		exit(1);
	}

		//wait for response
	reply=0;
	if(read(sock, &reply, sizeof(reply)) == -1){
		perror("read");
		exit(1);
	}

	printf("reply: %d\n", reply);

	//close connection
	shutdown(sock, 2);
	close(sock);

	return EXIT_SUCCESS;

	printf("Request sent\n");
}
