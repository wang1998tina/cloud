#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "server.h"

#define PORT 8999

int n_nodes, job_index, ticket_ct, free_n_size;

node * free_nodes;
sem_t sem_free;
sem_t sem_running;
job * running_jobs;

sigset_t proc;

/*Communication thread */
void * process_req(void * s){ //comm thread
	int sock = *((int *)s);
	request buffer;
	int size=1024;
	void * content;

	//empty data from buffer
	memset((void*)&buffer, 0, sizeof(buffer));

	printf("Comm starting\n");
	//read request from client
	if(read(sock, &buffer, sizeof(buffer)) < 0){
		perror("read");
		exit(1);
	}

	if(buffer.request_type == DEPLOY){

		//create job object and update list of jobs
		job new_job;

		sem_wait(sem_running);
		running_jobs[job_index] = new_job;
		running_jobs[job_index].ticket_num = ticket_ct;

		if(buffer.replicas <= free_n_size){
			int j;
			for(j=0; j<buffer.replicas; j++){
				buffer.assoc_nodes[j] = free_n[j];
			}
		} else{
			//queue the job
		}

		job_index++;
		sem_post(sem_running);

		ticket_ct++;
	}

	int ssock = free_nodes[0].sock;

	//write buffer to side node
	if(write(ssock, &buffer, sizeof(buffer)) == -1){
		perror("write");
		exit(2);
	}

	//read zip file into buffer and send to side	
	content = malloc(1024);
	while(size == 1024){
		if((size = read(sock, content, 1024)) < 0){
			perror("read");
			exit(1);
		}
		if(write(ssock, content, size) == -1){
			perror("write");
			exit(2);
		}
	}

	//send a reponse to the client
	int status = 1;
	if(write(sock, &status, sizeof(status)) == -1){
		perror("write");
		exit(2);
	}

	//destruction
	shutdown(sock,2);
	close(sock);
	pthread_exit(0);
}

