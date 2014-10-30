//--------------------------------client.cpp-----------------------------//
#include<stdio.h>
#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string>
#include<unistd.h>
using namespace std;
#define MAXBUF 600


typedef unsigned char uchar;
//-------------------------SBCP structure--------------------------------//
struct Packet{
	uint16_t vrsn:9;
	uint16_t type:7;
	short length;
	short type_1;
	short len_1;
	char message_1[MAXBUF];
	short type_2;
	short len_2;
	char message_2[MAXBUF];
};
//-------------------used for decode SBCP packet from server-------------//
bool decode(struct Packet *packet){
	if(packet->vrsn==3){						//check if the packet is SBCP type
		if(packet->type==3){					//check if the SBCP type is FWD
			if(packet->type_1 == 1){			//check the SBCP attribute type
				cout<<"username confliction!"<<endl;
				return 0;
			}else{
				cout<<packet->message_1<<" says: "<<packet->message_2<<endl;
return 1;		}}else
			return 1;
	}else{
		return 1;
	}
}
//-------------------used for encode message into SBCP-------------------//
struct Packet encode(char* text, int type, int length){
	struct Packet rev;
	rev.vrsn = 3;
	rev.length = 8+length;
	rev.len_1 = length;
	if(type==1){							//type==1 means encode the message in JOIN type
		rev.type = 2;
		rev.type_1 = 2;
		for(int i = 0;i<MAXBUF;i++)
			rev.message_1[i] = text[i];
	}else{								//else we encode it in SEND type
		rev.type = 4;
		rev.type_1 = 4;
		for(int i = 0;i<MAXBUF;i++)
			rev.message_1[i] = text[i];
	}
	rev.len_2 = 0;
	rev.type_2 = 0;
	return rev;
}
//-------------------------main function---------------------------------//
int main(int argc, char **argv){
	int csd;							//initialize all the variables
	struct sockaddr_in addr;
	struct hostent* hret;
	short port = 0;
	char buf[MAXBUF];
	struct Packet msg;
	
	int n_bytes = 0;
	fd_set rfd;
	struct timeval timeout;
	int rev, maxfd = 0;
	if((csd = socket(AF_INET,SOCK_STREAM,0))==-1){			//open socket
		perror("cannot create client socket");
		exit(0);
	}

	port = atoi(argv[3]);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	hret = gethostbyname(argv[2]);
	memcpy(&addr.sin_addr.s_addr, hret->h_addr, hret->h_length);
	
	if(connect(csd, (struct sockaddr*)&addr, sizeof(struct sockaddr))==-1){		//connect to server
		perror("Cannot connect to server");
		exit(0);
	}
	cout<<"My user name is: "<<argv[1]<<endl;
	msg = encode(argv[1],1,sizeof(argv[1]));				//encode a JOIN msg and send
	write(csd,&msg,sizeof(msg));
	memset(&msg,0,sizeof(msg));
	while(1){								//main loop
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		FD_ZERO(&rfd);
		FD_SET(0,&rfd);
		FD_SET(csd,&rfd);						//set up the fd set
		if(csd>maxfd)
			maxfd = csd;
		rev = select(maxfd+1,&rfd,NULL,NULL,&timeout);			//select function
		if(rev==-1){
			perror("error select!");
			break;
		}
		else if(rev==0){}
		else{
			if(FD_ISSET(csd,&rfd)){					//if client has packet come in
				FD_CLR(csd,&rfd);				//decode it and print out
				rev = 0;
				n_bytes = read(csd,&msg,sizeof(msg));
				bool re = decode(&msg);
				if(re == 0){
					close(csd);
					return 0;
				}
				memset(&msg,0,sizeof(msg));
			}
			if(FD_ISSET(0,&rfd)){					//if client is typing
				FD_CLR(0,&rfd);					//encode msg and send to server
				rev = 0;
				cin>>buf;
				msg = encode(buf,0,sizeof(buf));
				write(csd,&msg,sizeof(msg));
				memset(&msg,0,sizeof(msg));
				memset(buf,0,sizeof(buf));
			}
		}
	}
	close(csd);
	return 0;
}
