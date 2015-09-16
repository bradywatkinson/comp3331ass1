/* fpont 12/99 */
/* pont.net		*/
/* TCPClient.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close */
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#define SERVER_PORT 50000
#define TIMEOUT_PERIOD 200000

#define MAX_MSG 100

/*
 * peermsg is the structure of all messages passed between peers
 * id for each message: 1 == ping request, 0 == ping response
 */

struct peermsg {
	int msgtype;
	int id;
};

typedef struct peermsg *peermsg;

int main (int argc, char *argv[]) {

	if(argc != 4) {
		printf("usage: %s <id> <peer 1> <peer 2>\n",argv[0]);
		exit(1);
	}

	int myid = atoi(argv[1]);
	int child1 = atoi(argv[2]);
	int child2 = atoi(argv[3]);

	int sd1, sd2, rc, i;
	struct sockaddr_in myaddr,sendaddr,recvaddr;

	sd1=socket(AF_INET,SOCK_DGRAM,0);
	sd2=socket(AF_INET,SOCK_DGRAM,0);
	if(sd1<0 || sd2<0) {
		perror("cannot open socket ");
		exit(1);
	}

	// 
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_PERIOD;
	
	int num_timeouts = 0;
	char recvline[MAX_MSG];

	socklen_t len = sizeof(recvaddr);


	peermsg reqmsg = malloc(sizeof(struct peermsg));
	peermsg respmsg = malloc(sizeof(struct peermsg));

	/*
	 * Protocol has two main states;
	 * Wait for ping request; peer will wait for a message to be received on
	 * it's dedicated socket and send ping response
	 * Send ping request; if not ping reuqests are received before TIMEOUT_PERIOD
	 * the peer will initiate it's own ping request to each of it's children
	 */
	while (1) {

		printf("Waiting for requests\n");
		// Set up incoming essahe
		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd1,(struct sockaddr *)&myaddr,sizeof(myaddr));

		// Wait for TIMEOUT_PERIOD for a ping request
		bzero(&recvaddr,sizeof(recvaddr));
		if (setsockopt(sd1, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		if(recvfrom(sd1,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len) < 0){
			//timeout reached
			printf("timeout%d, no messages received on port %d.\n",num_timeouts++,SERVER_PORT+myid);
		// checks the content of the message to see if it is a ping request or ping response
		} else if (((peermsg)recvline)->msgtype == 1) {
			printf("A ping request message was received from Peer %d\n",((peermsg)recvline)->id);
			//printf("received %s on port %d\n", recvline, recvaddr.sin_port);
			

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->msgtype = 0;
			respmsg->id = myid;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+reqmsg->id);

			sendto(sd2,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));
		}
		//sleep(1);
		

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+child1);

		//char msg[255] = "test";
		reqmsg->msgtype = 1;
		reqmsg->id = myid;

		printf("Sending request %d to port to %d\n",reqmsg->msgtype, SERVER_PORT+child1);

		sendto(sd2,reqmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// Wait for response
		bzero(&recvaddr,sizeof(recvaddr));
		if (setsockopt(sd1, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		if(recvfrom(sd1,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len) < 0){
			//timeout reached
			printf("timeout%d, no response received on port %d.\n",num_timeouts++,SERVER_PORT+myid);
		// checks the content of the message to see if it is a ping request or ping response
		} else if (((peermsg)recvline)->msgtype == 0) {
			printf("A ping response message was received from Peer %d\n",((peermsg)recvline)->id);
		} else {
			printf("error: unrecognised message %d\n",((peermsg)recvline)->msgtype);
		}
		//sleep(1);
	}

	return 0;
	
}

