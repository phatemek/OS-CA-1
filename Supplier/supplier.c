#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_STRING_LEN 100
#define MAX_LINE 1024
#define MAX_ING 1024
#define RES_WAIT_TIME 90
#define TCP_ADDRESS "127.0.0.1"
#define UDP_ADDRESS "127.0.0.1"

int UDP_PORT = 0;
int TCP_PORT = 0;
char username[MAX_STRING_LEN];
int udpFd;
int serverTcpFd;
int resFd;
int requestCount = 0;
struct sockaddr_in tcpAddress;
struct sockaddr_in udpAddress;
int logFd;

int max(int a, int b) {
    return a > b ? a : b;
}

void logEvent(char* str) {
    write(logFd, str, strlen(str));
}

void getArgs(int argc, char* argv[]) {
    if(argc > 1) {
        UDP_PORT = atoi(argv[1]);
    }
    else {
        perror("not enough arguments");
        exit(1);
    }
}

void print(char* str) {
    char message[MAX_LINE];
    sprintf(message, "%s", str);
    write(1, message, strlen(message));
}

void greet() {
    print("Please enter your username: ");
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, MAX_LINE);
    sprintf(username, "%s", buffer);
    char message[MAX_LINE];
    sprintf(message, "Welcome %s as Supplier!\n", username);
    write(1, message, strlen(message));
}

struct sockaddr_in initAddr(char* addr, int port) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(addr);
    address.sin_port = htons(port);
    return address;
}

int initUDPSocket() {
    int udpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpFd < 0) {
        perror("udpFd not initialized");
        exit(1);
    }
    return udpFd;
}

void modifySocket() {
    int broadcast = 1, opt = 1;
    int res = setsockopt(udpFd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    if (res < 0) {
        perror("setsockopt");
        close(udpFd);
        exit(1);
    }
    res = setsockopt(udpFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    if (res < 0) {
        perror("setsockopt");
        close(udpFd);
        exit(1);
    }
}

void bindSocket(){
    int res = bind(udpFd, (struct sockaddr *)&udpAddress, sizeof(udpAddress));
    if (res < 0) {
        perror("bind");
        close(udpFd);
        exit(1);
    }
}

void broadcastUser() {
    char message[MAX_LINE];
    sprintf(message, "%s", username);
    ssize_t res = sendto(udpFd, message, strlen(message), 0,(struct sockaddr *)&udpAddress,
     sizeof(udpAddress));
    if (res < 0) {
        perror("sendto");
        close(udpFd);
        exit(1);
    }
}

void udpSetup() {
    udpFd = initUDPSocket();
    modifySocket();
    bindSocket();
}

void tcpSetup() {
    serverTcpFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverTcpFd < 0) {
        perror("tcp socket failed");
        exit(1);
    }
    int opt = 1;
    if (setsockopt(serverTcpFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);
    if (bind(serverTcpFd, (struct sockaddr *)&tcpAddress, sizeof(tcpAddress)) < 0) {
        perror("bind");
        exit(1);
    }
    if (getsockname(serverTcpFd, (struct sockaddr *)&tcpAddress, &clientLen) != 1) {
        TCP_PORT = ntohs(tcpAddress.sin_port);
    }
    if (listen(serverTcpFd, 4) < 0) {
        perror("listen");
        exit(1);
    }
    char message[MAX_LINE];
    sprintf(message, "tcp port: %d\n", TCP_PORT);
    write(1, message, sizeof(message));
}

int acceptRes() {
    struct sockaddr_in resAddress;
    int addressLen = sizeof(resAddress);
    int resFd = accept(serverTcpFd, (struct sockaddr *)&resAddress, (socklen_t*) &addressLen);

    return resFd;
}

void handleUdp() {
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    int bytesRead = recv(udpFd, buffer, MAX_LINE, 0);
}

void ansResRequest() {
    if (requestCount == 0) {
        print("there are no requests.\n");
        return;
    }
    char message[MAX_LINE] = "your answer (yes/no): ";
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    write(1, message, strlen(message));
    read(0, buffer, MAX_LINE);
    memset(message, 0, sizeof(message));
    if (strcmp(buffer, "yes\n") == 0) {
        logEvent("supplier accepted request.\n");
        sprintf(message, "y%s Supplier accepted!\n", username);
    } else {
        logEvent("supplier denied request.\n");
        sprintf(message, "n%s Supplier denied!\n", username);
    }
    send(resFd, message, strlen(message), 0);
    requestCount = 0;
}

void handleTerminal() {
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    read(0, buffer, MAX_LINE);
    if (strcmp(buffer, "answer request\n") == 0) {
        ansResRequest();
    }
    else {
        print("invalid request.\n");
    }
}

void handleTCP(char* buffer, int currResFd) {
    if (requestCount == 1 && buffer[0] == 'r') {
        send(currResFd, "wait", strlen("wait"), 0);
        return;
    }
    resFd = currResFd;
    if (buffer[0] == 'r') {
        requestCount = 1;
        logEvent("new ingredient request received.\n");
        print("new request ingredient!\n");
    }
    else if (buffer[0] == 'T') {
        logEvent("request timed out.\n");
        requestCount = 0;
    }
    else {
        print("invalid buffer,\n");
    }
}

void ansReq(fd_set workingSet, fd_set masterSet, int maxFd) {
    while(1) {
        workingSet = masterSet;
        select(maxFd + 1, &workingSet, NULL, NULL, NULL);
        
        if (FD_ISSET(0, &workingSet)) {
            handleTerminal();
        }
        else if (FD_ISSET(udpFd, &workingSet)) {
            handleUdp();
        }

        for (int i = 5; i <= maxFd; i++) {
            if (FD_ISSET(i, &workingSet) ) {
                if (i == serverTcpFd) {
                    int newRes = acceptRes();
                    FD_SET(newRes, &masterSet);
                    maxFd = max(newRes, maxFd);
                }
                else {
                    char buffer[MAX_LINE];
                    memset(buffer, 0, MAX_LINE);
                    int bytesReceived = recv(i , buffer, MAX_LINE, 0);
                    if (bytesReceived == 0) {
                        close(i);
                        FD_CLR(i, &masterSet);
                        continue;
                    }
                    handleTCP(buffer, i);
                }
            }
        }
    }
}

void work() {
    fd_set masterSet, workingSet;
    FD_ZERO(&masterSet);
    int maxFd;
    FD_SET(serverTcpFd, &masterSet);
    FD_SET(0, &masterSet);
    FD_SET(udpFd, &masterSet);
    maxFd = max(serverTcpFd, udpFd);
    ansReq(workingSet, masterSet, maxFd);
}

void makeLogFile() {
    logFd = open("supplier-log.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (logFd == -1) {
        perror("open logFd");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    getArgs(argc, argv);
    makeLogFile();
    greet();
    tcpAddress = initAddr(TCP_ADDRESS, TCP_PORT);
    udpAddress = initAddr(UDP_ADDRESS, UDP_PORT);
    udpSetup();
    tcpSetup();
    broadcastUser();
    work();
    return 0;
}