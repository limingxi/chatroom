//------------------------------server.cpp-------------------------------//
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string>
#include<errno.h>
#include<unistd.h>
#include<iostream>
using namespace std;

#define MAXBUF 600
typedef unsigned char uchar;
//--------------------SBCP structure-------------------------------------//
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
//-----------------used for encode the message into SBCP---------------//
struct Packet encode(char* msg, char *name, int type){
	struct Packet rev;
	rev.vrsn = 3;
	rev.type = 3;
	if(type==1){					//encode a rejection of connection packet due to username confliction
		rev.type_1 = 1;
		for(int i = 0;i<MAXBUF;i++)
		rev.message_1[i] = msg[i];
	}else{						//encode the message and username into a packet
		rev.type_1 = 2;
		for(int i = 0;i<MAXBUF;i++)
		rev.message_1[i] = name[i];
		rev.type_2 = 4;
		for(int i = 0;i<MAXBUF;i++)
		rev.message_2[i] = msg[i];
	}
	return rev;
}
//-----------------used for decode and return a packet to send---------//

struct Packet decode(struct Packet packet, string &nameset,int user, bool &fwd){
	Packet rev;
	memset(&rev,0,sizeof(rev));
	char name[MAXBUF];
	if(packet.vrsn==3){							//check if the packet is SBCP
		if(packet.type==2){						//type=2 means it is a join message
			size_t qclt = nameset.find(packet.message_1);		//check if username conflict
			if(qclt==string::npos){					//if not conflict, put the name in name set
				cout<<"user "<<packet.message_1<<" just joined!"<<endl;
				char mg[] = "I just joined the room!";
				rev = encode(mg,packet.message_1,4);
				size_t qcl = nameset.find("++");
				if(qcl!=string::npos){
					int whole = nameset.length();
					nameset = nameset.substr(0,qcl+1)+packet.message_1+nameset.substr(qcl+1,whole-qcl-1);
				}else{
				int len=0;
				for(int i = 0;i<MAXBUF;i++){
					if(packet.message_1[i]==0){
						len = i+1;
						break;
					}
				}
				for(int m = 0;m<len;m++)
					name[m] = packet.message_1[m];	
					nameset = nameset+name+'+';
				}	
				return rev;
			}else{			
				cout<<"detected a user name confliction! the second ";	//if conflict, encode a packet to inform
				size_t qcl = nameset.find("++");			//the user, and here we also put the name in
				if(qcl!=string::npos){					//name set but we will delete it in main function
					int whole = nameset.length();
					nameset = nameset.substr(0,qcl+1)+packet.message_1+nameset.substr(qcl+1,whole-qcl-1);
				}else{
				int len=0;
				for(int i = 0;i<MAXBUF;i++){
					if(packet.message_1[i]==0){
						len = i+1;
						break;
					}
				}
				for(int m = 0;m<len;m++)
					name[m] = packet.message_1[m];	
					nameset = nameset+name+'+';
				}	
				fwd = 1;
				char mg[] = "username confliction";
				rev = encode(mg,packet.message_1,1);
				return rev;
			}
		}else if(packet.type==4){					//type==4 means this is a SEND type message
			int pos=-1,post=0;					//so we encode the message as FWD type and
			for(int i=0;i<user;i++){				//return the packet waiting for foward.
				pos = nameset.find_first_of('+',pos+1);
				post = nameset.find_first_of('+',pos+1);
			}
			string usn = nameset.substr(pos+1,post-pos-1);
			for(unsigned int m = 0;m<usn.length();m++)
				name[m] = usn[m];
			rev = encode(packet.message_1,name,4);
			memset(name,0,sizeof(name));
			return rev;
		}else
			return rev;
	}else
		return rev;
}