/* Side node */
void * process_node(void * arg){ //side node. arg = port#
	int port = *((int*)arg);
	int sc, scom, fd, fd1, size=1024;
	struct sockaddr_in sin;
	request req;
	void * content;
	pid_t pid;
	int pipefd[2];

	//create directory to put local work
	char dir_name[15];
	sprintf(dir_name, "%s%i", "output_", port);
	if(mkdir(dir_name, 0700) == -1){
		perror("mkdir");
		exit(1);
	}

	//set up socket to receive data
	if((sc = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(1);
	}
	//set up address
	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = htonl(INADDR_ANY);	
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	//allows another socket to bind to the same addr+port immediately
	int reuse = 1;
	setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	//bind, listen, accept
	if(bind(sc, (struct sockaddr*)&sin, sizeof(sin)) <0){
		perror("bind");
		exit(1);
	}
	listen(sc, 3);

	//maintains open line with main
	if((scom = accept(sc, 0,0)) == -1){
		perror("accept");
		exit(2);
	}

	/*Loop*/
	while(1){
	
		//empty data from buffer
		memset((void*)&req, 0, sizeof(req));

		//receive request data
		if(read(scom, &req, sizeof(req)) < 0){
			perror("read");
			exit(1);
		}
		printf("Side: received request\n");

		if(req.request_type == DEPLOY){

				//open file for writing
				char path_file[30];
				sprintf(path_file, "%s/client.tar.gz", dir_name); //create zip file in dir

				if((fd = open(path_file, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0){
					perror("open");
					exit(1);
				}
		
				//receive zip file + write
				content = malloc(1024);
				while(size == 1024){
					printf("Side: writing\n");
					if((size = read(scom, content, 1024)) < 0){
						perror("read");
						exit(1);
					}
					if(write(fd, content, size) == -1){
						perror("write");
						exit(2);
					}
				}
				printf("Side: received zip\n");

				//unzip file
				if((pid = fork()) == 0){
					//extract file, save in output dir
					execlp("tar", "tar", "xvzf", path_file, "-C", dir_name, (char*)NULL);
					perror("exec");
					exit(2);
				} else{
					waitpid(pid,0,0);
				}

				pipe(pipefd); //create a pipe

				printf("Starting exec\n");

				if((pid = fork()) == 0){

					close(pipefd[0]); //close reading end in child

					dup2(pipefd[1], 1); //send stdout to pipe
					dup2(pipefd[1], 2); //send stderr to pipe

					close(pipefd[1]);
			
					//makefile is in output folder - run it there
					execlp("make", "make", "run", "-C", dir_name, (char*)NULL);
					perror("exec");
					exit(2);

				} else{ //parent
			
					char buff[1024];
					int size_b=1;
					close(pipefd[1]); //close writing end

					waitpid(pid,0,0);

					//create an output file
					char port_s[6];
					char file_name[15];
					sprintf(port_s, "%i", port);
					sprintf(file_name, dir_name);
					strcat(file_name, "/output_");
					strcat(file_name, port_s);
					strcat(file_name, ".txt");
					printf("file: %s\n", file_name);

					if((fd1 = open(file_name, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0){
						perror("open");
						exit(1);
					}

					while(size_b > 0){
						//read from pipe
						if((size_b = read(pipefd[0], buff, 1024)) < 0){
							perror("read");
							exit(1);
						}
						//output to file
						if(write(fd1, buff, 1024) == -1){
							perror("write");
							exit(2);
						}
					}
				}

		} else if(req.request_type == RETRIEVE){
			//Handle
		}
	}

}


int main(int argc, char ** argv){

	printf("Starting\n");

	create_side_nodes();

	//helper method
	field_setup();

	//upon ctrl+c, cleanup initiated
	handle_sig();
	
	struct sockaddr_in sin;
	struct sockaddr_in cin;
	socklen_t cin_len = (socklen_t)sizeof(cin);
	int sc, scom,i;
	pthread_t ctid;


	//create socket
	if((sc = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(1);
	}

	//set up address
	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = htonl(INADDR_ANY);	
	sin.sin_port = htons(PORT);
	sin.sin_family = AF_INET;

	//allows another socket to bind to the same addr+port immediately
	int reuse = 1;
	setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	//bind, listen, accept from client
	if(bind(sc, (struct sockaddr*)&sin, sizeof(sin)) <0){
		perror("bind");
		exit(1);
	}
	listen(sc, 10);

	//main loop
	while(1){
		if((scom = accept(sc, (struct sockaddr*)&cin, &cin_len)) == -1){
			perror("accept");
			exit(2);
		}

		//create a communication thread
		int* tscom = (int*) malloc(sizeof(int));
		*tscom = scom;
		pthread_create(&ctid, 0, process_req, tscom);

	}

	//join all threads
	pthread_join(ctid,0);
	for(i=0; i<n_nodes;i++){
		printf("0\n");
		pthread_join(free_nodes[i].tid,0);
	}

	//destruction
	close(sc);


	return EXIT_SUCCESS;
}

/*HELPER METHODS*/

void create_side_nodes(){

	printf("Creating side nodes\n");

	FILE * fp;
	char * line = NULL;
	size_t len;

	//open cloud_nodes
	if((fp = fopen("cloud_nodes.txt", "r")) == NULL){
		perror("open");
		exit(1);
	}
	//get # of nodes
	getline(&line, &len, fp);

	n_nodes = atoi(line);
	free_n_size = n_nodes;

	if((free_nodes = (node *)malloc(n_nodes * sizeof(node))) == NULL){
		perror("malloc");
		exit(2);
	}
	
	
	//create side nodes
	int i;
	for(i=0; i<n_nodes; i++){
		//read from file
		getline(&line, &len, fp);
		int cport = atoi(line);

		//create and save node
		node cur;
		free_nodes[i] = cur;
		//create side thread
		pthread_t ntid;
		int * n_port = (int *)malloc(sizeof(int));
		*n_port = cport;
		pthread_create(&ntid, 0, process_node, n_port);

		//save info
		free_nodes[i].tid = ntid;
		free_nodes[i].sock = cport; //save temporarily there
		
	}

	//for some reason i need to give the side nodes a little head start
	sleep(1);

	for(i=0; i<n_nodes; i++){
		struct sockaddr_in dest;
		int sock;
		if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("socket");
			exit(1);
		}
		//set up side node address
		memset((void *)&dest, 0, sizeof(dest));
		dest.sin_family = AF_INET;
		dest.sin_addr.s_addr = htonl(INADDR_ANY);
		dest.sin_port = htons(free_nodes[i].sock);
		//establish connection
		if(connect(sock, (struct sockaddr *)&dest, sizeof(dest)) == -1){
			perror("connect");
			exit(1);
		}
		//save this
		free_nodes[i].sock = sock;
	}

	fclose(fp);
	
}

//handles creation of data structures used by server
void field_setup(){

	//create list of jobs
	if((running_jobs = (job *)malloc(n_nodes * sizeof(job))) == NULL){
		perror("malloc");
		exit(2);
	}

	job_index = 0;
	ticket_ct = 0;

	//initialize semaphores
	if((sem_free = sem_open("mysem_1", O_CREAT | O_EXCL | O_RDWR, 0666, 1)) == SEM_FAILED){
		perror("sem_open");
	}
	if((sem_running = sem_open("mysem_2", O_CREAT | O_EXCL | O_RDWR, 0666, 1)) == SEM_FAILED){
		perror("sem_open");
	}
}

/*Upon SIGINT, cleanup will be initiated*/
void handle_sig(){
	sigfillset(&proc);
	sigdelset(&proc, SIGINT);
	struct sigaction act;
	act.sa_handler = cleanup;
	act.sa_mask = proc;
	act.sa_flags = 0;
	sigaction(SIGINT, &act, 0);
}

//destroys semaphore
void cleanup(int sig){

	sem_close(sem_free);
	sem_close(sem_running);
	sem_unlink("mysem_1");
	sem_unlink("mysem_2");
	exit(0);

}

