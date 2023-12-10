#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <cstdlib>
#define FOR(i, x, y) for(int i = x; i < y; i++)
#define pb push_back
#define BUFFER_SIZE 1024

using namespace std;

const char* dir_path;
char* month;
int pipew;
int piper;
char* source;
int log_fd;

void log_event(string str) {
    str += "\n";
    const char* event_c = str.c_str();
    write(log_fd, event_c, strlen(event_c));
}

vector<int> read_source_message(int piper) {
    vector<int> result;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    read(piper, buffer, sizeof(buffer));
    string log_str(buffer);
    log_event("hosue recieved from source: " + log_str);
    char* token = strtok(const_cast<char*>(buffer), " ");
    result.pb(atoi(token));
    FOR(i, 0, 4) {
        token = strtok(nullptr, " ");
        result.pb(atoi(token));
    }
    close(piper);
    return result;
}

void send_pipe_info(int* pipeWRs, int* pipeWRh) {
    close(pipeWRs[1]);
    close(pipeWRh[0]);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s %s %s", source, dir_path, month);
    write(pipeWRh[1], buffer, strlen(buffer));
    string log_str(buffer);
    log_event("house sent to source: " + log_str);
    close(pipeWRh[1]);
}

vector<int> connect_source_process() {
    vector<int> result;
    int pipeWRs[2];
    int pipeWRh[2];
    pipe(pipeWRs);
    pipe(pipeWRh);
    int pid = fork();
    if (pid == 0) {
        close(pipeWRs[0]);
        close(pipeWRh[1]);
        char pipeWRs_str[BUFFER_SIZE];
        snprintf(pipeWRs_str, sizeof(pipeWRs_str), "%d", pipeWRs[1]);
        char pipeWRh_str[BUFFER_SIZE];
        snprintf(pipeWRh_str, sizeof(pipeWRh_str), "%d", pipeWRh[0]);
        log_event("house executed source.out file.");
        execlp("./source.out", "./source.out", pipeWRs_str, pipeWRh_str, NULL);
        return result;
    }
    else {
        send_pipe_info(pipeWRs, pipeWRh);
        result = read_source_message(pipeWRs[0]);
        return result;
    }
}

int get_bill(int bill_factor) {
    int houseWRfd = open("houseWR", O_WRONLY);
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "%d %s %s %s", bill_factor, month, source, dir_path);
    write(houseWRfd, message, strlen(message));
    string log_str_msg(message);
    log_event("hosue wrote to fifo: " + log_str_msg);
    close(houseWRfd);
    int billWRfd = open("billWR", O_RDONLY);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    read(billWRfd, buffer, sizeof(buffer));
    string log_str_bf(buffer);
    log_event("hosue read from fifo: " + log_str_bf);
    close(billWRfd);
    int cost = atoi(buffer);
    return cost;
}

void send_to_top(vector<int> result, int cost) {
    result[3] = cost;
    string res = "";
    FOR(i, 0, result.size()) {
        res += to_string(result[i]) + " ";
    }
    const char* res_c = res.c_str();
    write(pipew, res_c, strlen(res_c));
    log_event("house sent to top: " + res);
    close(pipew);
}

void read_pipe() {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    read(piper, buffer, sizeof(buffer));
    string log_str(buffer);
    log_event("house recieved from top: " + log_str);
    char* token = strtok(const_cast<char*>(buffer), " ");
    dir_path = token;
    token = strtok(nullptr, " ");
    month = token;
    token = strtok(nullptr, " ");
    source = token;
    close(piper);
}

void open_log_file() {
    log_fd = open("log.txt", O_APPEND | O_CREAT | O_RDWR, 0777);
}

void wait_for_children() {
    int status;
    int pid = wait(&status);
}

void handle_top() {
    read_pipe();
    vector<int> result = connect_source_process();
    int cost = get_bill(result[3]);
    send_to_top(result, cost);
    close(pipew);
    wait_for_children();
}

int main(int argc, char* argv[]) {
    pipew = stoi(argv[1]);
    piper = stoi(argv[2]);
    open_log_file();
    handle_top();
    close(log_fd);
    return 0;
}