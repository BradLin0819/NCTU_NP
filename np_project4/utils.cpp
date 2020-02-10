#include "utils.h"

void set_readfd(int readfd, int newfd)
{
    if (dup2(readfd, newfd) == -1)
        errexit("Modify read fd error!\n");
}

void set_writefd(int writefd, int newfd)
{
    if (dup2(writefd, newfd) == -1)
        errexit("Modify write fd error!\n");
}

void close_fd(int fd)
{
    if (close(fd) == -1)
    {
        string errmsg = "Close fd " + to_string(fd) + " error!\n";
        errexit(errmsg);
    }
}

void errexit(string errmsg)
{
    cerr << errmsg << endl;
    exit(EXIT_FAILURE);
}

vector<string> split_with_delims(const string& str, const string delims)
{
    vector<string> args;
    string result;

    if (str != "")
    {
        char* temp_dup = strdup(str.c_str());
        char* delimiter = (char*)delims.c_str();
        char* pch;
        
        pch = strtok(temp_dup, delimiter);
        while(pch != NULL)
        {
            result = pch;
            args.push_back(result);
            pch = strtok(NULL, delimiter);
        }
        free(temp_dup);
    }
    return args;
}

void child_handler(int signo)
{
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0);
}

int passiveTCP(int port, int qlen)
{
    int sockfd;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	cerr << "Server can't create socket" << endl;
		return -1;
	}
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int optval = 1;
    
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        errexit("Server can't bind socket\n");

    if (listen(sockfd, qlen) < 0)
        errexit("Server can't listen\n");

    return sockfd;
}

int connectTCP(char* host, int port)
{
    struct sockaddr_in server_addr;
    int sockfd;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if ((server_addr.sin_port = htons(port)) == 0 )
        errexit("can't get service entry\n");
    if ((server_addr.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
        errexit("can't get host entry\n");

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errexit("Server can't create socket\n");
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
	    cerr << "Server can't connect socket" << endl;
		return -1;
	}
    return sockfd;
}

bool compare_ip(string real_ip, string ip_mask)
{
	vector<string> real_ip_split;
	vector<string> ip_mask_split;

	real_ip_split = split_with_delims(real_ip, ".");
	ip_mask_split = split_with_delims(ip_mask, ".");

	for (int i = 0; i < real_ip_split.size(); ++i)
	{
		string curr = real_ip_split[i];
		string comp = ip_mask_split[i];

		if (curr != comp && comp != "*")
			return false;
	}
	return true;
}