//----------------------------main function------------------------------//
int main(int argc, char **argv){
	int ssd;							//setup all the variables
	int *csd = NULL;
	fd_set rfd;
	struct timeval timeout;
	struct sockaddr_in addr,caddr;
	short port=0;
	string nameset="++";
	struct Packet buf;
	int rev, maxclt, maxfd = 0, cltcount = 0;
	socklen_t size_addr = sizeof(struct sockaddr_in);
	int n_bytes = 0;
	if(argv[3])							//get the maximum of clients
		maxclt = atoi(argv[3]);
	else{
		perror("this program takes at least three arguments!");
		exit(0);
	}
	csd = (int *)malloc(maxclt*sizeof(int));
	int init;
	for(init=0;init<maxclt;init++)
		csd[init] = 0;
	ssd = socket(AF_INET,SOCK_STREAM,0);				//open a socket
	port = atoi(argv[2]);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(ssd,(struct sockaddr*)&addr,sizeof(addr));
	if(listen(ssd,10)==-1){						//listen to clients
		perror("error while setting socket to listen state");
		exit(0);
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	maxfd = ssd;
	csd[cltcount] = accept(ssd,(struct sockaddr*)&caddr,&size_addr);//accept the first client
	cltcount++;
	while(1){							//the main loop
		FD_ZERO(&rfd);
		FD_SET(ssd,&rfd);
		int l;
		for(l = 0;l<maxclt;l++){				//set the fds for the use of select
			if(csd[l]!=0){
				FD_SET(csd[l],&rfd);
				if(csd[l]>maxfd)
					maxfd = csd[l];
			}
		}
		rev = select(maxfd+1,&rfd,NULL,NULL,&timeout);		//select function
		if(rev<0){						//return value for select <0 means error
			perror("select error\n");
			break;
		}else if(rev==0){					//return value for select =0 we continue
			continue;
		}else{
		if(FD_ISSET(ssd,&rfd)){					//if FD_ISSET(ssd,&rfd) is true, another client come in
			int csdtemp = accept(ssd,(struct sockaddr*)&caddr,&size_addr);
			FD_CLR(ssd,&rfd);
			rev = 0;
			if(csdtemp>=0){
				if(cltcount+1>maxclt){
					printf("maxclient reached!");
				}else{
					if(csdtemp>maxfd)
						maxfd = csdtemp;
					for(int i=0;i<maxclt;i++){
						if(csd[i]==0){
							csd[i] = csdtemp;
							break;
						}
					}
					cltcount++;
				}				
			}
		}
		int i=0;
		for(i = 0;i < cltcount; i++){
			if(FD_ISSET(csd[i],&rfd)){			//check all the fds in rfd set
			FD_CLR(csd[i],&rfd);
			rev = 0;
			memset(&buf,0,sizeof(buf));
			n_bytes = read(csd[i],&buf,sizeof(buf));
			if(n_bytes==0){					//n_bytes==0 means client quit
				int pos = -1, post = 0;
				for(int j = 0;j<i+1;j++){
					pos = nameset.find_first_of('+',pos+1);
					post = nameset.find_first_of('+',pos+1);
				}
				int wholel = nameset.length();
				string qname =nameset.substr(pos+1,post-pos-1);
				nameset = nameset.substr(0,pos+1)+nameset.substr(post,wholel-post);
				cout<<"user "<<qname<<" quit!"<<endl;
				csd[i]=0;
				cltcount--;
				continue;
			}
			int j=0;
			bool fwd = 0;
			struct Packet pac = decode(buf,nameset,i+1,fwd);
			if(fwd==1){					//there is one case that
				memset(&buf,0,sizeof(buf));		//we can't foward, we need to send
				write(csd[i],&pac,sizeof(buf));		//reason back to the client due to
				memset(&pac,0,sizeof(pac));		//username confliction
			}else{
			for(j = 0;j<cltcount;j++){			//foward the message one by one except the sender
				if(csd[j]!=0&&j!=i&&pac.type==3&&pac.message_1[0]!=char(0)){
				memset(&buf,0,sizeof(buf));
				write(csd[j],&pac,sizeof(pac));
				}
			}
			memset(&pac,0,sizeof(pac));
			}	
		}
	}
	}
	}
	free(csd);
	return 0;
}

