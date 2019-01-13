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
