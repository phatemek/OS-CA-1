#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include "csv.h"

#define FOR(i, x, y) for(int i = x; i < y; i++)
#define BUFFER_SIZE 1024

using namespace std;

char buffer[BUFFER_SIZE];
string dir_path;
int log_fd;

void log_event(string str) {
    str += "\n";
    const char* event_c = str.c_str();
    write(log_fd, event_c, strlen(event_c));
}

int get_coeff(string source, int intended_month) {
    char file_path[BUFFER_SIZE];
    const char* dir_path_c = dir_path.c_str();
    sprintf(file_path, "%s/bills.csv", dir_path_c);
    string file_path_str = file_path;
    io::CSVReader<5>in(file_path_str);
    in.read_header(io::ignore_extra_column, "Year", "Month", "water", "gas", "electricity");
    int year, month, water, gas, electricity;
    while(in.read_row(year, month, water, gas, electricity)){
        if (month == intended_month) {
            if (source == "Water") return water;
            else if (source == "Gas") return gas;
            else return electricity;
        }
    }
    return -1;
}

string fix_path(char* token) {
    char* tok = strtok(const_cast<char*>(token), "/");
    tok = strtok(nullptr, "/");
    string tok_str(tok);
    string path = "./" + tok_str;
    return path;
}

int calc_bill() {
    int houseWRfd = open("houseWR", O_RDONLY);
    read(houseWRfd, buffer, sizeof(buffer));
    string log_str(buffer);
    log_event("bill read from fifo: " + log_str);
    char* token = strtok(const_cast<char*>(buffer), " ");
    int hours = atoi(token);
    token = strtok(nullptr, " ");
    int month = atoi(token);
    token = strtok(nullptr, " ");
    string source(token);
    token = strtok(nullptr, " ");
    dir_path = fix_path(token);
    int coeff = get_coeff(source, month);
    close(houseWRfd);
    return coeff * hours;
}

void send_to_house(int cost) {
    int billWRfd = open("billWR", O_WRONLY);
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%d", cost);
    write(billWRfd, buffer, strlen(buffer));
    string log_str(buffer);
    log_event("bill wrote to fifo: " + log_str);
    close(billWRfd);
}

void open_log_file() {
    log_fd = open("log.txt", O_APPEND | O_CREAT | O_RDWR, 0777);
}

void handle_house() {
    int cost = calc_bill();
    send_to_house(cost);
}

int main(int argc, char* argv[]) {
    open_log_file();
    handle_house();
    close(log_fd);
    return 0;
}