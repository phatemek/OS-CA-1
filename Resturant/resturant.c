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
#define MAX_ING 1024
#define MAX_FOOD 100
#define MAX_ORDER 100
#define WAIT_TIME 90
#define TCP_ADDRESS "127.0.0.1"
#define UDP_ADDRESS "127.0.0.1"
#define OPEN_SGN "resOpen"
#define CLOSE_SGN "resClose"
#define WAITING_STATUS 1
#define TIME_OUT_STATUS 2
#define ACCEPTED_STATUS 3
#define DENIED_STATUS 4


int OPEN = 1;
int UDP_PORT = 0;
int TCP_PORT = 0;
char username[MAX_STRING_LEN];
int udpFd;
int serverTcpFd;
struct sockaddr_in tcpAddress;
struct sockaddr_in udpAddress;
int foodCount = 0;
int ingCount = 0;
int timeOut = 0;
int orderCount = 0;
int logFd;

typedef struct {
    char name[MAX_STRING_LEN];
    int amount;
} Ingredient;

typedef struct {
    char name[MAX_STRING_LEN];
    int ingCount;
    Ingredient ingredients[MAX_ING];
} Food;

typedef struct {
    char username[MAX_STRING_LEN];
    int custPort;
    char foodName[MAX_STRING_LEN];
    int status;
} Order;

Food foods[MAX_FOOD];
Ingredient ingredients[MAX_ING];
Order orders[MAX_ORDER];

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
    char message[MAX_LINE];
    sprintf(message, "Welcome %s as Restaurant!\n", username);
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
        perror("setsockopt");
        close(udpFd);
        exit(1);
    }
}

void broadcastUser(char* status) {
    char message[MAX_LINE];
    sprintf(message, "%s %s", status, username);
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
    write(1, message, strlen(message));
}

int acceptCustomer(int serverTcpFd) {
    struct sockaddr_in custAddress;
    int addressLen = sizeof(custAddress);
    
    int custFd = accept(serverTcpFd, (struct sockaddr *)&custAddress, (socklen_t*) &addressLen);
    print("Client connected!\n");

    return custFd;
}

void startWorking() {
    if (OPEN) {
        print("resturant is already open.\n");
        return;
    }
    logEvent("started working.\n");
    OPEN = 1;
    broadcastUser(OPEN_SGN);
}

int waitingReqsCount() {
    int res = 0;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == WAITING_STATUS) {
            res++;
        }
    }
    return res;
}

void stopWorking() {
    if (waitingReqsCount() != 0) {
        print("there are pending requests.\n");
        return;
    }
    logEvent("stopped working.\n");
    OPEN = 0;
    broadcastUser(CLOSE_SGN);
}

void showRecipes() {
    logEvent("restaurant: show recipes\n");
    char buffer[MAX_LINE];
    for (int i = 0; i < foodCount; i++) {
        memset(buffer, 0, sizeof(buffer));
        sprintf(buffer, "%d- %s:\n", i+1, foods[i].name);
        write(1, buffer, strlen(buffer));
        for (int j = 0; j < foods[i].ingCount; j++) {
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "\t%s: %d\n", foods[i].ingredients[j].name, foods[i].ingredients[j].amount);
            write(1, buffer, strlen(buffer));
        }
    }
}

typedef struct {
    int supplierPort;
    char ingredient[MAX_LINE];
    int amount;
} IngReq;

IngReq getPIA() {
    char buffer[MAX_LINE];
    IngReq ingReq;
    memset(ingReq.ingredient, 0, sizeof(ingReq.ingredient));
    memset(buffer, 0, sizeof(buffer));
    char message[MAX_LINE] = "port of supplier: ";
    write(1, message, strlen(message));
    read(0, buffer, MAX_LINE);
    ingReq.supplierPort = atoi(buffer);
    memset(buffer, 0, sizeof(buffer));
    write(1, "name of ingredient: ", strlen("name of ingredient: "));
    ssize_t bytesRead = read(0, buffer, MAX_LINE);
    if (bytesRead > 0) {
        buffer[bytesRead - 1] = '\0';
    }
    memcpy(ingReq.ingredient, buffer, sizeof(buffer));
    memset(buffer, 0, sizeof(buffer));
    write(1, "amount of ingredient: ", strlen("amount of ingredient: "));
    read(0, buffer, MAX_LINE);
    ingReq.amount = atoi(buffer);
    char logBuffer[MAX_LINE];
    sprintf(logBuffer, "requested ingredient: %s, amount: %d, restaurant port: %d\n",  
    ingReq.ingredient, ingReq.amount, ingReq.supplierPort);
    logEvent(logBuffer);
    return ingReq;
}

