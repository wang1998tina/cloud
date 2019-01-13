#define DEPLOY 0
#define STATUS 1
#define RETRIEVE 2

typedef struct req {
	int request_type;
	struct sockaddr_in client_addr;
	int ticket;
	char dir[20];
	int replicas;
} request;

typedef struct nodes{
	pthread_t tid;
	int sock;
} node;

typedef struct jobs{
	int ticket_num;
	node assoc_nodes[5];
} job;

extern int n_nodes;
extern node * free_nodes;
extern sem_t sem_free;
extern job * running_jobs;
extern sem_t sem_running;


void create_side_nodes();
void cleanup(int sig);
void handle_sig();

