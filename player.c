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

int main(int argc, char *argv[]) {
	int opt = 1;
	int leftPlayer, rightAccpet, rightPlayer;
	int leftPlayerPort, rightPlayerPort;
	int myId, leftId, rightId;
	int s, rc, port, i = 0, len, rightBind, rightListen, sd;
	int firstPlayer = 0;
	char host[64];
	int recvFlag = 0;
	struct hostent *hp, *rightHost, *leftHost;
	struct sockaddr_in sin;
	char buf[25240], tempBuf[25240], str[25240];
	int playerSockets[3], activity;
	socklen_t addrlen;
	struct sockaddr_in rightSock, leftSock, rightIncoming;
	int fdmax;
	char *hostAddr;
	int hops = -1, cantBind = 1;
	int playerId = -1;
	int playerSelected;
	fd_set master;
	fd_set readfds;
	int offset = 0;
	srand(time(NULL));
	for (i = 0; i < 3; i++) {
		playerSockets[i] = 0;
	}

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <master-machine-name> <port-number>\n", argv[0]);
		exit(1);
	}

	hp = gethostbyname(argv[1]);
	if (hp == NULL) {
		fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
		exit(1);
	}
	port = atoi(argv[2]);
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		/*perror("socket:");*/
		exit(s);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);

	rc = connect(s, (struct sockaddr *) &sin, sizeof(sin));
	if (rc < 0) {
		/*perror("connect:");*/
		exit(rc);
	}

	/* master added */
	playerSockets[0] = s;
	FD_SET(s, &master);
	fdmax = s;
	char buffer[32];

	while (1) {
		/* To recv a large packet */
		do {
			memset(buffer, 0, sizeof(buffer));
			len = recv(s, buffer, 32, 0);
			if (len > 0) {
				sprintf(buf, "%s%s", buf, buffer);
				char c = buffer[31];
				if (len == 32 && c != '!') {
				} else {
					recvFlag = 1;
					buf[sizeof(buf)] = '\0';
					break;
				}
			} else if (len <= 0) {
				/*perror("recv");*/
				exit(1);
			}
		} while (len > 0);

		/* Ends --- To recv a large packet */
		if (recvFlag) {
			strcpy(tempBuf, buf);
			char *bufPtr;
			bufPtr = strtok(tempBuf, "#");
			if (bufPtr != NULL && !strcmp(bufPtr, "INIT")) {
				firstPlayer = 1;
				hostAddr = strtok(NULL, "#");
				rightPlayerPort = atoi(strtok(NULL, "#"));
				/*leftPlayerPort = atoi(strtok(NULL,"#"));
				 myId = atoi(strtok(NULL,"#"));
				 leftId = atoi(strtok(NULL,"#"));
				 rightId = atoi(strtok(NULL,"#"));*/
				break;
			} else if (bufPtr != NULL && !strcmp(bufPtr, "CreatePort")) {
				firstPlayer = 0;
				hostAddr = strtok(NULL, "#");
				rightPlayerPort = atoi(strtok(NULL, "#"));
				/*leftPlayerPort = atoi(strtok(NULL,"#"));
				 myId = atoi(strtok(NULL,"#"));
				 leftId = atoi(strtok(NULL,"#"));
				 rightId = atoi(strtok(NULL,"#"));*/
				break;
			}
		}
	}

	/*Creating connection for Right Player */
	gethostname(host, sizeof host);
	rightHost = gethostbyname(host);

	rightPlayer = socket(AF_INET, SOCK_STREAM, 0);
	if (rightPlayer < 0) {
		/*perror("rightPlayer socket");*/
		exit(rightPlayer);
	}

	if (setsockopt(rightPlayer, SOL_SOCKET, SO_REUSEPORT, (char *) &opt,
			sizeof(opt)) < 0) {
		/*perror("setsockopt");*/
		exit(EXIT_FAILURE);
	}

	rightSock.sin_family = AF_INET;
	rightSock.sin_addr.s_addr = INADDR_ANY;
	rightSock.sin_port = htons(rightPlayerPort);
	/*printf("For port %d\n", rightPlayerPort);*/
	memcpy(&rightSock.sin_addr, rightHost->h_addr_list[0], rightHost->h_length);

	while (cantBind) {
		rightBind = bind(rightPlayer, (struct sockaddr *) &rightSock,
				sizeof(rightSock));
		/*printf("Right bind %d\n", rightBind);*/
		if (rightBind < 0) {
			/*perror("right bind:");*/
			rightPlayerPort = (rand() % (20000 - 10000)) + 10000;
			/*printf("New Port %d\n", rightPlayerPort);*/
			rightSock.sin_port = htons(rightPlayerPort);
			/*exit(rightBind);*/
		} else {
			/*printf("Sending to Master\n");*/
			sprintf(str, "%s%s%d%s%s", "set", "#", rightPlayerPort, "#","!");
			len = send(s, str, strlen(str), 0);
			if (len != strlen(str)) {
				/*perror("send");*/
				exit(1);
			}
			cantBind = 0;
			break;
		}
	}

	/*printf("bind done, ready for the ring\n");*/
	memset(buf, 0, sizeof(buf));
	memset(tempBuf, 0, sizeof(tempBuf));
	while (1) {
		/* To recv a large packet */
		do {
			memset(buffer, 0, sizeof(buffer));
			len = recv(s, buffer, 32, 0);
			if (len > 0) {
				buffer[len] = '\0';
				sprintf(buf, "%s%s", buf, buffer);
				char c = buffer[31];
				if (len == 32 && c != '!') {
				} else {
					recvFlag = 1;
					buf[sizeof(buf)] = '\0';
					break;
				}
			} else if (len <= 0) {
				/*perror("recv");*/
				exit(1);
			}
		} while (len > 0);

		/* Ends --- To recv a large packet */
		if (recvFlag) {
			/*printf("Message %s\n",buf);*/
			strcpy(tempBuf, buf);
			char *bufPtr;
			bufPtr = strtok(tempBuf, "#");
			if (bufPtr != NULL && !strcmp(bufPtr, "FIRST")) {
				firstPlayer = 1;
				hostAddr = strtok(NULL, "#");
				rightPlayerPort = atoi(strtok(NULL, "#"));
				leftPlayerPort = atoi(strtok(NULL, "#"));
				myId = atoi(strtok(NULL, "#"));
				leftId = atoi(strtok(NULL, "#"));
				rightId = atoi(strtok(NULL, "#"));
				break;
			} else if (bufPtr != NULL && !strcmp(bufPtr, "CreatePort")) {
				firstPlayer = 0;
				hostAddr = strtok(NULL, "#");
				rightPlayerPort = atoi(strtok(NULL, "#"));
				leftPlayerPort = atoi(strtok(NULL, "#"));
				myId = atoi(strtok(NULL, "#"));
				leftId = atoi(strtok(NULL, "#"));
				rightId = atoi(strtok(NULL, "#"));
				break;
			}
		}
	}
	printf("Connected as player %d\n", myId);

	rightListen = listen(rightPlayer, 1);
	if (rightListen < 0) {
		/*perror("leftPlayer listen");*/
		exit(rightListen);
	}

	/* Connecting to left player */
	leftPlayer = socket(AF_INET, SOCK_STREAM, 0);
	if (leftPlayer < 0) {
		perror("socket:");
		exit(leftPlayer);
	}

	leftSock.sin_family = AF_INET;
	leftSock.sin_port = htons(leftPlayerPort);
	/*printf("Host %s\n",hostAddr);
	printf("leftPlayerPort %d\n",leftPlayerPort);*/
	leftHost = gethostbyname(hostAddr);
	if (leftHost == NULL) {
		fprintf(stderr, "host not found (%s)\n", hostAddr);
		exit(1);
	}
	memcpy(&leftSock.sin_addr, leftHost->h_addr_list[0], leftHost->h_length);

	if (firstPlayer) {
		addrlen = sizeof(rightSock);
		rightAccpet = accept(rightPlayer, (struct sockaddr *) &rightIncoming,&addrlen);
		if (rightAccpet < -1) {
			/*perror("left accept");*/
			exit(rightAccpet);
		} else {
			playerSockets[1] = rightAccpet;
			FD_SET(rightAccpet, &master);
		}
		while (1) {
			len = recv(s, buf, 32, 0);
			if (len < 0) {
				/*perror("recv");*/
				exit(1);
			}
			buf[len] = '\0';
			if (!strcmp("close", buf))
				break;
			if (len > 0) {
				if (!strcmp("CONNECT", buf)) {
					break;
				}
			}
		}
		rc = connect(leftPlayer, (struct sockaddr *) &leftSock,
				sizeof(leftSock));
		if (rc < 0) {
			/*perror("left connect:");*/
			exit(rc);
		} else {
			sprintf(str, "%s%s", "Connected","!");
			len = send(s, str, strlen(str), 0);
			if (len != strlen(str)) {
				perror("send");
				exit(1);
			}
			playerSockets[2] = leftPlayer;
			FD_SET(leftPlayer, &master);
		}
		if (rightAccpet > fdmax)
			fdmax = rightAccpet;
	} else {
		rc = connect(leftPlayer, (struct sockaddr *) &leftSock,
				sizeof(leftSock));
		if (rc < 0) {
			perror("left connect:");
			exit(rc);
		} else {
			sprintf(str, "%s%s", "Connected","!");
			len = send(s, str, strlen(str), 0);
			if (len != strlen(str)) {
				perror("send");
				exit(1);
			}
			playerSockets[2] = leftPlayer;
			FD_SET(leftPlayer, &master);
		}
		addrlen = sizeof(rightSock);
		rightAccpet = accept(rightPlayer, (struct sockaddr *) &rightIncoming,&addrlen);
		if (rightAccpet < -1) {
			perror("left accept");
			exit(rightAccpet);
		} else {
			playerSockets[1] = rightAccpet;
			FD_SET(rightAccpet, &master);
		}
		if (rightAccpet > fdmax)
			fdmax = rightAccpet;
	}

	/*printf("All Set, ready for the game\n");*/
	while (1) {
		FD_ZERO(&readfds);
		readfds = master;

		activity = select(fdmax + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR)) {
			perror("select error");
			exit(1);
		}
		for (i = 0; i < 3; i++) {
			sd = playerSockets[i];
			if (FD_ISSET(sd, &readfds)) {
				recvFlag = 0;
				offset = 0;
				memset(buf, 0, sizeof(buf));
				do {
					memset(buffer, 0, sizeof(buffer));
					len = recv(sd, buffer, 32, 0);
					if (len > 0) {
						buffer[len] = '\0';
						sprintf(buf, "%s%s", buf, buffer);
						char c = buffer[31];
						offset = offset + len;
						if(strstr(buf,"!") != NULL) {
							recvFlag = 1;
							buf[sizeof(buf)] = '\0';
							break;
						}
						/*if (len == 32 && c != '!') {

						} else {
							recvFlag = 1;
							buf[sizeof(buf)] = '\0';
							break;
						}*/
					} else if (len <= 0) {
						if (errno != EINTR) {
							/*perror("recv Error");*/
							exit(1);
						} else if (len == 0) {
							/*perror("recv closed");*/
							exit(1);
						}
					}
				} while (len > 0);

				if (recvFlag) {
					/*printf("INCOMING %s\n", buf);*/
					memset(tempBuf, 0, sizeof(tempBuf));
					memset(str, 0, sizeof(str));
					strcpy(tempBuf, buf);
					char *bufPtr;
					char *path;
					char *trunc = strtok(tempBuf, "!");
					if (!strcmp("close", buf)) {
						exit(1);
					}
					bufPtr = strtok(trunc, "#");
					if (!strcmp("Start", bufPtr)) {
						hops = atoi(strtok(NULL, "#"));
						hops = hops - 1;
						if (hops == 0) {
							printf("I'm it\n");
							path = strtok(NULL, "$");
							/*printf("%s\n", path);*/
							sprintf(str, "%s%d%s%s", "Message#", myId, "$","!");
							len = send(s, str, strlen(str), 0);
							if (len != strlen(str)) {
								perror("send");
								exit(1);
							}
							break;
						}
						int tempRand = rand() % 2;
						if (tempRand == 0) {
							playerSelected = rightAccpet;
							playerId = rightId;
						} else {
							playerSelected = leftPlayer;
							playerId = leftId;
						}
						sprintf(str, "%s%s%d%s%d%s%s", "Hops", "#", hops, "#",myId, "$", "!");
						/*printf(">> first >> %s\n",str);*/
						printf("Sending potato to %d\n", playerId);
						len = send(playerSelected, str, strlen(str), 0);
						if (len != strlen(str)) {
							perror("send");
							exit(1);
						}
					} else if (!strcmp("Hops", bufPtr)) {
						/*      Hops#9999#1$!        */
						hops = atoi(strtok(NULL, "#"));
						hops = hops - 1;
						if (hops == 0) {
							printf("I'm it\n");
							path = strtok(NULL, "$");
							/*sprintf(path, "%s%s%d", path, "#", myId);*/
							sprintf(str, "%s%s%s%d%s%s", "Message#", path, "#", myId, "$","!");
							/*printf("%s\n", str);
							printf("Size of last %d\n",strlen(str));*/
							len = send(s, str, strlen(str), 0);
							if (len != strlen(str)) {
								perror("send");
								exit(1);
							}
							break;
						}
						int tempRand = rand() % 2;
						if (tempRand == 0) {
							playerSelected = rightAccpet;
							playerId = rightId;
						} else {
							playerSelected = leftPlayer;
							playerId = leftId;
						}
						printf("Sending potato to %d\n", playerId);
						path = strtok(NULL, "$");
						/*sprintf(path, "%s%s%d", path, "#", myId);*/
						sprintf(str, "%s%d%s%s%s%d%s%s", "Hops#", hops, "#", path, "#", myId,"$", "!");
						len = send(playerSelected, str, strlen(str), 0);
						/*printf(">> \tOffset %d \thops %d \tlenght %d\n",offset,hops,len);
						if(offset >= len){
							printf("\t\tOffset greater\n");
						}*/
						if (len != strlen(str)) {
							perror("send");
							exit(1);
						}
					}
				}
			}
		}
	}
}