int connectSupplier(IngReq ingreq) {
    int suppFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in supplierAddress = initAddr(TCP_ADDRESS, ingreq.supplierPort);
    if (connect(suppFd, (struct sockaddr *)&supplierAddress, sizeof(supplierAddress)) < 0) {
        print("Error in connecting to supplier\n");
    }
    return suppFd;
}

void sendReq(int suppFd, IngReq ingreq) {
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "resReq ing: %s amount: %d", ingreq.ingredient, ingreq.amount);
    send(suppFd, buffer, strlen(buffer), 0);
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

int exists(char* newIng) {
    for (int i = 0; i < ingCount; i++) {
        if (strcmp(newIng, ingredients[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

void addIng(IngReq ingreq) {
    int idx = exists(ingreq.ingredient);
    if (idx == -1) {
        memcpy(ingredients[ingCount].name, ingreq.ingredient, strlen(ingreq.ingredient));
        ingredients[ingCount].amount = ingreq.amount;
        ingCount++;
    }
    else {
        ingredients[idx].amount += ingreq.amount;
    }
}

void handleIng(char* buffer, IngReq ingreq) {
    if (buffer[0] == 'y') {
        addIng(ingreq);
    }
}

void alarmHandler(int signal) {
    timeOut = 1;
}

void reqIng() {
    IngReq ingreq = getPIA();
    int suppFd = connectSupplier(ingreq);
    sendReq(suppFd, ingreq);
    print("waiting for the supplier's response...\n");
    signal(SIGALRM, alarmHandler);
    siginterrupt(SIGALRM, 1);
    struct termios originalTerminos = blockTerminal();
    alarm(WAIT_TIME);
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    int bytesReceived = recv(suppFd , buffer, MAX_LINE, 0);
    if (buffer[0] == 'w') {
        logEvent("Supplier is occupied.\n");
        print("Supplier is occupied.\n");
    } else if (timeOut) {
        send(suppFd, "Time out", strlen("Time out"), 0);
        logEvent("Time out!\n");
        print("Time out!\n");
        timeOut = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminos);
        return;
    }
    alarm(0);
    if (bytesReceived == 0) {
        logEvent("Supplier closed!\n");
        print("Supplier closed!\n");
    } else {
        write(1, buffer+1, strlen(buffer+1));
    }
    handleIng(buffer, ingreq);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminos);
}

void showIngredients() {
    logEvent("restaurant: show ingredients\n");
    print("ingredient / amount\n");
    for (int i = 0; i < ingCount; i++) {
        if (ingredients[i].amount != 0) {
            char message[MAX_LINE];
            sprintf(message, "%s / %d\n", ingredients[i].name, ingredients[i].amount);
            write(1, message, strlen(message));
        }
    }
}

void showOrderList() {
    logEvent("restaurant: show order list\n");
    char buffer[MAX_LINE];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "username / port / order\n");
    write(1, buffer, strlen(buffer));
    for (int i = 0; i < orderCount; i++) {
        Order currOrder = orders[i];
        if (currOrder.status == WAITING_STATUS) {
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "%s %d %s\n", currOrder.username, currOrder.custPort, currOrder.foodName);
            write(1, buffer, strlen(buffer));
        }
    }
}

int connectCust(int custPort) {
    int custFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in custAddress = initAddr(TCP_ADDRESS, custPort);
    if (connect(custFd, (struct sockaddr *)&custAddress, sizeof(custAddress)) < 0) {
        print("Error in connecting to customer\n");
    }
    return custFd;
}

int enoughIng(Ingredient ingredient) {
    for (int i = 0; i < ingCount; i++) {
        if (strcmp(ingredient.name, ingredients[i].name) == 0) {
            if (ingredients[i].amount >= ingredient.amount) {
                return 1;
            }
        }
    }
    return 0;
}

int isPossible(int custPort) {
    Order custOrder;
    for (int i = 0; i < orderCount; i++) {
        if (custPort == orders[i].custPort) {
            custOrder = orders[i];
            break;
        }
    }
    Food custFood;
    for (int j = 0; j < foodCount; j++) {
        if (strcmp(foods[j].name, custOrder.foodName) == 0) {
            custFood = foods[j];
            break;
        }
    }
    for (int k = 0; k < custFood.ingCount; k++) {
        if (!enoughIng(custFood.ingredients[k])) {
            return 0;
        }
    }
    return 1;
}

void decIng(Ingredient ing) {
    for (int i = 0; i < ingCount; i++) {
        if (strcmp(ing.name, ingredients[i].name) == 0) {
            ingredients[i].amount -= ing.amount;
        }
    }
}

void handleIngDec(int custPort) {
    Order custOrder;
    for (int i = 0; i < orderCount; i++) {
        if (custPort == orders[i].custPort) {
            custOrder = orders[i];
            break;
        }
    }
    Food custFood;
    for (int j = 0; j < foodCount; j++) {
        if (foods[j].name == custOrder.foodName) {
            custFood = foods[j];
            break;
        }
    }
    for (int k = 0; k < custFood.ingCount; k++) {
        decIng(custFood.ingredients[k]);
    }
}

void setOrderStatus(int custPort, int status) {
    for (int i = 0; i < orderCount; i++) {
        if (custPort == orders[i].custPort) {
            orders[i].status = status;
        }
    }
}

void sendAnswer(char *answer, int custPort) {
    char message[MAX_LINE];
    memset(message, 0, sizeof(message));
    int custFd = connectCust(custPort);
    if (strcmp(answer, "yes\n") == 0 && isPossible(custPort)) {
        sprintf(message, "y %s", username);
        handleIngDec(custPort);
        setOrderStatus(custPort, ACCEPTED_STATUS);
    }
    else {
        sprintf(message, "n %s", username);
        setOrderStatus(custPort, DENIED_STATUS);
    }
    send(custFd, message, strlen(message), 0);
}

int checkOrder(int custPort) {
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].custPort == custPort && orders[i].status == WAITING_STATUS) {
            return 1;
        }
    }
    return 0;
}

