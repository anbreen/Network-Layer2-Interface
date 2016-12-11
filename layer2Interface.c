#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "iwlib.h"		/* Header */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/un.h> /* for Unix domain sockets */

#define bufferSize 17
#define TLVSize 17
#define serverPortNo 4439

#define CL2IPortNo 4239
#define UserAgentPortNo 4103
#define EMFCorePortNo 4105

///////structures used/////////////////////////////////////////
struct wireinfo   //structure used to monitor the information of the event LGD
{
	int link;
	int level;
	int noise;
	char name[100];
};

typedef struct Triggerinfo //the information that is send to the host agent is stored in thsi structure
{
	int type;
	char interfaceName[100];
	char IP[40];
} Trigger_info;

struct node {			// single node of the link list
	char data[100];
	struct node * next;
};

typedef struct Statusinfo// structure used to maintain and fetch the status fo every interface
{
	int up;
	int running;
} Status_info;

typedef struct node node;
node * head, *save, *current;
node * getnode();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int OpenUDC()			// function to open a connection with HA
{
	int on = 1;
	// Declarations
	int socketFD, bytesReceived;
	struct sockaddr_in serverAddress, clientAddress;

	// Create Socket
	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		exit(0);
	}

	if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))
			== -1) {
		perror("setsockopt");
		exit(1);
	}

	clientAddress.sin_family = AF_INET;
	clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	;
	clientAddress.sin_port = htons(CL2IPortNo);
	memset(clientAddress.sin_zero, '\0', sizeof clientAddress.sin_zero);
	if (bind(socketFD, (struct sockaddr*) &clientAddress, sizeof(clientAddress))
			== -1) {
		perror("\nBind");
		return -1;
	}

	// 3. Client Connects with the Server at the Server's Listening Port
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");			//
	serverAddress.sin_port = htons(serverPortNo);
	memset(serverAddress.sin_zero, '\0', sizeof serverAddress.sin_zero);
	if (connect(socketFD, (struct sockaddr*) &serverAddress,
			sizeof(serverAddress)) == -1) {
		perror("Connect");
		return -1;
	}
	return socketFD;
}

node * getnode() // function to allocate memory to a node of link list
{
	node * temp;
	temp = (node *) malloc(sizeof(node));

	if (temp == NULL) {
		printf("\nMemory allocation Failure!\n");
		return temp;
	} else
		return (temp);
}

void getIP(Trigger_info *Tinfo, int iofd) // when link up trigger is generated we all need to send the IP address in the trigger infor to the HA
{

	int i;
	struct ifreq ifreq;
	struct sockaddr_in *sin;
	strcpy(ifreq.ifr_name, Tinfo->interfaceName);
	if ((ioctl(iofd, SIOCGIFADDR, &ifreq)) < 0) {
		//fprintf(stderr, "SIOCGIFADDR: %s\n", strerror(errno));
		return;
	}
	sin = (struct sockaddr_in *) &ifreq.ifr_broadaddr;
	sprintf(Tinfo->IP, "%s", inet_ntoa(sin->sin_addr));

}

void MakeWireInfo(struct wireinfo *winfo, const iwqual *qual,
		const iwrange * range, int has_range) {
	int len;
	if (has_range
			&& ((qual->level != 0)
					|| (qual->updated & (IW_QUAL_DBM | IW_QUAL_RCPI)))) {
		if (!(qual->updated & IW_QUAL_QUAL_INVALID)) {
			winfo->link = qual->qual;
		}

		if (qual->updated & IW_QUAL_RCPI) {
			if (!(qual->updated & IW_QUAL_LEVEL_INVALID)) {
				double rcpilevel = (qual->level / 2.0) - 110.0;
				winfo->level = rcpilevel;
			}

			if (!(qual->updated & IW_QUAL_NOISE_INVALID)) {
				double rcpinoise = (qual->noise / 2.0) - 110.0;
				winfo->noise = rcpinoise;
			}
		} else {
			if ((qual->updated & IW_QUAL_DBM)
					|| (qual->level > range->max_qual.level)) {
				if (!(qual->updated & IW_QUAL_LEVEL_INVALID)) {
					int dblevel = qual->level;
					if (qual->level >= 64)
						dblevel -= 0x100;
					winfo->level = dblevel;
				}
				if (!(qual->updated & IW_QUAL_NOISE_INVALID)) {
					int dbnoise = qual->noise;
					if (qual->noise >= 64)
						dbnoise -= 0x100;
					winfo->noise = dbnoise;
				}
			} else {
				if (!(qual->updated & IW_QUAL_LEVEL_INVALID)) {
					winfo->level = qual->level;
				}

				if (!(qual->updated & IW_QUAL_NOISE_INVALID)) {

					winfo->noise = qual->noise;

				}
			}
		}
	} else {
		winfo->level = qual->level;
		winfo->link = qual->qual;
		winfo->noise = qual->noise;
	}
}

void StatusFuntion(Status_info *Sinfo, char *interfacename) // fetch and maintain the status of interfaces
{
	int fd;
	struct ifreq ifreq;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	strcpy(ifreq.ifr_name, interfacename);

	if ((ioctl(fd, SIOCGIFFLAGS, &ifreq) < 0)) {
		Sinfo->up = 0;
		Sinfo->running = 0;
		//fprintf(stderr,  " SIOCGIFFLAGS: %s\n", strerror(errno));
		return;
	}

	if (ifreq.ifr_flags & IFF_UP)
		Sinfo->up = 1;
	else
		Sinfo->up = 0;

	if (ifreq.ifr_flags & IFF_RUNNING)
		Sinfo->running = 1;
	else
		Sinfo->running = 0;
	close(fd);

}

