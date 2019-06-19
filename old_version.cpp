#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <assert.h>
#include <sys/time.h>
#include "potato.h"

using namespace std;

struct Player{
	int player_id;
	int player_port;
	int player_fd;
	char player_addr[512];

};

int main(int argc, char *argv[]) {
	//ringmaster <port_num> <num_players> <num_hops>
	
	if(argc!=4){
		cout << "Syntax: ringmaster <port_num> <num_players> <num_hops>\n" << endl;
		exit(1);
	}
	int port_num = atoi(argv[1]);
	int num_players = atoi(argv[2]);
	int num_hops = atoi(argv[3]);
	
	
	if(port_num<=1024||port_num>65535){
		cout << "Port already in used or not valid\n" << endl;
		exit(1);
	}
	
	if(num_players<=1){
		cout << "num_players is not valid\n" << endl;
		exit(1);
	}
	
	if(!(num_hops>=0&&num_hops<=512)){
		cout << "num_hops is not valid\n" << endl;
		exit(1);
	}
	
//	Potato Ringmaster
//	Players = <number>
//	Hops = <number>
	cout<<"Potato Ringmaster"<<endl;
	cout<<"Players = "<< num_players <<endl;
	cout<<"Hops = "<< num_hops <<endl;
	
	vector<Player*> players;
	Potato potato;
	potato.num_hops = num_hops;
	
	
	
	//------Note: part of the code below is from example------
	
	//setting up socket for ring master
	int status;
	int socket_fd;
	struct addrinfo host_info;
	struct addrinfo *host_info_list;
	const char *hostname = NULL;
	const char *port = argv[1];

	memset(&host_info, 0, sizeof(host_info));

	host_info.ai_family   = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;
	host_info.ai_flags    = AI_PASSIVE;

	status = getaddrinfo(hostname, port, &host_info, &host_info_list);
	if (status != 0) {
		cerr << "Error: cannot get address info for host" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	}
	socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
		
	if (socket_fd == -1) {
		cerr << "Error: cannot create socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} //if

	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) {
		cerr << "Error: cannot bind socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} //if

	status = listen(socket_fd, num_players);
	if (status == -1) {
		cerr << "Error: cannot listen on socket" << endl; 
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} //if

	cout << "Waiting for connection on port " << port << endl;
	int i = 0;
	
	//setting up connections to all players
	while(players.size()<num_players){
		
		struct sockaddr_storage socket_addr;
		socklen_t socket_addr_len = sizeof(socket_addr);
		int client_connection_fd;
		client_connection_fd = accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
		if (client_connection_fd == -1) {
			perror("cannot accept connection on socket: ");
			return -1;
		} //if
		
		char hoststr[NI_MAXHOST];
		char portstr[NI_MAXSERV];
		int rc = getnameinfo((struct sockaddr *)&socket_addr, socket_addr_len, hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc == 0) printf("New connection from %s\n", hoststr);
		
		
	  //send player_id and total number of players
		
		
		//receive the port of that player
		
		
		

		printf("Peer IP address: %s\n", hoststr);
		
		Player *player = new Player;
		player->player_id = i;
		player->player_fd = client_connection_fd;
		
		send(client_connection_fd, &i, sizeof(int), 0);
		send(client_connection_fd, &num_players, sizeof(int), 0);
		
		
		int listening_port;
		recv(client_connection_fd, &listening_port, sizeof(int), 0);
		player->player_port = listening_port;
		
		
		printf("Peer port      : %d\n", player->player_port);
		strcpy(player->player_addr, hoststr);
		players.push_back(player);
		
		cout<<"Player "<<i<<" is ready to play"<<endl;
		cout<<"Player "<<i<<" has fd "<<player->player_fd<<endl;
		i++;
	}
	
	//send the left and right ip and port to all the players
	for(int k = 0; k < num_players; k++){
		
		const char *message_server = "server";
		send(players[k]->player_fd, message_server, 6, 0);
		int ready_to_connect;
		recv(players[k]->player_fd, &ready_to_connect, sizeof(int), 0);
		cout<<"ready to connect is "<<ready_to_connect<<endl;
		
		
		const char *message_client = "client";
		if( k == num_players-1){
			status = send(players[0]->player_fd, message_client, 6, 0);
			cout<<"the size of message_client is : "<<strlen(message_client)<<endl;
			cout<<message_client<<" !!!!sent size(message_client) is : "<< status <<endl;
			status = send(players[0]->player_fd, players[k]->player_addr, 512, 0);
			
			if(status == -1){
				 printf("Error : Cannot send message to player %d\n", k+1);
         perror("Error : Unable to send message : ");
         exit(1);
			}
			status = send(players[0]->player_fd, &players[k]->player_port, sizeof(int), 0);
			if(status == -1){
				 printf("Error : Cannot send message to player %d\n", k+1);
         perror("Error : Unable to send message : ");
         exit(1);
			}
			int connected;
			recv(players[0]->player_fd, &connected, sizeof(int), 0);
			//cout<<"connected is "<<connected<<endl;
		}
		else{
			cout<<"sending to"<<players[k+1]->player_fd<<endl;
			status = send(players[k+1]->player_fd, message_client, 6, 0);
			cout<<"the size of message_client is : "<<strlen(message_client)<<endl;
			cout<<message_client<<" ???sent size(message_client) is : "<< status <<endl;			
			status = send(players[k+1]->player_fd, players[k]->player_addr, 512, 0);
//			cout<<"sended: "<<players[k]->player_addr<<"which is"<<status<<" bytes\n";
			if(status == -1){
				 printf("Error : Cannot send message to player %d\n", k+1);
         perror("Error : Unable to send message : ");
         exit(1);
			}
			status = send(players[k+1]->player_fd, &players[k]->player_port, sizeof(int), 0);
			if(status == -1){
				 printf("Error : Cannot send message to player %d\n", k+1);
         perror("Error : Unable to send message : ");
         exit(1);
			}
			int connected;
			recv(players[k+1]->player_fd, &connected, sizeof(int), 0);
			//cout<<"connected is "<<connected<<endl;
		}
		
	}
	cout<<"everying is connected\n";
	
	//construct a potato according to the hops
	
	potato.num_hops = num_hops;
	potato.no_hop = -1;
	for(i = 0; i<10; i++){
		potato.trace[i] = -1;
	}
	
	if(num_hops > 0){
		
		//srand a player to start the game
		
		srand((unsigned int) time(NULL));
		int startplayer_id = rand()%num_players;
		cout<<"Ready to start the game, sending potato to player "<<startplayer_id<<endl;
		send(players[startplayer_id]->player_fd, &potato, sizeof(potato), 0);
		
		// use select to check every player's fd
		// https://blog.csdn.net/wypblog/article/details/6826286
		fd_set player_fds; 
		timeval timeout={30,0}; //time out after thirty seconds
		
		Potato result_potato;
		FD_ZERO(&player_fds);
		int max_fd = -1;
		for(int i = 0; i < num_players; i++){
			FD_SET(players[i]->player_fd, &player_fds);
			max_fd = max(max_fd+1, players[i]->player_fd);
		}
		assert(max_fd!=-1);
		status = select(max_fd, &player_fds, NULL, NULL, &timeout);
		if(status < 0){
			perror("cannot select: ");
			exit(1);
		}
		for(int i = 0; i < num_players; i++){
			if(FD_ISSET(players[i]->player_fd, &player_fds)){
				status = recv(players[i]->player_fd, &result_potato, sizeof(result_potato), MSG_WAITALL);
				if(status == -1){
					perror("cannot get resulted potato: ");
					exit(1);
				}
				break;
			}
		}
		//print trace
		
		cout<<"------------------------------------------\n";
		cout<<"got a potato! Show the potato data:\n";
		cout<<"num_hops: "<<result_potato.num_hops<<endl;
		cout<<"no_hop: "<<result_potato.no_hop<<endl;
		cout<<"------------------------------------------\n";
		cout<<"Trace of potato:"<<endl;
		cout<<result_potato.trace[0];
		for(i = 1; i < num_hops; i++){
			if(result_potato.trace[i] == -1){
				cout<<"something was wrong I can feel it"<<endl;
				exit(1);
			}
			cout<<","<<result_potato.trace[i];
		}
	}

	//terminate the game
	//send a -1 hop potato to all the 
	cout<<"sending poison_potato\n";
	Potato poison_potato;
	poison_potato.num_hops = -1;
	poison_potato.no_hop = -1;
	for(i = 0; i<10; i++){
		poison_potato.trace[i] = -1;
	}
	
	for(i = 0; i < num_players; i++){
		status = send(players[i]->player_fd, &poison_potato, sizeof(poison_potato), 0);
		if(status == -1){
			perror("cannot send the potato to terminate: ");
			exit(1);
		}

	}
	

	freeaddrinfo(host_info_list);
	close(socket_fd);
	
}



#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/time.h>
#include "potato.h"

using namespace std;




int main(int argc, char *argv[]) {
	//player <machine_name> <port_num>
	if(argc!=3){
		cout << "player <machine_name> <port_num>\n" << endl;
		return 1;
	}
	
	int status;
	int master_socket_fd;
	struct addrinfo host_info;
	struct addrinfo *host_info_list;
	
	const char *hostname = argv[1];
	int port_num = atoi(argv[2]);
	if(port_num<=1024||port_num>65535){
		cout << "Port already in used or not valid\n" << endl;
		exit(1);
	}
	const char *port = argv[2];
	


	memset(&host_info, 0, sizeof(host_info));
	host_info.ai_family   = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;

	status = getaddrinfo(hostname, port, &host_info, &host_info_list);
	if (status != 0) {
		cerr << "Error: cannot get address info for host" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} //if

	master_socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
	if (master_socket_fd == -1) {
		cerr << "Error: cannot create socket" << endl;
		cerr << "  (" << hostname << "," << port << ")" << endl;
		return -1;
	} //if
	
	cout << "Connecting to " << hostname << " on port " << port << "..." << endl;
	
	status = connect(master_socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) {
		perror("Error: cannot connect to socket");
		return -1;
	} //if
	//receive no of playser...
	cout<<"trying to get id and num...\n";
	int player_id;
	int player_num;
	recv(master_socket_fd, &player_id, sizeof(int), 0);
	recv(master_socket_fd, &player_num, sizeof(int), 0);
	
	cout << "Connected as player " << player_id << " out of " << player_num << " total players" << endl;
	
	//find a port(as a server..) that is currently free
	//https://www.linuxquestions.org/questions/linux-server-73/c-code-to-get-next-free-port-4175428177/
	
	int player_socket_fd;
	int left_player_socket_fd;
	int right_player_socket_fd;
	int player_port = -1;
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	player_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(player_socket_fd == -1){
		cerr << "Error: cannot create socket" << endl;
		perror("Error : Unable to create socket : ");
		return -1;
	}
	//sin.sin_addr.s_addr = 0;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_UNSPEC;		
	bool found_port = false;

	
	for(int p = 1025; p<=65535; p++){
			sin.sin_port = htons(p);
			int yes = 1;
			status = setsockopt(player_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			status = bind(player_socket_fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
			if (status != -1) {
				found_port =true;
				player_port = p;
				break;
			}

	}
	if(player_port == -1) exit(1);
	send(master_socket_fd, &player_port, sizeof(int), 0);
	


	

	
	
	//create a ring...a play need to be server and client each one time
	//setup connection
	char buffer[7];
	for(int count = 0; count < 2; count++){
		
		cout<<"looping "<<count<<"...\n";
		
		memset(&buffer, 0, 7);
		status = recv(master_socket_fd, buffer, 7, 0);
		cout<<"recieved size(status) is : "<< status<<endl;
		cout<<"buffer is "<<buffer<<endl;
		if(strcmp(buffer, "server")==0){
			//listen and accept
			cout<< "I am server in this loop\n";
			status = listen(player_socket_fd, 1);
			if (status == -1) {
				cerr << "Error: cannot connect to socket" << endl;
				cerr << "  (" << hostname << "," << port << ")" << endl;
				return -1;
			}
			struct sockaddr_storage socket_addr;
			socklen_t socket_addr_len = sizeof(socket_addr);
			int ack = 69;
			status = send(master_socket_fd, &ack, sizeof(int), 0);
			if(status == -1){
				printf("Error : Cannot send ack\n");
				perror("Error : Unable to send ack : ");
				exit(1);
			}
			right_player_socket_fd = accept(player_socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
			if (right_player_socket_fd == -1) {
				cerr << "Error: cannot connect to socket" << endl;
				cerr << "  (" << hostname << "," << port << ")" << endl;
				return -1;
			}
			cout<<"right_player_socket_fd connected\n"<<"The address and port is: \n";
			char hoststr[NI_MAXHOST];
			char portstr[NI_MAXSERV];
			int rc = getnameinfo((struct sockaddr *)&socket_addr, socket_addr_len, hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
			if (rc == 0) printf("New connection from %s:%s\n", hoststr, portstr);
			
		}
		else if(strcmp(buffer, "client")==0){//buffer == client
			cout<< "I am client in this loop\n";
			int port_to_connect;
			char left_addr[512];
			
			memset(&left_addr, 0, 512);
						
			status = recv(master_socket_fd, left_addr, 512, 0);
			cout<<"recieved size(addr) is : "<< status<<endl;
			if(status == -1){
				printf("Error : Cannot receive left_addr\n");
				perror("Error : Unable to send message : ");
				exit(1);
			}
			cout<<"addr received: "<<left_addr<<endl;
			
			status = recv(master_socket_fd, &port_to_connect, sizeof(int), 0);
			cout<<"recieved size(port) is : "<< status<<endl;
			if(status == -1){
				printf("Error : Cannot receive port_to_connect\n");
				perror("Error : Unable to send message : ");
				exit(1);
			}
			
			cout<<"port received: "<<port_to_connect<<endl;

			
			
			left_player_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
			if(left_player_socket_fd == -1){
				perror("Error : Unable to create socket : ");
				return -1;
			}
			
			
			struct sockaddr_in left_player;
			memset(&sin, 0, sizeof(left_player));
			
			left_player.sin_family = AF_UNSPEC;
			inet_pton(AF_UNSPEC, left_addr, &(left_player.sin_addr.s_addr));
			left_player.sin_port = htons(port_to_connect); 
			cout<<"client trying to connect..\n";
			status = connect(left_player_socket_fd, (struct sockaddr *) &left_player, sizeof(left_player));
			if (status == -1) {
				perror("Error: cannot connect to socket: ");
				return -1;
			}
			int ack = 1;
			send(master_socket_fd, &ack, sizeof(int), 0);
			cout<<"left_player_socket_fd connected\n";
		}
		else{
			
			cout<<"buffer error: \n"<<buffer<< sizeof(buffer)<<endl;
			exit(1);
		}
	}
	
	
	fd_set two_player_one_master_fds;
	
	FD_ZERO(&two_player_one_master_fds);
	FD_SET(master_socket_fd, &two_player_one_master_fds);
	FD_SET(right_player_socket_fd, &two_player_one_master_fds);
	FD_SET(left_player_socket_fd, &two_player_one_master_fds);
	int max_fd = -1;
	max_fd = max(master_socket_fd, max(left_player_socket_fd, right_player_socket_fd))+1;
	assert(max_fd!=-1);
	
	while(true){
		
		Potato received_potato;
		//bool received = false;
		status = select(max_fd, &two_player_one_master_fds, NULL, NULL, NULL);
		if(status < 0){
			perror("cannot select: ");
			exit(1);
		}
		if(FD_ISSET(master_socket_fd, &two_player_one_master_fds)){
			status = recv(master_socket_fd, &received_potato, sizeof(received_potato), MSG_WAITALL);
			if(status == -1){
				perror("cannot recieve potato from master: ");
				exit(1);
			}
			//received = true;
		}
		else if(FD_ISSET(right_player_socket_fd, &two_player_one_master_fds)){
			status = recv(right_player_socket_fd, &received_potato, sizeof(received_potato), MSG_WAITALL);
			if(status == -1){
				perror("cannot recieve potato from right: ");
				exit(1);
			}
			//received = true;
		}
		else if(FD_ISSET(left_player_socket_fd, &two_player_one_master_fds)){
			status = recv(left_player_socket_fd, &received_potato, sizeof(received_potato), MSG_WAITALL);
			if(status == -1){
				perror("cannot recieve potato from left: ");
				exit(1);
			}
			//received = true;
		}
		
		
		
		
		
		cout<<"------------------------------------------\n";
		cout<<"got a potato! Show the potato data:\n";
		cout<<"num_hops: "<<received_potato.num_hops<<endl;
		cout<<"no_hop: "<<received_potato.no_hop<<endl;
		cout<<"------------------------------------------\n";
		if(received_potato.num_hops == -1){
			break;
		}
		
		else{
			received_potato.num_hops--;
			received_potato.no_hop++;
			
			
			assert(received_potato.no_hop>=0);
			received_potato.trace[received_potato.no_hop] = player_id;
			
			
			
			if(received_potato.num_hops == 0){
				cout<<"I’m it\n";
				cout<<"current trace is: "<<received_potato.trace<<endl;
				//send it to master
				status = send(master_socket_fd, &received_potato, sizeof(received_potato), 0);
				if(status == -1){
					perror("cannot send the potato to terminate: ");
					exit(1);
				}
			}
			else{
				
				//send it to left or right
				srand((unsigned int) time(NULL));
				int send_to_left = rand()%2;
				if(send_to_left == 1){
					status = send(left_player_socket_fd, &received_potato, sizeof(received_potato), 0);
					if(status == -1){
						perror("cannot send the potato to left_player_socket_fd: ");
						exit(1);
					}
					int next_player;
					if(player_id == 0)
						next_player= player_num-1;
						
					else{
						next_player = player_id-1;
					}
					cout<<"Sending potato to left "<<next_player<<endl;
				}
				else if(send_to_left == 0){
					status = send(right_player_socket_fd, &received_potato, sizeof(received_potato), 0);
					if(status == -1){
						perror("cannot send the potato to right_player_socket_fd: ");
						exit(1);
					}
					int next_player;
					if(player_id == player_num-1)
						next_player= 0;
						
					else{
						next_player = player_id+1;
					}
					cout<<"Sending potato to right "<<next_player<<endl;
				}
				else{
					cout<<"~~~~~~~~~~~This should not happen!";
				}
			}
		}	
	}
	
	freeaddrinfo(host_info_list);
	close(player_socket_fd);
	close(left_player_socket_fd);
	close(right_player_socket_fd);
	close(master_socket_fd);

}