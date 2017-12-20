/******************************************
 * CS457 Reliable UDP Programming
 * client.c
 * Purpose: Enforce reliable data transfer
 * @author Jacob Pankey
********************************************/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include "slide.h"
#include <vector>
#define BUF_SIZE 256
#define LINE_SIZE 244
#define HDR_LENGTH 12

using namespace std;
/
FILE *output;
struct sockaddr_in serveraddr;
int sockfd, lastWrite;
/* List holds out of order frames */
vector<frame> others;

int isWriting = 1;

/********************************************
 * This function sends an acknowledgement
 * to the server everytime it recieves a
 * message. Changes the header flag to 1 to
 * signal that this is an acknowledgement
 * @param - The header for the packet that was
 * recieved
 *********************************************/
void sendAck(struct swphdr *hdr)
{
	cout << "Sending Ack for: " << hdr->seqNum << endl;
        char sendBuffer[256];
        hdr->flag = 1;
        memcpy(sendBuffer, hdr, sizeof(struct swphdr));
        sendto(sockfd,sendBuffer,256 ,0 ,(struct sockaddr*)
                &serveraddr, sizeof(serveraddr));
}


/********************************************
 * Standard Checksum algorithm for our very
 * short header. This is not implemented 
 * entirely.
 * *********************************************/
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







/********************************************
 * After recieving a packet we call this function
 * to decide if it is the next packet to write.
 * We send an ackowledgement regardless. If 
 * it is not the next frame we add it to the
 * vector list. If it is the last fram to write
 * we mark it with the is last flag.
 *******************************************/
int isNextFrame(struct swphdr *hdr, char *writeLine, int bytesRecv)
{
	cout << "lastFrame written: " << lastWrite << endl;
	cout << "Recieved Frame #:" << hdr->seqNum << endl;
	if(hdr->seqNum == lastWrite + 1)
	{
		cout << "WriteLine\n" << writeLine << endl;
		fwrite(writeLine, strlen(writeLine), 1, output);
		lastWrite = hdr->seqNum;
		if(hdr->flag == 3)
		{
			cout << "Finsihed writing the last frame" << endl;
			isWriting = 0;
		}
		return 1;
	}
	else if(hdr->seqNum <= lastWrite)
	{		
		cout << "Recieved a frame that we have already written" << endl;
		sendAck(hdr);
		return 0;
	}
	else
	{
		cout << "Adding frame to others: " << hdr->seqNum << endl;
		others.push_back(frame());
		others[others.size() - 1].seqNum = hdr->seqNum;
		strcpy(others[others.size() - 1].data, writeLine);
		if(hdr->flag == 3)
			others[others.size() -1].isLast = 1;
		return 0;
	}
}
/*************************************************
 * After we find the next frame inside the list it
 * is possible that we are ready to write many others.
 * When there exists more frames to write the function
 * writes to the file and returns 1 to indicate that
 * there may be more frames in the list to write
 **************************************************/
int checkForNextFrame()
{
	cout << "Checking for next frame" << endl;
	int i;
	for(i = 0; i < others.size(); i++)
	{
		if(others[i].seqNum == lastWrite + 1)
		{	
			cout << "Found next frame: " << others[i].seqNum << endl;
			fwrite(others[i].data, strlen(others[i].data), 1, output);
			lastWrite = others[i].seqNum;
			others.erase(others.begin() + i);
			if(others[i].isLast)
			{	
				cout << "Finished writing the last frame" << endl;
				isWriting = 0;
				return 0;
			}
			return 1;	
		}
	}
	return 0;
}




/********************************************
 * Function writes data recieved from server
 * to a file named output.txt. Continues to
 * read from socket until server reaches 
 * end of file. this function only writes a
 * single file.
 * @param *arg - specifies the socket to read
 * data in from
 *******************************************/

