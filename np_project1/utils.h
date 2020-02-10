#ifndef __UTILS_H_
#define __UTILS_H_

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include "Parser.h"

using namespace std;

struct pipe_counter{
    int readfd;
    int writefd;
    int counter;
    pipe_counter(int readfd, int writefd, int counter):readfd(readfd), writefd(writefd), counter(counter){}
};

pid_t fork_process();
void child_handler(int signo);
int execute_cmd(const vector<string>& args);
bool cmd_exists(const string& cmd);
void pipes_update(vector<pipe_counter>& pipes);
int pipe_for_reading(vector<pipe_counter>& pipes);
int pipe_for_writing(vector<pipe_counter>& pipes, int pipeN);
void set_readfd(int readfd, int newfd);
void set_writefd(int writefd, int newfd);
void close_fd(int fd);
void erase_pipe(vector<pipe_counter>& pipes, int pipe_index);



#endif