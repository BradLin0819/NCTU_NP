#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <cerrno>
#include <map>
#include <fstream>
#include <regex>
#include <string.h>

using namespace std;
using namespace boost::asio;
using namespace boost::algorithm;

#define MAXSERVER 5
typedef shared_ptr<ip::tcp::socket> http_sock_ptr;

io_service global_io_service;
map< string, map<string, string> > server_conf;
int host_num;

string get_console_html();
string get_panel_html();
string output_shell(string session, char* content);
string output_command(string session, char* content);
string escape(string content);
string get_environment_variable(string index);
int setup_connection_conf(string query_string, map< string, map<string, string> >& conf);


class cgi_session : public enable_shared_from_this<cgi_session> 
{
    private:
        enum { max_length = 4096 };
        string session_name;
        ip::tcp::socket _socket;
        http_sock_ptr _http_socket;
        ip::tcp::resolver resolv;
        char data[max_length];
        ifstream file_input;
        deadline_timer delay;

    public:
        cgi_session(string session_name, http_sock_ptr http_socket) : session_name(session_name), _socket(global_io_service), resolv(global_io_service), 
            _http_socket(http_socket), delay(global_io_service), file_input(server_conf[session_name]["file"]) 
        {
        }

        void start()
        {
            do_resolve();
        }

        ~cgi_session()
        {
            if (_http_socket.use_count() == 2)
            {
                _http_socket->close();
                _http_socket.reset();
            }
        }

    private:
        void do_resolve()
        {
            auto self(shared_from_this());
            ip::tcp::resolver::query _query(server_conf[session_name]["host"], server_conf[session_name]["port"]);
            
            resolv.async_resolve(_query,
                [this, self](boost::system::error_code ec, ip::tcp::resolver::iterator it)
                {
                    if (!ec)
                    {
                        do_connect(it);
                    }
                });
        }
        
        void do_connect(ip::tcp::resolver::iterator it)
        {
            auto self(shared_from_this());
            
            _socket.async_connect(*it,
                [this, self](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        do_read();
                    }
                });
        }

        void do_read() 
        {
            auto self(shared_from_this());

            memset(data, 0, sizeof(data));
            _socket.async_read_some(
                buffer(data, max_length),
                [this, self](boost::system::error_code ec, size_t length)
                {
                    if (!ec)
                    {   
                        string shell_output = output_shell(session_name, data);
                        
                        _http_socket->async_write_some(
                            buffer(shell_output.c_str(), shell_output.length()),
                            [this, self](boost::system::error_code ec, size_t /*length*/)
                            {});

                        if (string(data).find("% ") != string::npos)
                        {
                            delay.expires_from_now(boost::posix_time::millisec(200));
                            delay.async_wait(
                                [this, self](boost::system::error_code ec)
                                {
                                    do_send_cmd();
                                });
                        }
                        do_read();
                    }
                });
        }

        void do_send_cmd() 
        {
            auto self(shared_from_this());
            string input_line = "";
            
            getline(file_input, input_line);
            if (input_line == "exit")
            {
                file_input.close();
            }
            input_line += "\n";

            string command_output = output_command(session_name, (char*)input_line.c_str());
            
            _http_socket->async_write_some(
                buffer(command_output.c_str(), command_output.length()),
                [this, self](boost::system::error_code ec, size_t /*length*/)
                {});

            _socket.async_write_some(
                buffer(input_line.c_str(), input_line.length()), 
                [this, self](boost::system::error_code ec, size_t /*length*/)
                {});
        }
};

class http_session : public enable_shared_from_this<http_session>
{
    private:
        enum { max_length = 4096 };
        ip::tcp::socket _socket;
        char data[max_length];
        string http_header;
        string request_method;
        string request_uri;
        string query_string;
        string server_protocol;
        string http_host;
        string server_addr;
        string server_port;
        string remote_addr;
        string remote_port;

    public:
        http_session(ip::tcp::socket socket) : _socket(move(socket))
        {
            //do nothing
        }
        void start()
        { 
            do_read();
        }

