#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <iostream>
#include <vector>

using namespace std;

class Command{
    private:
        vector<string> args; 
        string filename;
        int pipeN_count;
        bool is_first;
        bool is_last;
        bool has_pipe;
        bool has_exclamation;
        int user_pipe_in;
        int user_pipe_out;

    public:
        Command();
        void set_cmd(string cmd);
        void set_args(const vector<string>& args);
        void set_pipe(bool has_pipe);
        void set_exclamation(bool exclamation);
        void set_filename(string filename);
        void set_pipeN_count(int num);
        void set_first(bool is_first);
        void set_last(bool is_last);
        void set_user_pipe_in(int user_pipe_in);
        void set_user_pipe_out(int user_pipe_out);
        bool check_first();
        bool check_last();
        bool check_singlecmd();
        bool check_pipe();
        bool check_pipeN();
        bool check_exclamation();
        bool check_redirection();
        bool check_if_pipe();
        bool check_if_user_pipe_in();
        bool check_if_user_pipe_out();
        int get_pipeN_count();
        int get_user_pipe_in();
        int get_user_pipe_out();
        string get_filename();
        const vector<string>& get_args();
        void info();
};


#endif