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


#include <pthread.h>
#include <sys/time.h>

#define SERVER_PORT 50000
#define TIMEOUT_PERIOD 200000 //usec
#define REQUEST_PERIOD 2000000 //usec
#define RESPONSE_TIMEOUT 100000 //usec

#define MAX_MSG 100

#define RESPONSE 0
#define REQUEST 1
#define SUCC_DIED_REQUEST 10
#define SUCC_DIED_RESPONSE 20

#define NORMAL 0
#define DEAD_SUCCESSOR 10

#define LOST_PACKET_THRESHOLD 4

#define TRUE 1
#define FALSE 0

struct peermsg {
	char msgtype;
	char sender_id;
	char successor_id;
};

typedef struct peermsg *peermsg;

char myid;		// the id of the active peer
char successor_first;	// the active peers current first successor
char successor_second;	// the active peers current second successor
char status;	// the active peers current message status

int seqcount[2];

struct timeval tv;

void *input ()
{
	char buffer[100];
	while (TRUE) {
		scanf("%s",buffer);
		if (strcmp(buffer,"quit") == 0) {
			printf("Quit initiated\n");
			exit(1);
		}
	}
	return NULL;
}

void *replace_sucessor ()
{

	printf("-->Successor has no response. Begin replacing\n");

	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = SUCC_DIED_REQUEST;
	reqmsg->sender_id = myid;
	reqmsg->successor_id = 0;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	bzero(&sendaddr,sizeof(sendaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	sendaddr.sin_port=htons(SERVER_PORT+successor_first);

	sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

	close(sd);

	free(reqmsg);

	return NULL;
}

void *successor_first_ping (void *argv)
{
	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = REQUEST;
	reqmsg->sender_id = myid;
	reqmsg->successor_id = 0;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (TRUE) {

		++seqcount[0];
		
		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+successor_first);

		printf("Sending request to port %d\n", SERVER_PORT+successor_first);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// if we have sent a certain amount of packets more than we have received
		if (seqcount[0] > LOST_PACKET_THRESHOLD && successor_second != -1) {
			successor_first = successor_second;
			successor_second = -1;
			status = DEAD_SUCCESSOR;
		}
		
		usleep(REQUEST_PERIOD);
	}

	return NULL;
}

void *successor_second_ping (void *argv)
{
	int sd;
	struct sockaddr_in sendaddr;

	pthread_t pth;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = REQUEST;
	reqmsg->sender_id = myid;
	reqmsg->successor_id = 0;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (TRUE) {

		while (status == DEAD_SUCCESSOR) {
			pthread_create(&pth, NULL, replace_sucessor, NULL);
			usleep(200000);
		}
		
		++seqcount[1];

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+successor_second);

		//printf("Sending request %d %d to port %d\n",reqmsg->msgtype, reqmsg->sender_id, SERVER_PORT+successor_first);
		printf("Sending request to port %d\n", SERVER_PORT+successor_second);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// if we have sent a certain amount of packets more than we have received
		if (seqcount[1] > LOST_PACKET_THRESHOLD) {
			successor_second = -1;
			status = DEAD_SUCCESSOR;
		}
		
		usleep(REQUEST_PERIOD);
	}

	return NULL;
}


void *receiver (void *argv)
{
	int sd;
	struct sockaddr_in myaddr,sendaddr,recvaddr;
	char recvline[MAX_MSG];

	peermsg respmsg = malloc(sizeof(struct peermsg));

	respmsg->msgtype = 0;
	respmsg->sender_id = myid;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	respmsg->msgtype = 0;
	respmsg->sender_id = myid;
	respmsg->successor_id = 0;


	while (TRUE) {

		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd,(struct sockaddr *)&myaddr,sizeof(myaddr));

		socklen_t len = sizeof(recvaddr);

		recvfrom(sd,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len);
		//printf("A message was received: %d %d\n", ((peermsg)recvline)->msgtype, ((peermsg)recvline)->sender_id);
		if (((peermsg)recvline)->msgtype == RESPONSE) { 
			printf("A ping response message was received from Peer %d\n",((peermsg)recvline)->sender_id);
			seqcount[!(((peermsg)recvline)->sender_id==successor_first)] = 0;
		} else if (((peermsg)recvline)->msgtype == REQUEST) {
			printf("A ping request message was received from Peer %d\n",((peermsg)recvline)->sender_id);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->sender_id);

			respmsg->msgtype = RESPONSE;

			printf("Sending response to port %d\n",SERVER_PORT+((peermsg)recvline)->sender_id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_DIED_REQUEST) {

			printf("Successor Died Request received from %d."
					" Sending first succcessor: %d\n",((peermsg)recvline)->sender_id,successor_first);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->sender_id);

			respmsg->msgtype = SUCC_DIED_RESPONSE;
			respmsg->successor_id = successor_first;

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_DIED_RESPONSE) {

			printf("Successor Died Response received from %d. Updating successor_second\n",((peermsg)recvline)->sender_id);
			successor_second = ((peermsg)recvline)->successor_id;
			status = NORMAL;
			seqcount[!(((peermsg)recvline)->sender_id==successor_first)] = 0;
		}
	}
}

int main (int argc, char *argv[])
{

	if(argc != 4) {
		printf("usage: %s <id> <peer 1> <peer 2>\n",argv[0]);
		exit(1);
	}

	myid = atoi(argv[1]);
	successor_first = atoi(argv[2]);
	successor_second = atoi(argv[3]);
	seqcount[0] = 0;
	seqcount[1] = 0;

	status = NORMAL;

	// set up socket timeout delay;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_PERIOD;
	
	pthread_t pthSF;
	pthread_t pthSS;
	pthread_t pthR;	
	pthread_t pthI;
	pthread_create(&pthSF, NULL, successor_first_ping, NULL);
	usleep(1000);
	pthread_create(&pthSS, NULL, successor_second_ping, NULL);
	usleep(1000);
	pthread_create(&pthR, NULL, receiver, NULL);
	usleep(1000);
	pthread_create(&pthI, NULL, input, NULL);

	while(TRUE);
}