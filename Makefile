CC= gcc -Wall -lpthread
OBJ=obj
BIN=bin
C=client

cloudapps: ${BIN}/deploy ${BIN}/server

runserver: runs

rundeploy: rund

${OBJ}/server.o: server.h server.c
	${CC}/ -c -o ${OBJ}/server.o server.c

${OBJ}/deploy.o: ${C}/client.h ${C}/deploy.c
	${CC}/ -c -o ${OBJ}/deploy.o ${C}/deploy.c -I${C}

${BIN}/server: ${OBJ}/server.o
	${CC} -o ${BIN}/server ${OBJ}/server.o

${BIN}/deploy: ${OBJ}/deploy.o
	${CC} -o ${BIN}/deploy ${OBJ}/deploy.o


runs: ${BIN}/server
	${BIN}/server

rund: ${BIN}/deploy
	${BIN}/deploy 1 client/client.tar.gz


clean:
	rm -f ${OBJ}/* ${BIN}/*
	rm -f server 
	rm -rf output_*

	


