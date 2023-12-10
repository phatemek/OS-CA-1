#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "./cJSON/cJSON.h"

#define MAX_STRING_LEN 100
#define MAX_LINE 1024
#define MAX_FOOD 100
#define WAIT_TIME 120
#define TCP_ADDRESS "127.0.0.1"
#define UDP_ADDRESS "127.0.0.1"

int UDP_PORT = 0;
int TCP_PORT = 0;
char username[MAX_STRING_LEN];
int udpFd;
int tcpFd;
struct sockaddr_in udpAddress;
struct sockaddr_in tcpAddress;
int foodCount = 0;
int resFd;
int logFd;
struct termios originalTerminos;

typedef struct {
    char foodName[MAX_STRING_LEN];
} MenuFood;

MenuFood menu[MAX_FOOD];

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
    size_t len = strlen(username);
    username[len-1] = '\0';
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
    udpFd = socket(AF_INET, SOCK_DGRAM, 0);
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

void bindSocket() {
    int res = bind(udpFd, (struct sockaddr*)&udpAddress, sizeof(udpAddress));
    if (res < 0) {
        perror("bind");
        close(udpFd);
        exit(1);
    }
}

void udpSigHandler(int signal) {
    char message[MAX_LINE];
    sprintf(message, "Welcome %s as Customer!\n", username);
    write(1, message, strlen(message));
}

int acceptTcpConnection() {
    int connectionFd = -1;
    struct sockaddr_in connectionAddr;
    int len = sizeof(connectionAddr);
    connectionFd = accept(tcpFd, (struct sockaddr *)&connectionAddr, (socklen_t*) &len);
    return connectionFd;
}

void checkUnique(){
    int sock = acceptTcpConnection();
    if (sock != -1){
        perror("username not unique\n");
        exit(1);
    }
}

void broadcastUser() {
    char buffer[MAX_LINE];
    sprintf(buffer, "user %s %d", username, TCP_PORT);
    int res = sendto(udpFd, buffer, strlen(buffer), 0,(struct sockaddr *)&udpAddress, sizeof(udpAddress));
    if (res <= 0){
        perror("sendto udp");
        exit(1);
    }
    signal(SIGALRM, udpSigHandler);
    siginterrupt(SIGALRM, 1);
    alarm(1);
    checkUnique();
    alarm(0);
}

void udpSetup() {
    udpFd = initUDPSocket();
    modifySocket();
    bindSocket();
}

void tcpSetup() {
    tcpFd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpFd < 0) {
        perror("tcp socket failed");
        exit(1);
    }
    int opt = 1;
    if (setsockopt(tcpFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);
    if (bind(tcpFd, (struct sockaddr *)&tcpAddress, sizeof(tcpAddress)) < 0) {
        perror("bind");
        exit(1);
    }
    if (getsockname(tcpFd, (struct sockaddr *)&tcpAddress, &clientLen) != 1) {
        TCP_PORT = ntohs(tcpAddress.sin_port);
    }
    if (listen(tcpFd, 4) < 0) {
        perror("listen");
        exit(1);
    }
}

void showMenu() {
    logEvent("customer: show menu\n");
    char buffer[MAX_LINE];
    for (int i = 0; i < foodCount; i++) {
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%d- %s\n", i+1, menu[i].foodName);
        write(1, buffer, strlen(buffer));
    }
}

typedef struct {
    char foodName[MAX_LINE];
    int resPort;
} FoodReq;

FoodReq getPF() {
    char message[MAX_LINE] = "name of food: ";
    char buffer[MAX_LINE];
    FoodReq foodReq;
    memset(buffer, 0, sizeof(buffer));
    write(1, message, strlen(message));
    read(0, buffer, MAX_LINE);
    size_t len = strlen(buffer);
    buffer[len-1] = '\0';
    memcpy(foodReq.foodName, buffer, sizeof(buffer));
    memset(message, 0, sizeof(message));
    sprintf(message, "port of restaurant: ");
    write(1, message, strlen(message));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, MAX_LINE);
    foodReq.resPort = atoi(buffer);
    char logBuffer[MAX_LINE];
    sprintf(logBuffer, "customer: order food, food name: %s, restaurant port: %d\n", 
    foodReq.foodName, foodReq.resPort);
    logEvent(logBuffer);
    return foodReq;
}

