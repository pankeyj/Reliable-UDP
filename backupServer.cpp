#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "slide.h"
#define BUF_SIZE 256
#define LINE_SIZE 242
#define HDR_LENGTH 14
using namespace std;
void * recvAck(struct swphdr*);
void decreaseTimers(void*);
void resend(frame*, void*);
struct sockaddr_in serveraddr,clientaddr;
int lastAck = 0, lastSent = 1, nextAck = 0;
vector<frame> window;





unsigned short int ip_checksum(const void * buf, size_t hdr_len)
{
        unsigned long sum = 0;
        const unsigned short int *ip;
        ip = (const unsigned short int*)buf;
        while(hdr_len > 1)
        {
                sum += *ip++;
                if(sum & 0x80000000)
                        sum = (sum & 0xFFFF) + (sum >> 16);
                hdr_len -= 2;
        }

        while(sum >> 16)
                sum = (sum & 0xFFFF) + (sum >> 16);

        return(~sum);
}







/*********************************************
 * This function opens the file specified by
 * fileName and reads the file 256 bytes at a 
 * time. These bytes are sent to socket arg to
 * be written to disk
 * @param fileName specifies the file to send
 * @param arg specifies the socket to send to
 ********************************************/
void sendFile(char *fileName, void *arg)
{
        unsigned int len = sizeof(struct sockaddr_in);
	struct swphdr *hdr;
	hdr = (struct swphdr *)malloc(sizeof(struct swphdr));
	FILE *fp = fopen(fileName, "rb");
	int clientSocket = *(int*)arg;
	char line[LINE_SIZE];
	char sendBuffer[BUF_SIZE];
	if(!(fp))
	{
		printf("ERROR: FILE %s NOT FOUND\n", fileName);
		return;
	}

	int bit = 1;
	int n;
	
	hdr->seqNum = lastSent;
	hdr->flag = 0;
	
	// Read file and send data chunks until end of file is reached
	while(bit)
	{
		// We only send if the window size is less than 5
		if(window.size() < 5)
		{
			memset(sendBuffer, '\0', BUF_SIZE);
			memset(line, '\0', LINE_SIZE);
			window.push_back(frame());
			window[window.size() - 1].seqNum = lastSent; 
			cout << "Current window" << window.size() << endl;
			n = fread(line, 1 , LINE_SIZE, fp);
			memcpy(window[window.size() - 1].data, line, LINE_SIZE);
			cout << "Line" << line << endl;
			// Increase the sequence number
			hdr->seqNum = lastSent;

			// Add our header to the send buffer
			memcpy(sendBuffer, hdr, HDR_LENGTH);
			memcpy(sendBuffer + HDR_LENGTH, line, n);


			// True when last data chunk is sent. Set bool to 0
			// and exit the while loop
			if (n < LINE_SIZE)
			{
				if(feof(fp))
					printf("End of file\n");
				if(ferror(fp))
					printf("Error reading\n");
				bit = 0;				
				hdr->flag= 3;
				window[window.size() - 1].isLast = 1;
				memcpy(sendBuffer, hdr, HDR_LENGTH);				
			}	

			
		        
                        sendto(clientSocket, sendBuffer,
                                n + HDR_LENGTH, 0,(struct sockaddr*)&clientaddr,
                                sizeof(serveraddr));
                        lastSent++;

			decreaseTimers(arg);
		}
		else
		{
			// Non blocking receive call this time around.
			// If acknowledgement is dropped we don't want
			// to get stuck here.
			decreaseTimers(arg);
			cout << "Server stuck in loop" << endl;
			int n = recvfrom(clientSocket, line, 256, MSG_DONTWAIT, 
				(struct sockaddr*) &clientaddr, &len);
			
			if( n == -1 )
				continue;
			
	                memset(hdr, '\0', sizeof(struct swphdr));
	                memcpy(hdr, line, sizeof(struct swphdr));
	                cout << "Recieved from client: " << line << endl;

	                if(hdr->flag == 1)
	                {
	                        cout << "Recieved an acknowldegement" << endl;
	                        recvAck(hdr);
	                }
	                memset(line, '\0', sizeof(line));
		}
	}
	while(window.size() != 0)
	{
		decreaseTimers(arg);
		int n = recvfrom(clientSocket, line, 256, MSG_DONTWAIT,
			(struct sockaddr*) &clientaddr, &len);
		if( n == -1 )
			continue;

		memset(hdr, '\0', sizeof(struct swphdr));
		memcpy(hdr, line, sizeof(struct swphdr));
		cout << "Recieved from client: " << line << endl;

		if(hdr->flag == 1)
		{
			cout << "Recieved an acknowldegement" << endl;
			recvAck(hdr);
		}
		memset(line, '\0', sizeof(line));

	}

	fclose(fp);
	cout << "Exited loop\n";
}