    private:
        void do_read()
        {
            auto self(shared_from_this());

            _socket.async_read_some(
                buffer(data, max_length),
                [this, self](boost::system::error_code ec, size_t length)
                {
                    if (!ec)
                    {
                        http_header = data;
                        setup_http_session();
                    }
                });
        }

        void setup_http_session()
        {
            auto self(shared_from_this());
            vector<string> each_line;
            vector<string> first_line;
            vector<string> uri_and_query;
            vector<string> host_line;
            vector<string> host_port;
            boost::system::error_code ec;
            ip::tcp::endpoint server_endpoint = _socket.local_endpoint(ec);
            if (!ec)
            {
                server_addr = server_endpoint.address().to_string();
                server_port = to_string(server_endpoint.port());
            }
            ip::tcp::endpoint client_endpoint = _socket.remote_endpoint(ec);
            if (!ec)
            {
                remote_addr = client_endpoint.address().to_string();
                remote_port = to_string(client_endpoint.port());
            }

            split(each_line, http_header, is_any_of("\r\n"));
            split(first_line, each_line[0], is_any_of(" "));
            split(host_line, each_line[2], is_any_of(" "));
            request_method = first_line[0];
            request_uri = first_line[1];
            if (first_line[1].find("?") != string::npos)
            {
                split(uri_and_query, first_line[1], is_any_of("?"));
                query_string = uri_and_query[1];
            }
            else
            {
                query_string = "";
            }
            server_protocol =  first_line[2];
            split(host_port, host_line[1], is_any_of(":"));
            http_host = host_port[0];
            
            if (request_uri.find("/panel.cgi") != string::npos)
            {
                string panel_html = get_panel_html();

                _socket.async_send(
                    buffer(panel_html.c_str(), panel_html.length()),
                    [this, self](boost::system::error_code ec, size_t /* length */)
                    {
                        if (!ec)
                        {
                            _socket.close();
                        }
                    });
            }
            else if (request_uri.find("/console.cgi") != string::npos)
            {
                string console_html = "";
                host_num = 0;

                server_conf.clear();
                host_num = setup_connection_conf(query_string, server_conf);
                console_html = get_console_html();

                _socket.async_send(
                    buffer(console_html.c_str(), console_html.length()),
                    [this, self](boost::system::error_code ec, size_t /* length */)
                    {
                        if (!ec)
                        {
                            http_sock_ptr http_socket(&_socket);
                            for (int i = 0; i < host_num; ++i)
                            {
                                string session_name = "s" + to_string(i);
                                make_shared<cgi_session>(session_name, http_socket)->start();
                            }
                            global_io_service.run();
                        }
                    });
            }
        }

        void print_http_header()
        {
            cout << request_method << endl;
            cout << request_uri << endl;
            cout << query_string << endl;
            cout << server_protocol << endl;
            cout << http_host << endl;
            cout << server_addr << endl;
            cout << server_port << endl;
            cout << remote_addr << endl;
            cout << remote_port << endl;
        }
};

class httpserver
{
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;

    public:
        httpserver(short port)
            : _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
            _socket(global_io_service)
        {
            do_accept();
        }

    private:
        void do_accept()
        {
            _acceptor.async_accept(_socket, [this](boost::system::error_code ec)
            {
                if (!ec)
                {
                    make_shared<http_session>(move(_socket))->start();
                }
                do_accept();
            });
        }
};