int connectRes(FoodReq foodReq) {
    int resFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in resAddress = initAddr(TCP_ADDRESS, foodReq.resPort);
    if (connect(resFd, (struct sockaddr *)&resAddress, sizeof(resAddress)) < 0) {
        print("Error in connecting to restaurant\n");
    }
    return resFd;
}

int foodInMenu(char* name) {
    for (int i = 0; i < foodCount; i++) {
        if (strcmp(menu[i].foodName, name) == 0) {
            return 1;
        }
    }
    return 0;
}

int sendOrder(int resFd, FoodReq foodReq) {
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    if (!foodInMenu(foodReq.foodName)) {
        logEvent("there was no such food.\n");
        sprintf(buffer, "no such food.\n");
        write(1, buffer, strlen(buffer));
        return 0;
    }
    sprintf(buffer, "custReq username: %s port: %d food: %s", username, TCP_PORT, foodReq.foodName);
    send(resFd, buffer, strlen(buffer), 0);
    return 1;
}

void alarmHandler(int signal) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminos);
    char message[MAX_LINE];
    memset(message, 0, MAX_LINE);
    sprintf(message, "T %d", TCP_PORT);
    send(resFd, message, strlen(message), 0);
    memset(message, 0, MAX_LINE);
    sprintf(message, "Time out!\n");
    write(1, message, strlen(message));
}

struct termios blockTerminal() {
    struct termios originalTermios;
    struct termios modifiedTermios;
    if (tcgetattr(STDIN_FILENO, &originalTermios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }
    modifiedTermios = originalTermios;
    modifiedTermios.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &modifiedTermios) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    return originalTermios;
}

void waitForRes(int resFd) {
    print("waiting for the restaurant's response...\n");
    originalTerminos = blockTerminal();
    signal(SIGALRM, alarmHandler);
    siginterrupt(SIGALRM, 1);
    alarm(WAIT_TIME);
}

void orderFood() {
    FoodReq foodReq = getPF();
    resFd = connectRes(foodReq);
    if(sendOrder(resFd, foodReq)) {
        waitForRes(resFd);
    }
}

