#include "utils.h"
#include <memory>
#define MAXBUFFSIZE 8192
#define CONNECT 0x01
#define BIND 0x02

void relaydata(int src_fd, int dst_fd);

enum access_mode
{
    REJECT = 1,
    ACCEPT = 2
};

class socks_server
{
    private:
        int socks_server_fd;
    
    public:
        socks_server(int port);
        void run();
};

class socks_session : public enable_shared_from_this<socks_session>
{
    private:
        int socks_client_fd;
        string src_ip;
        ushort src_port;
        string dst_ip;
        ushort dst_port;
        ifstream firewall_conf;
    
    public:
        socks_session(int fd, string src_ip, ushort src_port):socks_client_fd(fd), src_ip(src_ip), src_port(src_port), firewall_conf("socks.conf")
		{
			//do nothing
		}
        void run();
        access_mode get_access_mode(string dst_ip, u_char socks_mode);
		~socks_session()
		{
			close_fd(socks_client_fd);
		}
};



int main(int argc, char* const argv[])
{
    if (argc != 2)
    {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }
    signal(SIGCHLD, child_handler);
    
    socks_server server((ushort)atoi(argv[1]));
    server.run();

    return 0;
}

socks_server::socks_server(int port)
{
    this->socks_server_fd = passiveTCP(port, 30);
}

void socks_server::run()
{
    int server_fd = this->socks_server_fd;

    while (true)
    {
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
		if (client_fd < 0)
			errexit("Server accept error\n"); 
        
		pid_t socks_session_pid;
        string client_ip = inet_ntoa(client_addr.sin_addr);
        ushort client_port = ntohs(client_addr.sin_port);
           
		if ((socks_session_pid = fork()) == 0)
        {
            close_fd(server_fd);
            make_shared<socks_session>(client_fd, client_ip, client_port)->run();
			exit(EXIT_SUCCESS);
        }
		else if (socks_session_pid < 0)
		{
			close_fd(client_fd);
			errexit("can't fork\n");
		}
		else
		{
			close_fd(client_fd);
		}
        
    }
}

void socks_session::run()
{
    int client_fd = this->socks_client_fd;
    u_char socks_request[MAXBUFFSIZE];
    u_char reply[8];
    u_char VN;
    u_char CD;
    string dst_ip;
    uint socks_4a_dstip;
    ushort dst_port;
    access_mode permission = ACCEPT;

    memset(reply, 0, sizeof(reply));
    memset(socks_request, 0, sizeof(socks_request));
    if (read(client_fd, socks_request, sizeof(socks_request)) < 0)
	{
		close_fd(client_fd);
        errexit("read socks request error\n");
	}

    VN = socks_request[0];
    
    //socks version
    if (VN != 0x04)
	{
		close_fd(client_fd);
        errexit("not supported version of socks packet\n");
	}

    CD = socks_request[1];
    dst_port = (socks_request[2] << 8) | socks_request[3];
    socks_4a_dstip = (socks_request[4] << 24) | (socks_request[5] << 16) | (socks_request[6] << 8) | socks_request[7];
    
    //destination is domain name
    if ((socks_4a_dstip & 0x000000FF) == socks_4a_dstip)
    {
        string domain_name;
        u_char *iter = (socks_request + 8);
        u_char *domain_name_iter = NULL;
        struct hostent *host_entry;
        
        while (*iter != '\0')
        {
            iter++;
        }
        domain_name_iter = (iter + 1);

        while (*domain_name_iter != '\0')
        {
            domain_name += *domain_name_iter;
			domain_name_iter++;
        }
        host_entry = gethostbyname(domain_name.c_str());
        dst_ip = inet_ntoa(*((struct in_addr*) 
                           host_entry->h_addr_list[0]));
    }
    else
    {
        dst_ip = to_string(socks_request[4]) + "." + to_string(socks_request[5]) + "." 
                + to_string(socks_request[6]) + "." + to_string(socks_request[7]);
    }

    permission = get_access_mode(dst_ip, CD);

   	cout << endl; 
  	cout << "+------------------------------------------------------+" << endl;
    cout << "<S_IP>: " << src_ip << endl;
    cout << "<S_PORT>: " << to_string(src_port) << endl;
    cout << "<D_IP>: " << dst_ip << endl;
    cout << "<D_PORT>: " << to_string(dst_port) << endl;
    cout << "<Command>: " << ((CD == CONNECT) ? "CONNECT" : (CD == BIND) ? "BIND" : "Unknown command") << endl;
    cout << "<Reply>: " << (permission == ACCEPT ? "Accept" : "Reject") << endl;
    cout << "+------------------------------------------------------+" << endl;
	cout << endl;

    if (permission == ACCEPT)
    {
        reply[1] = 0x5A;

        if (CD == CONNECT)
        {
            int app_server_fd = connectTCP((char*)dst_ip.c_str(), dst_port);
			
			if (app_server_fd > 0)
			{
            	write(client_fd, reply, 8);
            	relaydata(client_fd, app_server_fd);
			}           
        }
        else if (CD == BIND)
        {
            int app_server_fd;
            ushort bind_server_port;
            struct sockaddr_in server_sin;
            socklen_t server_len = sizeof(server_sin);
            struct sockaddr_in app_server_addr;
            socklen_t app_server_addr_len = sizeof(app_server_addr);
            int bind_server_fd = passiveTCP(INADDR_ANY, 30);
			
			if (bind_server_fd > 0)
			{
            	if (getsockname(bind_server_fd, (struct sockaddr *)&server_sin, &server_len) < 0)
				{	
					close_fd(bind_server_fd);
					close_fd(client_fd);
                	errexit("getsockname error\n");
				}
            	bind_server_port = ntohs(server_sin.sin_port);
            	reply[2] = (u_char)(bind_server_port / 256);
            	reply[3] = (u_char)(bind_server_port % 256);
            
            	write(client_fd, reply, 8);
            	if ((app_server_fd = accept(bind_server_fd, (struct sockaddr*)&app_server_addr, &app_server_addr_len)) < 0)
				{	
					close_fd(bind_server_fd);
					close_fd(client_fd);
                	errexit("bind mode accept error\n");
				}
            	write(client_fd, reply, 8);
            	relaydata(client_fd, app_server_fd);
				close_fd(bind_server_fd);
			}
        }
    }
    else
	{
        reply[1] = 0x5B;
		write(client_fd, reply, 8);
    }
}