int main(int argc, char* const argv[])
{
    if (argc != 2)
    {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }
    try
    {
        unsigned short port = atoi(argv[1]);

        httpserver server(port);
        global_io_service.run();
    }
    catch (exception& e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

string get_panel_html()
{
    string panel_html = "";
    string status_ok = "HTTP/1.1 200 OK\r\n";
    string test_case_menu = "";
	string host_menu = "";
    string FORM_METHOD = "GET";
    string FORM_ACTION = "console.cgi";
    string TEST_CASE_DIR = "test_case";
    string DOMAIN = "cs.nctu.edu.tw";

	for (int i = 1; i < 11; ++i)
    {
		test_case_menu += ("<option value=t" + to_string(i) + ".txt>t" + to_string(i) + ".txt</option>");
	}
	for (int i = 1; i <= 12; ++i) 
    {
		host_menu += ("<option value=nplinux" + to_string(i) + "." + DOMAIN + ">nplinux" + to_string(i) + "</option>");
	}
	panel_html += status_ok;
	panel_html += "Content-type: text/html\r\n\r\n";
	panel_html += "<!DOCTYPE html>\n";
	panel_html += "<html lang=\"en\">\n";
	panel_html += "	<head>\n";
	panel_html += "		<title>NP Project 3 panel_html</title>\n";
	panel_html += "		<link\n";
	panel_html += "			rel=\"stylesheet\"\n";
	panel_html += "			href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n";
	panel_html += "			integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n";
	panel_html += "			crossorigin=\"anonymous\"\n";
	panel_html += "		/>\n";
	panel_html += "		<link\n";
	panel_html += "			href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
	panel_html += "			rel=\"stylesheet\"\n";
	panel_html += "		/>\n";
	panel_html += "		<link\n";
	panel_html += "			rel=\"icon\"\n";
	panel_html += "			type=\"image/png\"\n";
	panel_html += "			href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n";
	panel_html += "		/>\n";
	panel_html += "		<style>\n";
	panel_html += "			* {\n";
	panel_html += "				font-family: 'Source Code Pro', monospace;\n";
	panel_html += "			}\n";
	panel_html += "		</style>\n";
	panel_html += "	</head>\n";
	panel_html += "	<body class = \"bg-secondary pt-5\">";
	panel_html += "		<form action = \"" + FORM_ACTION + "\" method = \"" + FORM_METHOD + "\">\n";
	panel_html += "			<table class = \"table mx-auto bg-light\" style = \"width: inherit\">\n";
	panel_html += "				<thead class = \"thead-dark\">\n";
	panel_html += "					<tr>\n";
	panel_html += "						<th scope = \"col\">#</th>\n";
	panel_html += "						<th scope = \"col\">Host</th>\n";
	panel_html += "						<th scope = \"col\">Port</th>\n";
	panel_html += "						<th scope = \"col\">Input File</th>\n";
	panel_html += "					</tr>\n";
	panel_html += "				</thead>\n";
	panel_html += "				<tbody>\n";

	for (int i = 0; i < MAXSERVER; ++i)
    {
		panel_html += "				<tr>\n";
		panel_html += ("					<th scope = \"row\" class = \"align-middle\">Session " + to_string(i + 1) + "</th>\n");
		panel_html += "					<td>\n";
		panel_html += "						<div class = \"input-group\">\n";
		panel_html += ("							<select name = \"h" + to_string(i) + "\" class = \"custom-select\">\n");
		panel_html += ("								<option></option>" + host_menu + "\n");
		panel_html += "							</select>\n";
		panel_html += "							<div class = \"input-group-append\">\n";
		panel_html += "								<span class = \"input-group-text\">.cs.nctu.edu.tw</span>\n";
		panel_html += "							</div>\n";
		panel_html += "						</div>\n";
		panel_html += "					</td>\n";
		panel_html += "					<td>\n";
		panel_html += ("						<input name = \"p" + to_string(i) + "\" type = \"text\" class = \"form-control\" size = \"5\" />\n");
		panel_html += "					</td>\n";
		panel_html += "					<td>\n";
		panel_html += ("						<select name = \"f" + to_string(i) + "\" class = \"custom-select\">\n");
		panel_html += "							<option></option>\n";
		panel_html += ("							" + test_case_menu + "\n");
		panel_html += "						</select>\n";
		panel_html += "					</td>\n";
		panel_html += "				</tr>";
	}

	panel_html += "					<tr>\n";
	panel_html += "						<td colspan=\"3\"></td>\n";
	panel_html += "						<td>\n";
	panel_html += "							<button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n";
	panel_html += "						</td>\n";
	panel_html += "					</tr>\n";
	panel_html += "				</tbody>\n";
	panel_html += "			</table>\n";
	panel_html += "		</form>\n";
	panel_html += "	</body>\n";
	panel_html += "</html>";

	return panel_html;
}

string get_console_html()
{
    string console_html = "";
    string status_ok = "HTTP/1.1 200 OK\r\n";

    console_html += status_ok;
    console_html += "Content-type: text/html\r\n\r\n";
    console_html += "<!DOCTYPE html>\n";
    console_html += "<html lang=\"en\">\n";
    console_html += "  <head>\n";
    console_html += "    <meta charset=\"UTF-8\" />\n";
    console_html += "    <title>NP Project 3 Console</title>\n";
    console_html += "    <link\n";
    console_html += "      rel=\"stylesheet\"\n";
    console_html += "      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n";
    console_html += "      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n";
    console_html += "      crossorigin=\"anonymous\"\n";
    console_html += "    />\n";
    console_html += "    <link\n";
    console_html += "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    console_html += "      rel=\"stylesheet\"\n";
    console_html += "    />\n";
    console_html += "    <link\n";
    console_html += "      rel=\"icon\"\n";
    console_html += "      type=\"image/png\"\n";
    console_html += "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    console_html += "    />\n";
    console_html += "    <style>\n";
    console_html += "      * {\n";
    console_html += "        font-family: 'Source Code Pro', monospace;\n";
    console_html += "        font-size: 1rem !important;\n";
    console_html += "      }\n";
    console_html += "      body {\n";
    console_html += "        background-color: #212529;\n";
    console_html += "      }\n";
    console_html += "      pre {\n";
    console_html += "        color: #cccccc;\n";
    console_html += "      }\n";
    console_html += "      b {\n";
    console_html += "        color: #ffffff;\n";
    console_html += "      }\n";
    console_html += "    </style>\n";
    console_html += "  </head>\n";
    console_html += "  <body>\n";
    console_html += "    <table class=\"table table-dark table-bordered\">\n";
    console_html += "      <thead>\n";
    console_html += "        <tr>\n";

    for (int i = 0; i < host_num; ++i)
    {
        string session_name = "s" + to_string(i);
        console_html += ("          <th scope=\"col\">" + server_conf[session_name]["host"] + ":" + server_conf[session_name]["port"] + "</th>\n"); 
    }

    console_html += "        </tr>\n";
    console_html += "      </thead>\n";
    console_html += "      <tbody>\n";
    console_html += "        <tr>\n";

    for (int i = 0; i < host_num; ++i)
    {
        string session_name = "s" + to_string(i);
        console_html += "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
    }

    console_html += "        </tr>\n";
    console_html += "      </tbody>\n";
    console_html += "    </table>\n";
    console_html += "  </body>\n";
    console_html += "</html>";

    return console_html;
}

string escape(string content)
{
    string result = "";

    for (int i = 0; i < content.length(); ++i)
    {
        if (isalnum(content[i]))
        {
            result += content[i];
        }
        else
        {
            result += ("&#" + to_string(int(content[i])) + ";");
        }
    }
    return result;
}

string output_shell(string session, char* content)
{
    string content_string = content;

    content_string = escape(content_string);
    return ("<script>document.getElementById('" + session + "').innerHTML += '" + content_string + "';</script>\n");
}

string output_command(string session, char* content)
{
    string content_string = content;

    content_string = escape(content_string);
    return ("<script>document.getElementById('" + session + "').innerHTML += '<b>" + content_string + "</b>';</script>\n");
}

int setup_connection_conf(string query_string, map< string, map<string, string> >& conf)
{
    vector<string> parsed_query_string;
    split(parsed_query_string, query_string, is_any_of("&"));
    int temp = 0;

    for (int i = 0; i < parsed_query_string.size(); ++i)
    {
        vector<string> pairs;
        string session_no;
        
        split(pairs, parsed_query_string[i], is_any_of("="));
        session_no = "s" + pairs[0].substr(1);
        //host
        if (pairs[0][0] == 'h')
        {
            conf[session_no]["host"] = pairs[1];
            if (pairs[1] != "")
                temp++;
        }
        //port
        else if (pairs[0][0] == 'p')
        {
            conf[session_no]["port"] = pairs[1];
        }
        //file
        else if (pairs[0][0] == 'f')
        {
            conf[session_no]["file"] = "test_case/" + pairs[1];
        }
    }
    return temp;
}