void handleTerminal() {
    char buffer[MAX_LINE];
    char message[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    read(0, buffer, MAX_LINE);
    if (strcmp(buffer, "show menu\n") == 0) {
        showMenu();
    } else if (strcmp(buffer, "order food\n") == 0) {
        orderFood();
    }
}

char* jsonToString() {
    int jsonFd = open("recipes.json", O_RDONLY);
    if (jsonFd == -1) {
        perror("Error opening json file");
        return "";
    }

    struct stat fileInfo;
    if (fstat(jsonFd, &fileInfo) == -1) {
        perror("Error getting file size");
        close(jsonFd);
        return "";
    }

    off_t fileSize = fileInfo.st_size;

    char *jsonString = (char *)malloc(fileSize + 1);
    if (read(jsonFd, jsonString, fileSize) == -1) {
        perror("Error reading file");
        close(jsonFd);
        free(jsonString);
        return "";
    }

    jsonString[fileSize] = '\0';

    close(jsonFd);
    return jsonString;
}

void addFood(char* foodName,char* ing,int amount) {
    if (foodCount == 0 || strcmp(menu[foodCount - 1].foodName, foodName) != 0) {
        memcpy(menu[foodCount].foodName, foodName, strlen(foodName));
        foodCount++;
    }
}

void traverseJSON(cJSON *json, const char *parentKey) {
    if (cJSON_IsObject(json)) {
        cJSON *child = json->child;
        while (child) {
            char newKey[MAX_STRING_LEN];
            if (parentKey != NULL) {
                snprintf(newKey, sizeof(newKey), "%s.%s", parentKey, child->string);
            } else {
                snprintf(newKey, sizeof(newKey), "%s", child->string);
            }
            traverseJSON(child, newKey);
            child = child->next;
        }
    } else if (cJSON_IsArray(json)) {
        cJSON *child = json->child;
        int i = 0;
        while (child) {
            char newKey[MAX_STRING_LEN];
            snprintf(newKey, sizeof(newKey), "%s[%d]", parentKey, i);
            traverseJSON(child, newKey);
            child = child->next;
            i++;
        }
    } else {
        char foodName[MAX_LINE], ing[MAX_LINE], amount[MAX_LINE];
        memset(foodName, '\0', MAX_LINE);
        memset(ing, '\0', MAX_LINE);
        int idx = -1;
        for (int i = 0; i < strlen(parentKey); i++){
            if (parentKey[i] == '.'){
                idx = i + 1;
                continue;
            }
            if (idx == -1){
                foodName[i] = parentKey[i];
            }else{
                ing[i - idx] = parentKey[i];
            }
        }
        int a = atoi(cJSON_Print(json));
        addFood(foodName, ing, a);
    }
}

void readJsonFoods(char* jsonString) {
    cJSON *root = cJSON_Parse(jsonString);
    if (!root) {
        char message[MAX_LINE];
        sprintf(message, "JSON parsing error: %s\n", cJSON_GetErrorPtr());
        write(2, message, strlen(message));
        free(jsonString);
        return;
    }
    traverseJSON(root, NULL);
    cJSON_Delete(root);
    free(jsonString);
}

void makeMenu() {
    char* jsonString = jsonToString();
    readJsonFoods(jsonString);
}

int acceptRes() {
    struct sockaddr_in resAddress;
    int addressLen = sizeof(resAddress);
    int resFd = accept(tcpFd, (struct sockaddr *)&resAddress, (socklen_t*) &addressLen);

    return resFd;
}

void connectUser(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = initAddr(TCP_ADDRESS, port);

    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0){
        perror("connect to user\n");
        exit(1);
    }
}

void handleUdp() {
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(udpFd, buffer, MAX_LINE, 0);
    char* token;
    token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    char name[MAX_STRING_LEN];
    memset(name, 0, sizeof(name));
    sprintf(name, "%s", token);
    token = strtok(NULL, " ");
    int port = atoi(token);
    if (strcmp(name, username) == 0) {
        connectUser(port);
    }
}

void handleTCP(char* buffer) {
    char* token;
    token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    char resName[MAX_STRING_LEN];
    memset(resName, 0, sizeof(resName));
    sprintf(resName, "%s", token);
    char message[MAX_LINE];
    memset(message, 0, sizeof(message));
    if (buffer[0] == 'y') {
        sprintf(message, "%s Restaurant accepted and your food is ready!\n", resName);
        logEvent(message);
        write(1, message, strlen(message));
    }
    else if (buffer[0] == 'n') {
        sprintf(message, "%s Restaurant denied.\n", resName);
        logEvent(message);
        write(1, message, strlen(message));
    }
    else {
        print("invalid buffer.\n");
    }
    alarm(0);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminos);
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
                if (i == tcpFd) {
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
                    handleTCP(buffer);
                }
            }
        }
    }
}

void work() {
    fd_set masterSet, workingSet;
    FD_ZERO(&masterSet);
    int maxFd;
    FD_SET(tcpFd, &masterSet);
    FD_SET(0, &masterSet);
    FD_SET(udpFd, &masterSet);
    maxFd = max(tcpFd, udpFd);
    ansReq(workingSet, masterSet, maxFd);
}

void makeLogFile() {
    logFd = open("customer_log.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (logFd == -1) {
        perror("open logFd");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    getArgs(argc, argv);
    makeLogFile();
    greet();
    udpAddress = initAddr(UDP_ADDRESS, UDP_PORT);
    tcpAddress = initAddr(TCP_ADDRESS, TCP_PORT);
    udpSetup();
    tcpSetup();
    broadcastUser();
    makeMenu();
    work();
    return 0;
}
