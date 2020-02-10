#ifndef __UTILS_H_
#define __UTILS_H_

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

using namespace std;

void set_readfd(int readfd, int newfd);
void set_writefd(int writefd, int newfd);
void close_fd(int fd);
void child_handler(int signo);
void errexit(string errmsg);
vector<string> split_with_delims(const string& str, const string delims);
int passiveTCP(int port, int qlen);
int connectTCP(char* host, int port);
bool compare_ip(string real_ip, string ip_mask);

#endif