void answerOrder() {
    int custPort;
    char buffer[MAX_LINE];
    char message[MAX_LINE] = "port of request: ";
    write(1, message, strlen(message));
    memset(buffer, 0, sizeof(buffer));
    read(0, buffer, MAX_LINE);
    custPort = atoi(buffer);
    int res = checkOrder(custPort);
    if (res) {
        memset(message, 0, sizeof(message));
        sprintf(message, "your answer (yes/no): ");
        write(1, message, strlen(message));
        memset(buffer, 0, sizeof(buffer));
        read(0, buffer, MAX_LINE);
        char logBuffer[MAX_LINE];
        char answer[MAX_STRING_LEN];
        sprintf(answer, "%s", buffer);
        size_t len = strlen(answer);
        answer[len-1] = '\0';
        sprintf(logBuffer, "answered to order: %s, port of order: %d\n", answer, custPort);
        logEvent(logBuffer);
        sendAnswer(buffer, custPort);
    }
    else {
        memset(message, 0, sizeof(message));
        sprintf(message, "no waiting requests with this port.\n");
        write(1, message, strlen(message));
    }
}

void showSalesHistory() {
    logEvent("restaurant: show sales history\n");
    char message[MAX_LINE];
    memset(message, 0, sizeof(message));
    sprintf(message, "username / order / result\n");
    write(1, message, strlen(message));
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status != WAITING_STATUS) {
            char status[MAX_STRING_LEN];
            if (orders[i].status == ACCEPTED_STATUS) {
                sprintf(status, "accepted");
            } else if (orders[i].status == DENIED_STATUS) {
                sprintf(status, "denied");
            } else if (orders[i].status == TIME_OUT_STATUS) {
                sprintf(status, "timeout");
            }
            sprintf(message, "%s %s %s\n", orders[i].username, orders[i].foodName, status);
            write(1, message, strlen(message));
        }
    }
}

