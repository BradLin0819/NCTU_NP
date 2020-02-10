#include "utils.h"

int execute_cmd(const vector<string>& args)
{
    vector<char*> temp_vec;
    for(int i = 0; i < args.size(); ++i)
    {
        temp_vec.push_back((char*)args[i].c_str());
    }
    temp_vec.push_back(NULL);
    return execvp(temp_vec[0], &temp_vec[0]);
}

bool cmd_exists(const string& cmd)
{
    string path(getenv("PATH"));
    vector<string> paths = split_with_delims(path, ":");
    for(int i = 0; i < paths.size(); ++i)
    {
        string filename = "";
        filename = paths[i] + "/" + cmd;
        if(access(filename.c_str(), F_OK) != -1)
            return true;
    }
    return false;
}

void pipes_update(vector<pipe_counter>& pipes)
{
    for(int i = 0; i < pipes.size(); ++i)
    {
        (pipes[i].counter)--;
    }
}

int pipe_for_reading(vector<pipe_counter>& pipes)
{
    for(int i = 0; i < pipes.size(); ++i)
    {
        if(pipes[i].counter == 0)
            return i;
    }
    return -1;
}

int pipe_for_writing(vector<pipe_counter>& pipes, int pipeN)
{
    for(int i = 0; i < pipes.size(); ++i)
    {
        if(pipes[i].counter == pipeN)
            return i;
    }
    return -1;
}

void set_readfd(int readfd, int newfd)
{
    if(dup2(readfd, newfd) == -1)
    {
        perror("Modify read fd error!\n");
        exit(1);
    }
}

void set_writefd(int writefd, int newfd)
{
    if(dup2(writefd, newfd) == -1)
    {
        perror("Modify write fd error!\n");
        exit(1);
    }
}

void close_fd(int fd)
{
    if(close(fd) == -1)
    {
        cerr << "Close fd " << fd << " error!\n";
        exit(1);
    }
}

void erase_pipe(vector<pipe_counter>& pipes, int pipe_index)
{
    if(pipe_index != -1)
    {
        if(pipes[pipe_index].counter == 0)
        {
            close(pipes[pipe_index].readfd);
            close(pipes[pipe_index].writefd);
            pipes.erase(pipes.begin() + pipe_index);
        }
    }
}