int search(char *kv) {
	current = head;
	if (head == NULL) {
		return 0;
	}

	else {
		while ((current->next != NULL) && (strcmp(current->data, kv) != 0))
			current = current->next;

		if (strcmp(current->data, kv) == 0) {
			return 1;
		} else
			return 0;
	}
}

int DeleteFromInterfaceList(int HASocket) {
	char undel[100];
	current = head;

	if (head == NULL) {

	} else {

		while (current != NULL) {

			Status_info Sinfo;
			StatusFuntion(&Sinfo, current->data);

			if (Sinfo.running == 0) {
				printf("\n****\n");
				printf("LINK DOWN \n");
				if (current == head) {

					if ((current->next) == NULL) {
						Trigger_info Tinfo;
						strcpy(Tinfo.interfaceName, current->data);
						Tinfo.type = 2;
						strcpy(Tinfo.IP, " ");
						//getIP(&sinfo);
						if (send(HASocket, &Tinfo, sizeof(Trigger_info), 0)
								== -1)
							perror("send");
						head = NULL;
					} else {
						strcpy(undel, current->data);

						Trigger_info Tinfo;
						strcpy(Tinfo.interfaceName, current->data);
						Tinfo.type = 2;
						strcpy(Tinfo.IP, " ");
						//getIP(&sinfo);
						if (send(HASocket, &Tinfo, sizeof(Trigger_info), 0)
								== -1)
							perror("send");
						current = current->next;
						free(head);
						head = current;

					}

				} else {
					Trigger_info Tinfo;
					strcpy(Tinfo.interfaceName, current->data);
					Tinfo.type = 2;
					strcpy(Tinfo.IP, " ");
					//getIP(&sinfo);
					if (send(HASocket, &Tinfo, sizeof(Trigger_info), 0) == -1)
						perror("send");
					strcpy(undel, current->data);
					save->next = current->next;
					free(current);

				}
			}
			save = current;
			current = current->next;
		}
	}
}

int AddInterfaceList(char *x) {
	node *temp;
	temp = getnode();
	if (temp == NULL) {
		return -1;
	} else {
		strcpy(temp->data, x);
		temp->next = NULL;

		if (head == NULL)
			head = temp;
		else {
			current = head;
			while (current != NULL) {
				save = current;
				current = current->next;
			}
			save->next = temp;
		}
		return 0;
	}

}

void display() {
	current = head;
	if (current == NULL)
		printf("\aThe List Is Empty!\n");

	while (current != NULL) {
		printf("\n: %s", current->data);
		current = current->next;

	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	head = NULL;
	printf("client: connecting to \n");
	int HASocket = OpenUDC();
	if (HASocket < 0)
		exit(0);

	int iofd = socket(AF_INET, SOCK_DGRAM, 0);
	if (iofd == -1) {
		perror("send");
		exit(0);
	}

	while (1) {
		///////Link Up and Link Down////////////		
		struct ifconf ifc;
		struct ifreq *ifr;
		int fd;
		int interf;
		char Names[100];
		char buff[1024];
		int searchresult = 0;

		ifc.ifc_len = sizeof(buff);
		ifc.ifc_buf = buff;
		if (ioctl(iofd, SIOCGIFCONF, &ifc) < 0) {
			fprintf(stderr, "1SIOCGIFCONF: %s\n", strerror(errno));
			return;
		}
		ifr = ifc.ifc_req;
		Names[0] = '\0';
		interf = ifc.ifc_len / sizeof(struct ifreq);
		for (; interf > 0; ifr++) {
			Status_info einfo;
			sprintf(Names, ifr->ifr_name);
			if (strcmp(Names, "lo") == 0) {
			} else {
				StatusFuntion(&einfo, Names);
				if (einfo.up == 1 && einfo.running == 1) {
					searchresult = search(Names);
					if (searchresult == 1) {
					} else {
						printf("\n****\n");
						printf("LINK UP\n");
						int AddCheck = AddInterfaceList(Names);
						if (AddCheck == -1) {
						} else {
							//AddInterfaceList(Names);
							Trigger_info sinfo;
							sinfo.type = 1;
							strcpy(sinfo.interfaceName, Names);
							getIP(&sinfo, iofd);
							if (send(HASocket, &sinfo, sizeof(Trigger_info), 0)
									== -1)
								perror("send");
						}
					}

					////////////////////link going dow/////////////////
					iwrange range;
					int has_range = 0;
					int skfd;
					Trigger_info sinfoLGR;
					iwstats stats;
					if (iw_get_range_info(iofd, Names, &(range)) >= 0)
						has_range = 1;
					struct wireinfo winfo;
					strcpy(winfo.name, Names);
					if (iw_get_stats(iofd, winfo.name, &stats, &range,
							has_range) >= 0) {
						MakeWireInfo(&winfo, &stats.qual, &range, has_range);
						//printf("%d\n",winfo.link);
						//printf("%d\n",winfo.level);
						//printf("%d\n",winfo.noise);
						//iw_print_stats(temp, sizeof(temp), &stats.qual, &range, has_range);
						//printf("    Link/Cell/AP      : %s\n", temp);
						if (winfo.link < 30 && winfo.link > 10) {
							sinfoLGR.type = 3;
							strcpy(sinfoLGR.interfaceName, winfo.name);
							//strcpy(sinfoLGR.IP," ");
							getIP(&sinfoLGR, iofd);

							if (send(HASocket, &sinfoLGR, sizeof(Trigger_info),
									0) == -1)
								perror("send");
						}
					}
					close(skfd);

				}
			}
			interf--;

		}
		DeleteFromInterfaceList(HASocket);
//display();
		fflush(stdin);
		sleep(5);
	}

	close(iofd);
	close(HASocket);
	return 0;
}

