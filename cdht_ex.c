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
#define LOST_PACKET_THRESHOLD 10

// Message Types
#define RESPONSE 0
#define REQUEST 1
#define SUCC_DIED_REQUEST 10
#define SUCC_DIED_RESPONSE 20
#define FILE_REQUEST 30
#define FILE_RESPONSE 40
#define DEPARTING_PEER 50

// Statuses
#define NORMAL 0
#define DEAD_SUCCESSOR 10
#define DEPARTING 20

#define TRUE 1
#define FALSE 0

#define DEBUG 0

struct peermsg {
	char msgtype;
	char sender_id;
	short msg_info;
	short msg_info2;
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

	char *buffer = malloc(101);
	size_t nbytes = 100;
	char command[100];
	int num;
	while (TRUE) {
		num = 0;
		getline(&buffer,&nbytes,stdin);
		sscanf(buffer,"%s %d",command,&num);
		if (strcmp(command,"quit") == 0) {
			if (DEBUG) printf("Quit initiated\n");
			//printf("Peer %d will now depart from the network\n", myid);
			status = DEPARTING;

			//exit(1);
		} else if (strcmp(command,"request") ==0) {
			printf("File request message for %d has been sent to my successor.\n",num);

			// create message
			peermsg reqmsg = malloc(sizeof(struct peermsg));
			reqmsg->msgtype = FILE_REQUEST;
			reqmsg->sender_id = myid;
			reqmsg->msg_info = num;

			// open a free socket
			int sd;
			sd=socket(AF_INET,SOCK_DGRAM,0);
			if(sd<0) {
				perror("cannot open socket ");
				exit(1);
			}

			// create datagram
			struct sockaddr_in sendaddr;
			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+successor_first);

			sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

			close(sd);
			free(reqmsg);
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
	reqmsg->msg_info = 0;

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

void *send_file_response (void *arg)
{
	short *tmp = (short *)arg;
	if (DEBUG) printf("-->Sending a file response for file %d to %d\n",tmp[0],tmp[1]);

	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = FILE_RESPONSE;
	reqmsg->sender_id = myid;
	reqmsg->msg_info = tmp[0];

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	bzero(&sendaddr,sizeof(sendaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	sendaddr.sin_port=htons(SERVER_PORT+tmp[1]);

	sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

	close(sd);
	free(arg);
	free(reqmsg);

	return NULL;
}

void *send_file_request (void *arg)
{
	short *tmp = (short*)arg;
	if (DEBUG) printf("-->Forwarding a file request for file %d\n",tmp[0]);


	
	// create a new message
	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = FILE_REQUEST;
	reqmsg->sender_id = tmp[1];
	reqmsg->msg_info = tmp[0];

	// open a free socket
	int sd;
	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	// create the packet
	struct sockaddr_in sendaddr;
	bzero(&sendaddr,sizeof(sendaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	sendaddr.sin_port=htons(SERVER_PORT+successor_first);

	sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

	close(sd);
	free(arg);
	free(reqmsg);

	return NULL;
}

void *successor_first_ping ()
{
	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = REQUEST;
	reqmsg->sender_id = myid;
	reqmsg->msg_info = 0;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (status != DEPARTING) {

		++seqcount[0];
		
		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+successor_first);

		if (DEBUG) printf("Sending request to port %d\n", SERVER_PORT+successor_first);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// if we have sent a certain amount of packets more than we have received
		if (seqcount[0] > LOST_PACKET_THRESHOLD && successor_second != -1) {
			successor_first = successor_second;
			successor_second = -1;
			status = DEAD_SUCCESSOR;
			printf("Peer %d is no longer alive.\n",successor_first);
			printf("My first successor is now peer %d.\n",successor_second);
		}
		
		usleep(REQUEST_PERIOD);
	}

	return NULL;
}

void *successor_second_ping ()
{
	int sd;
	struct sockaddr_in sendaddr;

	pthread_t pth;

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = REQUEST;
	reqmsg->sender_id = myid;
	reqmsg->msg_info = 0;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (status != DEPARTING) {

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
		if (DEBUG) printf("Sending request to port %d\n", SERVER_PORT+successor_second);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// if we have sent a certain amount of packets more than we have received
		if (seqcount[1] > LOST_PACKET_THRESHOLD) {
			printf("Peer %d is no longer alive.\n",successor_second);
			successor_second = -1;
			status = DEAD_SUCCESSOR;
			printf("My first successor is now peer %d.\n",successor_first);
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
	respmsg->msg_info = 0;


	while (status != DEPARTING) {

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

			if (DEBUG) printf("Sending response to port %d\n",SERVER_PORT+((peermsg)recvline)->sender_id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_DIED_REQUEST) {

			if (DEBUG) printf("Successor Died Request received from %d."
					" Sending first succcessor: %d\n",((peermsg)recvline)->sender_id,successor_first);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->sender_id);

			respmsg->msgtype = SUCC_DIED_RESPONSE;
			respmsg->msg_info = successor_first;

			sendto(sd,respmsg,sizeof(peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_DIED_RESPONSE) {

			if (DEBUG) printf("Successor Died Response received from %d. Updating successor_second\n",((peermsg)recvline)->sender_id);
			printf("My second successor is now peer %d.\n",((peermsg)recvline)->msg_info);
			successor_second = ((peermsg)recvline)->msg_info;
			status = NORMAL;
			seqcount[!(((peermsg)recvline)->sender_id==successor_first)] = 0;
			
		} else if (((peermsg)recvline)->msgtype == FILE_REQUEST) {

			// if the hash of the requested file is between myid and my first successors id
			// OR I am the last peer in the chain and the hash is bigger than me
			// I have the file: send the response
			if (( myid > successor_first && (((peermsg)recvline)->msg_info+1)%256 > myid )
					|| ( (((peermsg)recvline)->msg_info+1)%256 >= myid && (((peermsg)recvline)->msg_info+1)%256 < successor_first )) {
				printf("\tFile %d is here.\nA response message, destined for peer %d, has been sent.\n",
					((peermsg)recvline)->msg_info,((peermsg)recvline)->sender_id);

				short *tmp = malloc(sizeof(short)*2);
				tmp[0] = ((peermsg)recvline)->msg_info;
				tmp[1] = ((peermsg)recvline)->sender_id;
				pthread_t pth;
				pthread_create(&pth, NULL, send_file_response, tmp);

			// else i don't have the file; forward the message			
			} else {
				printf("\tFile %d is not stored here.\nFile request message has been forwarded to my successor\n",((peermsg)recvline)->msg_info);
				short *tmp = malloc(sizeof(short)*2);
				tmp[0] = ((peermsg)recvline)->msg_info;
				tmp[1] = ((peermsg)recvline)->sender_id;
				pthread_t pth;
				pthread_create(&pth, NULL, send_file_request, tmp);
			}
			
		} else if (((peermsg)recvline)->msgtype == FILE_RESPONSE) {
			
			printf("Received a response message from peer %d, which has the file %d.\n"
					,((peermsg)recvline)->sender_id,((peermsg)recvline)->msg_info);

		} else if (((peermsg)recvline)->msgtype == DEPARTING_PEER) {

			printf("Received departing peer message: first 1; %d second; %d.\n",((peermsg)recvline)->msg_info,((peermsg)recvline)->msg_info2);

			// case 1: my first successor has died
			if (((peermsg)recvline)->sender_id == successor_first) {
				successor_first = ((peermsg)recvline)->msg_info;
				successor_second = ((peermsg)recvline)->msg_info2;
			} else {
				successor_second = ((peermsg)recvline)->msg_info;
			}
			printf("Peer %d will depart from the network\n",((peermsg)recvline)->sender_id);
			printf("My first successor is now peer %d.\n",successor_first);
			printf("My second successor is now peer %d.\n",successor_second);

		}
	}

	int count = 0;
	int curr = -1;

	// wait for two distinct responses
	while (count<2) {

		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd,(struct sockaddr *)&myaddr,sizeof(myaddr));

		socklen_t len = sizeof(recvaddr);

		recvfrom(sd,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len);

		// found a unique predecessor
		if (((peermsg)recvline)->msgtype == REQUEST && ((peermsg)recvline)->sender_id != curr) {

			curr = ((peermsg)recvline)->sender_id;

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->sender_id);

			respmsg->msgtype = DEPARTING_PEER;
			respmsg->sender_id = myid;
			respmsg->msg_info = successor_first;
			respmsg->msg_info2 = successor_second;


			printf("Sending response to port %d: first; %d second; %d\n",SERVER_PORT+((peermsg)recvline)->sender_id, successor_first, successor_second);
			sendto(sd,respmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

			++count;
		}
	}
	exit(1);

	return NULL;
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
