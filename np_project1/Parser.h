#ifndef _PARSER_H_
#define _PARSER_H_

#include "Command.h"
vector<string> split_with_delims(const string& cmd, const string delims);
class Parser{
    private:
        string command_line;
    public:
        Parser(){}
        Parser(string input):command_line(input){}
        vector<Command*> get_command_list();
};

#endif