void handleTerminal() {
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    read(0, buffer, MAX_LINE);
    if (OPEN == 0 && strcmp(buffer, "start working\n") != 0) {
        print("resturant is closed.\n");
        return;
    }
    if (strcmp(buffer, "start working\n") == 0) {
        startWorking();
    } else if (strcmp(buffer, "break\n") == 0) {
        stopWorking();
    } else if (strcmp(buffer, "show recipes\n") == 0) {
        showRecipes();
    } else if (strcmp(buffer, "request ingredient\n") == 0) {
        reqIng();
    } else if (strcmp(buffer, "show ingredients\n") == 0) {
        showIngredients();
    } else if (strcmp(buffer, "show requests list\n") == 0) {
        showOrderList();
    } else if(strcmp(buffer, "answer request\n") == 0) {
        answerOrder();
    } else if(strcmp(buffer, "show sales history\n") == 0) {
        showSalesHistory();
    } else {
        print("invalid request.\n");
    }
}

void handleUdp() {
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);
    int bytesRead = recv(udpFd, buffer, MAX_LINE, 0);
}

int acceptCust() {
    struct sockaddr_in custAddress;
    int addressLen = sizeof(custAddress);
    int custFd = accept(serverTcpFd, (struct sockaddr *)&custAddress, (socklen_t*) &addressLen);

    return custFd;
}

Order makeOrder(char* buffer) {
    Order newOrder;
    char* token;
    newOrder.status = WAITING_STATUS;
    token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    memcpy(newOrder.username, token, strlen(token));
    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    newOrder.custPort = atoi(token);
    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    sprintf(newOrder.foodName, "%s", token);
    char logBuffer[MAX_LINE];
    sprintf(logBuffer, "new order recieved from: %s, food: %s\n", newOrder.username, newOrder.foodName);
    logEvent(logBuffer);
    return newOrder;
}

void addOrder(Order newOrder) {
    orders[orderCount] = newOrder;
    orderCount++;
}

void timeOutOrder(char* buffer) {
    char* token;
    token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    int port = atoi(token);
    char logBuffer[MAX_LINE];
    sprintf(logBuffer, "order timed out, customer port: %d\n", port);
    logEvent(logBuffer);
    setOrderStatus(port, TIME_OUT_STATUS);
}

void handleTCP(char* buffer, int currCustFd) {
    if (buffer[0] == 'c') {
        print("new order!\n");
        Order newOrder = makeOrder(buffer);
        addOrder(newOrder);
    }
    else if (buffer[0] == 'T') {
        timeOutOrder(buffer);
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
                    int newCust = acceptCust();
                    FD_SET(newCust, &masterSet);
                    maxFd = max(newCust, maxFd);
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

void addFood(char* foodName, char* ing, int amount)  {
    if (foodCount == 0 || strcmp(foods[foodCount-1].name, foodName) != 0) {
        memcpy(foods[foodCount].name, foodName, strlen(foodName));
        memcpy(foods[foodCount].ingredients[0].name, ing, strlen(ing));
        foods[foodCount].ingredients[0].amount = amount;
        foods[foodCount].ingCount++;
        foodCount++;
        return;
    }
    if (strcmp(foods[foodCount-1].name, foodName) == 0) {
        int ingCount = foods[foodCount-1].ingCount;
        memcpy(foods[foodCount-1].ingredients[ingCount].name, ing, strlen(ing));
        foods[foodCount-1].ingredients[ingCount].amount = amount;
        foods[foodCount-1].ingCount++;
        return;
    }
    print("add food invalid.\n");
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

void readJson() {
    char* jsonString = jsonToString();
    readJsonFoods(jsonString);
}

void makeLogFile() {
    logFd = open("restaurant-log.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (logFd == -1) {
        perror("open logFd");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    getArgs(argc, argv);
    makeLogFile();
    greet();
    readJson();
    tcpAddress = initAddr(TCP_ADDRESS, TCP_PORT);
    udpAddress = initAddr(UDP_ADDRESS, UDP_PORT);
    udpSetup();
    tcpSetup();
    broadcastUser(OPEN_SGN);
    work();
    return 0;
}