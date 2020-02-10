#ifndef _PARSER_H_
#define _PARSER_H_

#include "Command.h"
vector<string> split_with_delims(const string& cmd, const string delims);
bool is_redirection(string arg);
bool is_user_pipe_in(string arg);
bool is_user_pipe_out(string arg);
int number_of_user_pipe_out(const vector<string>& split_cmd);

class Parser{
    private:
        string command_line;
    public:
        Parser(){}
        Parser(string input):command_line(input){}
        vector<Command*> get_command_list();
};

#endif