void writeFile()
{
	int open = 1;
        output = fopen("output.txt", "wb");

        cout << "Created the file" << endl;
        if(!output)
        {
                perror("File error\n");
                return;
        }


        struct swphdr *hdr = (struct swphdr*)calloc(1, sizeof(struct swphdr));


	// This while loop is messy but cleans up duplicates
	// after we have finished writing
	while(1)
	{
	unsigned int len = sizeof(struct sockaddr_in);
	char line[BUF_SIZE];
	char writeLine[LINE_SIZE];
	int bytesRecv = recvfrom(sockfd, line, 256, 0, (struct sockaddr*)
                        &serveraddr, &len);

	
	memset(writeLine, '\0', LINE_SIZE);
	memcpy(hdr, line, HDR_LENGTH);
	memcpy(writeLine, line + HDR_LENGTH, bytesRecv-HDR_LENGTH);

	cout << "Ch3eck sum read " << hdr->chksum << endl;
	hdr->chksum = ip_checksum((const void*)line,12);
	cout << "!!! Checksum value" << hdr->chksum << endl;


	// This takes care of duplicate files being sent when we have finished writing
	if(hdr->seqNum <= lastWrite)
	{
		sendAck(hdr);
		continue;
	}

	// We can no longer use buffer length to decide
	// if this is the last frame. We know rely on a
	// header flag which adjust the value of isWriting
	while(isWriting)
	{
		memset(hdr, '\0',HDR_LENGTH );
		memset(writeLine, '\0', LINE_SIZE);
		memcpy(hdr, line, HDR_LENGTH);
		memcpy(writeLine, line + HDR_LENGTH, bytesRecv-HDR_LENGTH);
		memset(line, '\0', BUF_SIZE);
	

		cout << "#####Checksum read  " << hdr->chksum << endl;
                hdr->chksum = ip_checksum((const void*)line,12);
                cout << "###Check sum is :" << hdr->chksum<<endl;

		if(isNextFrame(hdr, writeLine, bytesRecv))
			while(checkForNextFrame());
		sendAck(hdr);
		// Small bug fix so we don't rewrite a duplicate
		if(isWriting)
		{			
			bytesRecv = recvfrom(sockfd, line, 256, 0, (struct sockaddr*)
				&serveraddr, &len);
		}
	}
	
	memset(hdr, '\0', HDR_LENGTH);
	memset(writeLine, '\0', LINE_SIZE);
	memcpy(hdr, line, HDR_LENGTH);
	memcpy(writeLine, line + HDR_LENGTH, bytesRecv-HDR_LENGTH);
	memset(line, '\0', BUF_SIZE);
	
	if(open)
		fclose(output);
	open = 0;
	
	}
	free(hdr);
}


int main(int argc, char** argv)
{
	// Check arguments and read them
	if(argc != 3)
	{
		cout << "Usage: Port, Ip required only" << endl;
	}
        int port = atoi(argv[1]);
        char ip[50];
        strcpy(ip, argv[2]);

	// Create the socket and check for errors
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(sockfd < 0)
	{
		printf("There was an error creating the socket\n");
		return 1;
	}

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = inet_addr(ip);
	
	// Client continuously loops to accept a new file
	// after the first finishes writing
	char line[LINE_SIZE];
	char sendBuffer[BUF_SIZE];
	
	unsigned int len = sizeof(struct sockaddr_in);

	// this loop is where we choose a file to send.
	// Should be multithreaded but isn't
	while(1)
	{
	        struct swphdr *reqhdr;
	        reqhdr->seqNum = 0;
		reqhdr->flag = 0;

		cout << "Which file would you like to receive" << endl;
		scanf("%s", &line);	
		memcpy(sendBuffer, reqhdr, HDR_LENGTH);
		memcpy(sendBuffer + HDR_LENGTH, line, LINE_SIZE);

                sendto(sockfd, sendBuffer, 256, 0, (struct sockaddr*)
                        &serveraddr, sizeof(serveraddr));

		cout << "Sent:" << line << endl;
		memset(line, '\0', LINE_SIZE);
		memset(sendBuffer, '\0', BUF_SIZE);
		writeFile();
	}

	return 0;
}