void decreaseTimers(void *arg)
{
	sleep(1);
	for(int i = 0; i < window.size(); i++)
	{
		window[i].timeLeft--;
		cout << "---------------Time left: " << window[i].timeLeft << endl;
		if(window[i].timeLeft <= 0)
		{
			resend(&window[i], arg);
			window[i].timeLeft = 5;	
		}
	}
}

void resend(frame *old, void *arg)
{

	cout << "---------------Resending frame: " << old->seqNum << endl;
	int j;
	char sendBuffer[256];
	int clientSocket = *(int*)arg;
	struct swphdr *hdr;
	hdr  =(struct swphdr*)  malloc(sizeof(struct swphdr));

	hdr->seqNum = old->seqNum;
	hdr->resend = 1;
//	hdr->chksum = ip_checksum((const void*)sendBuffer,BUF_SIZE);

	if(old->isLast)
		hdr->flag = 3;
	else
		hdr-> flag = 0;
	memcpy(sendBuffer, hdr, sizeof(struct swphdr));
	memcpy(sendBuffer+HDR_LENGTH, old->data, sizeof(old->data));







	sendto(clientSocket, sendBuffer,256,
		0,(struct sockaddr*)&clientaddr,sizeof(serveraddr));

	
}
void* recvAck(struct swphdr *hdr)
{
	cout << "AckRecvd\n SeqNum\t" << hdr->seqNum << endl;
	cout << "Seq Number of front frame" << window.front().seqNum << endl;

	for(int i = 0; i < window.size(); i++)
	{
		if(window[i].seqNum == hdr->seqNum)
		{
			window[i].isAcked = 1;
		}
	}
	cout << "In between loops" << endl;
	// If the front of the window has been acknowledged remove it
	// Continue until the new front frame is not acknowledged
	int j = 1;
	if(window.size() != 0 || window.size() < 10000){
	while(j)
	{
		if(window.front().isAcked)
		{
			cout << "Window size: " << window.size() << endl;
			cout << "Removing from window" <<
				 window.front().seqNum << endl;
			window.erase(window.begin());
		}
		else
		{
			j = 0;
		}
		if(window.size() == 0 || window.size() > 10000)
			j = 0;
	}
	}
}


int main(int argc,char ** argv)
{
	if(argc != 2)
        {
		cout << "Usage error! One int arg 'port number' expected" << endl;
		return 1;
        }

	/*Used for listening not communication*/
	int sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd < 0)
	{
		perror("There was an error creating the socket");
		return 1;
	}

	int port = atoi(argv[1]);
	cout << "Port: " << port << endl;
	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(port);
	serveraddr.sin_addr.s_addr=INADDR_ANY;
	
	bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
	
	unsigned int len = sizeof(struct sockaddr_in);
	char line[256];	
	struct swphdr *hdr = (struct swphdr*)calloc(1,sizeof(struct swphdr));
	while(1)
	{	
		cout << "Waiting to recieve" << endl;
		int n = recvfrom(sockfd, line, 257, 0, (struct sockaddr*)
			&clientaddr, &len);
		memset(hdr, '\0', sizeof(struct swphdr));
		memcpy(hdr, line, sizeof(struct swphdr));
		cout << "Recieved from client: " << line << endl;

		if(hdr->flag == 0)
		{
			cout << "Recieved a file transfer request" << endl;
			sendFile(line + HDR_LENGTH, &sockfd);
		}
/*		else if(hdr->flag == 2)
		{
			cout << "Recieved a negative acknowledgement" << endl;
//			resend(window[hdr->seqNum, &sockfd);
		}
		else
		{
			cout << "Recieved an acknowldegement" << endl;
			recvAck(hdr);
		}
*/
		memset(line, '\0', sizeof(line));
	}	
	return 0;
}

