#include <arpa/inet.h> //close
#include <errno.h>
#include <iostream>
#include <map>
#include <thread>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <string>
#include <sys/socket.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/types.h>
#include <unistd.h> //close
#include <vector>

using namespace std;


void parse_first_line(string first_line, map<string, string>& http_request) {

	int first_space = first_line.find(" ");
	string method = first_line.substr(0, first_space);
	http_request["method"] = method;
	int second_space = (first_line.substr(first_space + 1)).find(" ");
	string url = first_line.substr(first_space + 1).substr(0, second_space);
	http_request["path"] = url;
	int end = (first_line.substr(first_space + 1)).find("\r\n");
	string version = (first_line.substr(first_space + 1))
											 .substr(second_space + 1, end - second_space);
	http_request["version"] = version;
}

map<string, string> parse(string req) {
	map<string, string> http_request;
	int first_end = req.find("\r\n");
	string first_line = req.substr(0, first_end);
	parse_first_line(first_line, http_request);

	string rest = (req.substr(first_end + 1));
	int second_end = rest.find("\r\n");
	string host = rest.substr(7, second_end - 7);
	http_request["Host"] = host;

	return http_request;
}

void newConnection(int client_connection_fd, string request, char req_buffer[], int thread_no){
	cout<<"thread :"<<thread_no<<endl;
	map<string, string> http_request = parse(request);
	struct addrinfo host_info;
		struct addrinfo *host_info_list;

		// initialize the host
		memset(&host_info, 0, sizeof(host_info));

		host_info.ai_family = AF_UNSPEC;
		host_info.ai_socktype = SOCK_STREAM;
		host_info.ai_flags = AI_PASSIVE;
		// send request to remote server
		cout << "sending to " << http_request["Host"].c_str() << " with port of 80 "
				 << endl;

		int status = getaddrinfo(http_request["Host"].c_str(), "80", &host_info,
												 &host_info_list);
		if (status != 0) {
			cerr << "Error: cannot get address info for host" << endl;
			cerr << "  (" << http_request["Host"].c_str() << ", 80 "
					 << ")" << endl;
			return;
		} // if

		int socket_fd_to_server =
				socket(host_info_list->ai_family, host_info_list->ai_socktype,
							 host_info_list->ai_protocol);

		if (socket_fd_to_server == -1) {
			cerr << "Error: cannot create socket" << endl;
			cerr << "  (" << http_request["Host"] << ", 80)" << endl;
			return;
		} // if

		status = connect(socket_fd_to_server, host_info_list->ai_addr,
										 host_info_list->ai_addrlen);
		if (status == -1) {
			cerr << "Error: cannot connect to socket" << endl;
			cerr << "  (" << http_request["Host"] << ", 80)" << endl;
			return;
		} // if
		cout<<"thread "<<thread_no<<" connected";
		
		send(socket_fd_to_server, req_buffer, 1024, 0);
		
		cout<<"req_buffer sent\n";
		
		char resp_buffer[1024];
		bzero(resp_buffer, 1024);
		status = recv(socket_fd_to_server, resp_buffer, sizeof(resp_buffer), 0);
		cout <<"received "<<status<< " bytes of data"<< "response is:" ;
		for(int i = 0; i< 1024; i++){
			cout<<resp_buffer[i];
		}
		
		
		cout << "\n-------------------------------------" << endl;
		status = send(client_connection_fd, resp_buffer, sizeof(resp_buffer), 0);
		cout << "status = " << status << endl;
		if (status < 0) {
			perror("write");
		}

	//	while (1) {
	//	}
		freeaddrinfo(host_info_list);

		return;
}

int main(int argc, char *argv[]) {
	
	int status;
	int socket_fd;
	struct addrinfo host_info;
	struct addrinfo *host_info_list;
	const char *hostname = NULL;
	const char *port = "12345";

	// initialize the host
	memset(&host_info, 0, sizeof(host_info));

	host_info.ai_family = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;
	host_info.ai_flags = AI_PASSIVE;

	// first check
	status = getaddrinfo(hostname, port, &host_info, &host_info_list);
	if (status != 0) {
		cerr << "Error: cannot get address info for host" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} // if

	//
	socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
										 host_info_list->ai_protocol);
	if (socket_fd == -1) {
		cerr << "Error: cannot create socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} // if

	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) {
		cerr << "Error: cannot bind socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} // if

	status = listen(socket_fd, 100);
	if (status == -1) {
		cerr << "Error: cannot listen on socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} // if

	thread mythreads[100];
	
	for(int i = 0; i <100; i++){
		
			cout << "Waiting for connection on port " << port << endl;
			struct sockaddr_storage socket_addr;
			socklen_t socket_addr_len = sizeof(socket_addr);
			int client_connection_fd;
			client_connection_fd =
					accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
			if (client_connection_fd == -1) {
				cerr << "Error: cannot accept connection on socket" << endl;
				return -1;
			} // if
			
			char req_buffer[1024];
			bzero(req_buffer, sizeof req_buffer);
			recv(client_connection_fd, req_buffer, sizeof(req_buffer), 0);
			string request(req_buffer);
			
			mythreads[i] = thread(newConnection, client_connection_fd, request, req_buffer, i);
			
	}
	freeaddrinfo(host_info_list);
	close(socket_fd);

	return 0;
}