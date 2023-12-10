#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <filesystem>
#include <sys/wait.h>
#include <cstring>
#include <strings.h>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include "csv.h"
#define FOR(i, x, y) for(int i = x; i < y; i++)
#define pb push_back
#define BUFFER_SIZE 1024
#define DAYS 30
#define HOURS 6

using namespace std;

const char* dir_path;
const char* source;
int intended_month;
int pipew;
int piper;
int log_fd;

void log_event(string str) {
    str += "\n";
    const char* event_c = str.c_str();
    write(log_fd, event_c, strlen(event_c));
}

vector<int> calc_factors(vector<int> hours, int sum) {
    vector<int> result;
    int max_h = 0;
    FOR(i, 0, 6) {
        if (hours[i] > hours[max_h]) {
            max_h = i;
        }
    }
    int mean = sum/(DAYS*HOURS);
    result.pb(mean);
    result.pb(sum);
    result.pb(max_h);
    int diff = hours[max_h]/DAYS - mean;
    int bill_factor = 0;
    string source_str(source);
    if (source_str == "Water") {
        bill_factor = sum + hours[max_h]/4;
    }
    else if (source_str == "Electricity") {
        FOR(i, 0, 6) {
            if (hours[i] < mean*DAYS) {
                bill_factor += 0.75 * hours[i];
            }
            else {
                bill_factor += hours[i];
            }
        }
        bill_factor += hours[max_h]/4;
    }
    else {
        bill_factor = sum;
    }
    result.pb(bill_factor);
    result.pb(diff);
    return result;
}

void send_to_house(vector<int> result) {
    string message = "";
    FOR(i, 0, result.size()) {
        message += to_string(result[i]) + ' ';
    }
    const char* message_c = message.c_str();
    write(pipew, message_c, strlen(message_c));
    log_event("source sent to house: " + message);
}

void read_csv() {
    string dir_path_str(dir_path);
    string source_str(source);
    string file_path = dir_path_str + "/" + source_str + ".csv";
    io::CSVReader<9> in(file_path);
    in.read_header(io::ignore_extra_column, "Year", 
    "Month", "Day", "0", "1", "2", "3", "4", "5");
    int year, month, day, h0, h1, h2, h3, h4, h5;
    int sum = 0, c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
    while(in.read_row(year, month, day, h0, h1, h2, h3, h4, h5)){
        if (month == intended_month) {
            int row[] = {year, month, day, h0, h1, h2, h3, h4, h5};
            sum += h0 + h1 + h2 + h3 + h4 + h5;
            c0 += h0; c1 += h1; c2 += h2; c3 += h3; c4 += h4; c5 += h5;
        }
    }
    vector<int> hours = {c0, c1, c2, c3, c4, c5};
    vector<int> result = calc_factors(hours, sum);
    send_to_house(result);
}

void read_pipe() {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    read(piper, buffer, sizeof(buffer));
    string log_str(buffer);
    log_event("source recieved from house: " + log_str);
    char* token = strtok(const_cast<char*>(buffer), " ");
    source = token;
    token = strtok(nullptr, " ");
    dir_path = token;
    token = strtok(nullptr, " ");
    intended_month = atoi(token);
    close(piper);
}

void open_log_file() {
    log_fd = open("log.txt", O_APPEND | O_CREAT | O_RDWR, 0777);
}

void handle_house() {
    read_pipe();
    read_csv();
    close(pipew);
}

int main(int argc, char* argv[]) {
    pipew = stoi(argv[1]);
    piper = stoi(argv[2]);
    open_log_file();
    handle_house();
    close(log_fd);
    return 0;
}