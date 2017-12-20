#include <stdlib.h>
#include <stdio.h>
#include <time.h>
using namespace std;

/*************************
 * Header used for my protocol
 * Flag = 0: This is a message containing data
 * Flag = 1: This is an acknowledgement
 * Flag = 2: This is negative acknowledgement
 * Flag = 3: End of file flag
 * ***********************/
struct swphdr
{
	int seqNum;
	unsigned int resend = 0;
	unsigned int flag;
//	unsigned long chksum;
};

typedef struct frame
{
	unsigned int seqNum;
	char data[256];
	int isAcked = 0;
	int isLast = 0;
	int timeLeft = 5;
}frame;
