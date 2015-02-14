#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netdb.h>

struct playersRooster {
	int leftId;
	int rightId;
	int id;
	int port;
	char *hostAddr;
	struct playersRooster *leftPlayer;
	struct playersRooster *rightPlayer;
};

struct playersRooster *head, *tail, *currentPlayer;

void push(int playerId, int port,char *addr) {
	struct playersRooster *temp = (struct playersRooster*) malloc(
			sizeof(struct playersRooster));
	temp->id = playerId;
	temp->port = port;
	temp->hostAddr = (char *) malloc(100);
	sprintf(temp->hostAddr,"%s",addr);
	temp->rightPlayer = NULL;
	temp->leftPlayer = NULL;

	if (head == tail && head == NULL) {
		head = tail = temp;
		head->rightPlayer = tail->rightPlayer = NULL;
		head->leftPlayer = tail->leftPlayer = NULL;
	} else {
		tail->rightPlayer = temp;
		temp->leftPlayer = tail;
		tail = temp;
		head->leftPlayer = tail;
		tail->rightPlayer = head;
	}
}

int main(int argc, char *argv[]) {
	int opt = 1;
	int game = 1;
	int master_socket, addrlen, new_socket, activity, i, sd;
	int max_sd;
	struct sockaddr_in address;
	struct hostent *hp, *ihp;
	char host[64], str[64], create[64] = "CreatePort#",tempBuf[25240];
	int masterPort = 0, players = 0, hops = 0, playersConnected = 0;
	int playerPort;
	int playSetup = 0;
	char *playerHost = NULL;
	/**char buf[10240];**/
	char *buf;
	buf = (char *)malloc(25240);
	int playerConnected = 0,gameStarted =0;
	int firstConnected = 0;
	int playerSelected = -1,recvFlag =0;
	char buffer[32];
	int playerPath = -1,offset = 0;
	fd_set readfds;
	fd_set master;

	/* Initialize random seed */

	if (argc < 4) {
			fprintf(stderr, "Usage: %s <port-number> <number-of-players> <hops>\n", argv[0]);
			exit(1);
	}
	masterPort = atoi(argv[1]);
	players = atoi(argv[2]);
	hops = atoi(argv[3]);

	if(players<2){
		printf("Number of players less than 2\n");
		exit(1);
	}
	int client_socket[players], maxSockets = players;

	gethostname(host, sizeof host);
	hp = gethostbyname(host);
	if (hp == NULL) {
		fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
		exit(1);
	}

	for (i = 0; i < maxSockets; i++) {
		client_socket[i] = 0;
	}

	if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &opt,sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(masterPort);
	if (bind(master_socket, (struct sockaddr *) &address, sizeof(address))< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(master_socket, players) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	printf("Potato Master on %s\n", host);
	printf("Players = %d\n", players);
	printf("Hops = %d\n", hops);

	addrlen = sizeof(address);
	max_sd = master_socket;
	FD_SET(master_socket, &readfds);
	i = 0;
	while (game) {
		FD_ZERO(&master);
		master = readfds;

		for (i = 0; i < maxSockets; i++) {
			sd = client_socket[i];
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > max_sd)
				max_sd = sd;
		}

		activity = select(max_sd + 1, &master, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR)) {
			perror("select error");
			exit(1);
		}

		if (playersConnected < players) {
			if (FD_ISSET(master_socket, &master)) {
				if ((new_socket = accept(master_socket,(struct sockaddr *) &address, (socklen_t*) &addrlen))< 0) {
					perror("accept");
					exit(EXIT_FAILURE);
				}
				ihp = gethostbyaddr((char *) &address.sin_addr,sizeof(struct in_addr), AF_INET);
				FD_SET(new_socket,&readfds);
				/*printf("Player address %s\n",address.sin_addr);*/
				playerHost = ihp->h_name;
				char clientAddr[512];
				sprintf(clientAddr,"%d%s%d%s%d%s%d",address.sin_addr.s_addr&0xFF,".",(address.sin_addr.s_addr&0xFF00)>>8,".",(address.sin_addr.s_addr&0xFF0000)>>16,".",(address.sin_addr.s_addr&0xFF000000)>>24);
				if(new_socket > max_sd)
					max_sd = new_socket;
				printf("player %d is on %s\n", playersConnected, playerHost);
				playerPort = ntohs(address.sin_port);
				sprintf(str, "%s%s%s%d%s%s", "INIT#",playerHost,"#", 5, "#","!");
				/*printf("Sending to player %s\n",str);*/
				int len = send(new_socket, str, strlen(str), 0);
				if (len != strlen(str)) {
					perror("send");
					exit(1);
				}
				while(1){
					len = recv(new_socket, buf, 32, 0);
					if (len < 0) {
						perror("recv");
						exit(1);
					}
					buf[len] = '\0';
					if (len > 0) {
						/*printf("Buffer recv %s\n",buf);*/
						strcpy(tempBuf, buf);
						char *bufPtr;
						bufPtr = strtok(tempBuf,"#");
						/*printf("bufptr %s\n",bufPtr);*/
						if (!strcmp("set", bufPtr)) {
							playerPort = atoi(strtok(NULL,"#"));
							/*printf("Player port %d\n",playerPort);*/
							break;
						}
					}
				}
				push(playersConnected, playerPort,playerHost);
				playersConnected++;

				if (playersConnected == players) {
					playSetup = 1;
				}
				for (i = 0; i < maxSockets; i++) {
					if (client_socket[i] == 0) {
						client_socket[i] = new_socket;
						break;
					}
				}
			}
		}

		if (playSetup == 1) {
			struct playersRooster *temp = head;
			for (i = 0; i < maxSockets; i++) {
				sd = client_socket[i];
				int len;
				if (sd > 0) {
					if (i == 0)
						sprintf(str, "%s%s%s%d%s%d%s%d%s%d%s%d%s%s", "FIRST#",temp->leftPlayer->hostAddr,"#", temp->port, "#",temp->leftPlayer->port,"#",temp->id,"#",temp->leftPlayer->id,"#",temp->rightPlayer->id,"#","!");
					else
						sprintf(str, "%s%s%s%d%s%d%s%d%s%d%s%d%s%s", create,temp->leftPlayer->hostAddr,"#", temp->port, "#",temp->leftPlayer->port,"#",temp->id,"#",temp->leftPlayer->id,"#",temp->rightPlayer->id,"#","!");
					/*printf(">>> %s\n",str);*/
					temp = temp->rightPlayer;
					/*printf("Sending ->> %s\n",str);*/
					len = send(sd, str, strlen(str), 0);
					if (len != strlen(str)) {
						perror("send");
						exit(1);
					}
					sleep(1);
				}
			}
			playSetup = 2;
		}

		for (i = 0; i < maxSockets; i++) {
			sd = client_socket[i];
			int len;
			if (FD_ISSET(sd, &master)) {
				recvFlag = 0;
				memset(buf, 0, sizeof(buf));
				/*printf("Data set \n");*/
				do {
					memset(buffer, 0, sizeof(buffer));
					len = recv(sd, buffer, 32, 0);
					if (len > 0) {
						buffer[len] = '\0';
						/*printf(">>> before memcpy >>  %s\n",buf);
						printf(">>> buffer >> %s\n",buffer);
						printf("Offset >>> %d\n",offset);*/
						offset = offset + len;
						if(offset >= 50000){
							printf("Buffer size exceeded\n");
							exit(1);
						}
						if(offset == 0)
							sprintf(buf, "%s", buffer);
						else
							sprintf(buf, "%s%s", buf, buffer);
						if(strstr(buf,"!") != NULL) {
							recvFlag = 1;
							break;
						}
						/*memcpy(buf + offset,buffer,sizeof(buffer));*/
						/*printf(">>> after memcpy >>  %s\n",buf);
						printf(">>> offset >>  %d\n",offset);*/
						/*char c = buffer[31];
						if (len == 32 && c != '!') {

						} else {
							recvFlag = 1;

							offset=0;
							break;
						}*/
					} else if (len <= 0) {
						if (errno != EINTR) {
							perror("recv Error");
							exit(1);
						} else if (len == 0) {
							perror("recv closed");
							exit(1);
						}
					}
				} while (len > 0);

				if(recvFlag){
					memset(tempBuf, 0, sizeof(tempBuf));
					strcpy(tempBuf, buf);

					if (!strcmp("Connected!", buf)) {
						playerConnected++;
						break;
					}
					char *bufPtr;
					bufPtr = strtok(NULL, "$");
					/*printf("After $ >> %s\n",bufPtr);*/
					bufPtr = strtok(tempBuf, "#");
					/*printf("ptr >> %s\n",bufPtr);*/
					if (bufPtr != NULL && !strcmp(bufPtr, "Message")) {
						printf("Trace of potato:\n");
						bufPtr = strtok(NULL, "#");
						/*printf("First player >> %s\n",bufPtr);*/
						while (bufPtr != NULL) {
							playerPath = atoi(bufPtr);
							printf("%d",playerPath);
							bufPtr = strtok(NULL, "#");
							if(bufPtr != NULL)
								printf("%s",",");
						}
						printf("\n");
						game = 2;
					}
				}
			}
		}

		if(game == 2){
			sprintf(str,"%s","close");
			for (i = 0; i < maxSockets; i++) {
				sd = client_socket[i];
				int len = send(sd, str, strlen(str), 0);
				if (len != strlen(str)) {
					perror("send");
					exit(1);
				}
			}
			sleep(2);
			exit(1);
		}

		if (playerConnected == players - 1 && firstConnected == 0) {
			sd = client_socket[0];
			sprintf(str, "%s", "CONNECT");
			int len = send(sd, str, strlen(str), 0);
			if (len != strlen(str)) {
				perror("send");
				exit(1);
			}
			firstConnected = 1;
		}

		if(playerConnected == players && gameStarted == 0){
			/*printf("Lets begin the game\n");*/
			srand (time(NULL));
			gameStarted = 1;
			playerSelected = rand()%players;
		}

		if(gameStarted == 1){
			/*printf("%d\n",playerSelected);*/
			struct playersRooster *temp = head;
			int playerdId = -1;
			i = 0;
			while(i < players){
				playerdId = temp->id;
				if(playerdId == playerSelected)
					break;
				temp = temp->rightPlayer;
				i++;
			}
			sd = client_socket[playerdId];
			if(hops > 0){
				sprintf(str,"%s%s%d%s%s","Start","#",hops,"#","!");
				printf("All players present, sending potato to player %d\n",playerdId);
				int len = send(sd, str, strlen(str), 0);
				if (len != strlen(str)) {
					perror("send");
					exit(1);
				}
			}else{
				printf("All players present, sending potato to player %s\n","Nobody");
				printf("Trace of potato:\n");
				sprintf(str,"%s","close");
				for (i = 0; i < maxSockets; i++) {
					sd = client_socket[i];
					int len = send(sd, str, strlen(str), 0);
					if (len != strlen(str)) {
						perror("send");
						exit(1);
					}
				}
				sleep(1);
				exit(1);
			}
			gameStarted = -1;
		}
	}

	return 0;
}
