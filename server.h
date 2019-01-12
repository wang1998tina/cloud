#define DEPLOY 0
#define STATUS 1
#define RETRIEVE 2

typedef struct req {
	int request_type;
	struct sockaddr_in client_addr;
	int ticket;
	char dir[20];
} request;

typedef struct nodes{
	pthread_t tid;
	int sock;
} node;

extern int n_nodes;
extern node * free_nodes;


void create_side_nodes();
void cleanup(int sig);
void handle_sig();

