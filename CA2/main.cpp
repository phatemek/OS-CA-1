#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstring>
#include <vector>

#define BOLD_WHITE_TEXT "\033[1;37m"
#define BEIGE_TEXT "\033[1;38;2;245;218;185m"
#define BROWN_TEXT "\033[1;38;2;139;69;19m"
#define RED_TEXT "\033[1;38;2;183;65;14m"
#define BLUE_TEXT "\033[1;34m"
#define GOLD_TEXT "\033[1;38;2;255;215;0m"
#define RESET_TEXT "\033[0m"

#define FOR(i, x, y) for(int i = x; i < y; i++)
#define BUFFER_SIZE 1024
#define pb push_back

namespace fs = std::filesystem;
using namespace std;

int log_fd;
int piper;
char* dir_path;

void log_event(string str) {
    str += "\n";
    const char* event_c = str.c_str();
    write(log_fd, event_c, strlen(event_c));
}

void handle_top_house(int* pipeWRh, int* pipeWRt, vector<string> input, string intended_fp) {
    close(pipeWRh[1]);
    close(pipeWRt[0]);
    int pipew = pipeWRt[1];
    piper = pipeWRh[0];
    string message = intended_fp + " " + input[3] + " " + input[1];
    const char* message_c = message.c_str();
    write(pipew, message_c, strlen(message_c));
    log_event("top sent house through pipe: " + message);
    close(pipew);
}

void connect_house_process(string intended_fp, vector<string> input) {
    int pipeWRh[2];
    int pipeWRt[2];
    pipe(pipeWRh);
    pipe(pipeWRt);
    int pid = fork();
    if (pid == 0) {
        close(pipeWRh[0]);
        close(pipeWRt[1]);
        char pipeWRh_str[BUFFER_SIZE];
        snprintf(pipeWRh_str, sizeof(pipeWRh_str), "%d", pipeWRh[1]);
        char pipeWRt_str[BUFFER_SIZE];
        snprintf(pipeWRt_str, sizeof(pipeWRt_str), "%d", pipeWRt[0]);
        log_event("top executed house.out file.");
        execlp("./house.out", "./house.out", pipeWRh_str, pipeWRt_str, NULL);
    }
    else {
        handle_top_house(pipeWRh, pipeWRt, input, intended_fp);
    }
}

void connect_bill_process() {
    mkfifo("houseWR", 0777);
    mkfifo("billWR", 0777);
    int pid = fork();
    if (pid == 0) {
        log_event("top executed bill.out file.");
        execlp("./bill.out", "./bill.out", "", NULL);
    }
}

void make_children(vector<string> input) {
    string dir_path_str(dir_path);
    string intended_fp = dir_path_str + "/" + input[0];
    for (auto& entry : fs::directory_iterator(dir_path)) {
        if (fs::is_directory(entry.status()) && entry.path().string() == intended_fp) {
            connect_house_process(intended_fp, input);
        }
    }
    connect_bill_process();
}

vector<int> get_result() {
    vector<int> result;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    read(piper, buffer, sizeof(buffer));
    string log_str(buffer);
    log_event("top recieved from house: " + log_str);
    char* token = strtok(const_cast<char*>(buffer), " ");
    result.pb(atoi(token));
    FOR(i, 0, 4) {
        token = strtok(nullptr, " ");
        result.pb(atoi(token));
    }
    close(piper);
    return result;
}

vector<string> get_input() {
    vector<string> input;
    string dirs;
    string building, source, factor, month;
    for (auto& entry : fs::directory_iterator(dir_path)) {
        if (fs::is_directory(entry.status())) {
            dirs += entry.path().filename().string() + ", ";
        }
    }
    printf(BROWN_TEXT "Please specify the building " RESET_TEXT);
    printf(BEIGE_TEXT "(%s): " RESET_TEXT, dirs.substr(0, dirs.size()-2).c_str());
    printf(BOLD_WHITE_TEXT "");
    cin >> building;
    input.pb(building);
    printf(BROWN_TEXT "Please specify the source " RESET_TEXT);
    printf(BEIGE_TEXT "(Water, Gas, Electricity): " RESET_TEXT);
    printf(BOLD_WHITE_TEXT "");
    cin >> source;
    input.pb(source);
    printf(BROWN_TEXT "Please specify the factor " RESET_TEXT);
    printf(BEIGE_TEXT "(average, sum, peak, bill, difference): " RESET_TEXT);
    printf(BOLD_WHITE_TEXT "");
    cin >> factor;
    input.pb(factor);
    printf(BROWN_TEXT "Please specify the month " RESET_TEXT);
    printf(BEIGE_TEXT "(1-12): " RESET_TEXT);
    printf(BOLD_WHITE_TEXT "");
    cin >> month;
    input.pb(month);
    return input;
}

void answer(vector<int> result, string factor) {
    printf(GOLD_TEXT "The %s is " RESET_TEXT, factor.c_str());
    int idx;
    if (factor == "average") {
        idx = 0; 
    }
    else if (factor == "sum") {
        idx = 1;
    }
    else if (factor == "peak") {
        idx = 2;
    }
    else if (factor == "bill") {
        idx = 3;
    }
    else if (factor == "difference") {
        idx = 4;
    }
    printf(RED_TEXT "%d\n" RESET_TEXT, result[idx]);
}

void open_log_file() {
    log_fd = open("log.txt", O_TRUNC | O_CREAT | O_RDWR, 0777);
}

void check_argv(char* argv1) {
    if (argv1 == NULL) {
        cout << "directory address not specified.\n";
    }
}

void wait_for_children() {
    FOR(i, 0, 2) {
        int status;
        int pid = wait(&status);
    }
}

void handle_user() {
    vector<string> input = get_input();
    make_children(input);
    vector<int> result_vec = get_result();
    answer(result_vec, input[2]);
    wait_for_children();
}

int main(int argc, char* argv[]) {
    check_argv(argv[1]);
    dir_path = argv[1];
    open_log_file();
    handle_user();
    close(log_fd);
    return 0;
}