access_mode socks_session::get_access_mode(string dst_ip, u_char socks_mode)
{
	string line;
	while (getline(firewall_conf, line))
	{
		if (line != "" && line != "\r")
		{
				vector<string> split_string = split_with_delims(line, " ");
				string action = split_string[0];
				string mode = split_string[1];
				string ip_mask = split_string[2];
				
				if (split_string.size() != 0)
				{
					if (action == "permit")
					{
						u_char mode_num;
					
						if (mode == "c")
							mode_num = CONNECT;
						else if (mode == "b")
							mode_num = BIND;
					
						if (mode_num == socks_mode)
							if (compare_ip(dst_ip, ip_mask))
								return ACCEPT;
					}
				}
		}
	}
	return REJECT;    
}

void relaydata(int src_fd, int dst_fd)
{
    fd_set afds;
    fd_set rfds;
    int nfds;

    FD_ZERO(&afds);
	FD_ZERO(&rfds);
    FD_SET(src_fd, &afds);
    FD_SET(dst_fd, &afds);
    nfds = max(src_fd, dst_fd) + 1;
	
    while (true)
    {
        int readsize = 0;
		
		memcpy(&rfds, &afds, sizeof(afds));
        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            errexit("select error\n");
		else
        {
            if(FD_ISSET(src_fd, &rfds))
            {
				char src_buffer[MAXBUFFSIZE];
                
                readsize = read(src_fd, src_buffer, MAXBUFFSIZE);
                if (readsize <= 0)
				{
					close_fd(src_fd);
					close_fd(dst_fd);	
					exit(EXIT_SUCCESS);
				}
				
                if (write(dst_fd, src_buffer, readsize) <= 0)
					errexit("src write error");
			}
            if(FD_ISSET(dst_fd, &rfds))
            {
				char dst_buffer[MAXBUFFSIZE];

                readsize = read(dst_fd, dst_buffer, MAXBUFFSIZE);
                if (readsize <= 0)
				{
					close_fd(src_fd);
					close_fd(dst_fd);
					exit(EXIT_SUCCESS);
				}
				
                if (write(src_fd, dst_buffer, readsize) < 0) 
					errexit("dst write error");
			}
        }
    }
}
