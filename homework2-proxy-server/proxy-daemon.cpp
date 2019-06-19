#include <arpa/inet.h> //close
#include <errno.h>
#include <iostream>
#include <map>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h> //close
#include <unordered_map>
#include <vector>

#include "ThreadPool.h"

using namespace std;

struct Respond_Cache {

  vector<char> respond;
  bool public_cache;
  time_t arrival_time; // the time that the respond is added to the cache
  double max_age;      // parse from respond
};

unordered_map<string, Respond_Cache> cache;
mutex cache_mutex;
FILE *log_erss;
bool find_in_response(vector<char> resp_buffer, string target);
int search_cache(string request, int client_connection_fd, int thread_no) {
  time_t now = time(NULL);
  double diff;
  if (cache.count(request)) {
    Respond_Cache respond_in_cache = cache[request];
    diff = difftime(now, respond_in_cache.arrival_time);
    if (diff <= respond_in_cache.max_age || respond_in_cache.public_cache) {
      // success!

      // print to log: found time
      vector<char> resp_buffer = respond_in_cache.respond;
      int status = send(client_connection_fd, &resp_buffer.data()[0],
                        resp_buffer.size(), 0);

      string resp(resp_buffer.begin(), resp_buffer.end());
      int first_end = resp.find("\r\n");
      string first_line_resp = resp.substr(0, first_end);
      fprintf(log_erss, "%d: Responding \"%s\"\n", thread_no,
              first_line_resp.c_str());
      fflush(log_erss);

      cout << "!!!!!!!!!!!!!!!!!!!!!!!!found in cache and return!!!!!!" << endl;
      if (status < 0) {
        return 0;
      }
      return 1;
    } else {
      // print to log: time out blabla
      cache.erase(request);
      return 0;
    }
  }

  else {
    // print not in cache
    return 0;
  }
}

void evict_cache(int thread_no) {
  if (cache.size() == 1000) {
    srand(time(NULL));
    int target = rand() % 1000;
    unordered_map<string, Respond_Cache>::iterator it = cache.begin();
    for (; target > 0; --target) {
      it++;
    }
    cache.erase(it);
    fprintf(log_erss, "%d: WARNING evicted cache randomly\n", thread_no);
    fflush(log_erss);
  }
}

void save_to_cache(string req_buffer_str, string cache_control_line,
                   vector<char> resp_buffer, int thread_no) {
  Respond_Cache newRespond;
  if (find_in_response(resp_buffer, "no-store") ||
      find_in_response(resp_buffer, "private"))
    return;
  if (cache_control_line == "public") {
    newRespond.respond = resp_buffer;
    newRespond.public_cache = true;
    newRespond.arrival_time = time(NULL);
    newRespond.max_age = 0;
    cache[req_buffer_str] = newRespond;
    evict_cache(thread_no);
    // print saved to cache...
  } else if (cache_control_line == "must-revalidate") {
    newRespond.respond = resp_buffer;
    newRespond.public_cache = true;
    newRespond.arrival_time = time(NULL);
    newRespond.max_age = 0;
    cache[req_buffer_str] = newRespond;
    fprintf(log_erss, "%d: cached, but requires re-validation\n", thread_no);
    fflush(log_erss);
    evict_cache(thread_no);
    // print saved to cache...
  } else if (cache_control_line.substr(0, 8) == "max-age=") {
    int max_age = stoi(cache_control_line.substr(8));
    newRespond.respond = resp_buffer;
    newRespond.public_cache = false;
    newRespond.arrival_time = time(NULL);
    newRespond.max_age = max_age;
    cache[req_buffer_str] = newRespond;
    double cur = newRespond.arrival_time;
    double exptime = cur + max_age;
    time_t expire = exptime;
    evict_cache(thread_no);

    struct tm *timeinfo;
    timeinfo = gmtime(&expire);
    fprintf(log_erss, "%d: cached, expires at %s", thread_no,
            asctime(timeinfo));
    fflush(log_erss);
    // print saved to cache...
  } else if (cache_control_line.substr(0, 9) == "s-maxage=") {
    int s_max_age = stoi(cache_control_line.substr(9));
    newRespond.respond = resp_buffer;
    newRespond.public_cache = false;
    newRespond.arrival_time = time(NULL);
    newRespond.max_age = s_max_age;
    cache[req_buffer_str] = newRespond;
    evict_cache(thread_no);
  } else if (cache_control_line == "no-cache") {
    fprintf(log_erss, "%d: not cacheable because no-cache\n", thread_no);
    fflush(log_erss);
    return;
  }
  fprintf(log_erss,
          "%d: not cacheable because cache-controll is not specified\n",
          thread_no);
  fflush(log_erss);
}

bool find_in_response(vector<char> resp_buffer, string target) {
  string respond(resp_buffer.begin(), resp_buffer.end());
  int index = respond.find(target);
  if (index == respond.size())
    return false;
  return true;
}
string parse_cache_control(vector<char> resp_buffer) {
  string respond(resp_buffer.begin(), resp_buffer.end());
  int start = respond.find("Cache-Control: ");
  start += 15;
  int end = respond.find("\r\n", start);
  string res = respond.substr(start, end - start);
  cout << "parse_cache_control: " << res;
  return res;
}

void parse_first_line(string first_line, map<string, string> &http_request) {

  int first_space = first_line.find(" ");
  string method = first_line.substr(0, first_space);
  http_request["Method"] = method;
  int second_space = (first_line.substr(first_space + 1)).find(" ");
  string url = first_line.substr(first_space + 1).substr(0, second_space);
  http_request["Path"] = url;
  int end = (first_line.substr(first_space + 1)).find("\r\n");
  string version = (first_line.substr(first_space + 1))
                       .substr(second_space + 1, end - second_space);
  http_request["Version"] = version;
}

map<string, string> parse(string req) {
  map<string, string> http_request;
  int first_end = req.find("\r\n");
  string first_line = req.substr(0, first_end);
  parse_first_line(first_line, http_request);
  if (http_request["Method"] == "POST") {
    int start = req.find("Host:");
    string rest = req.substr(start + 6);
    start = rest.find("\r\n");
    string host = rest.substr(0, start);
    http_request["Host"] = host;
    return http_request;
  }

  string rest = (req.substr(first_end + 1));
  char *msg = (char *)rest.c_str();
  char *tail;
  char *head = (char *)msg;
  char *mid;
  cout << "parse the rest---before while" << endl;

  while (true) {
    tail = head + 1;
    while (*head++ != '\r') {
    };
    mid = strstr(tail, ":");
    // Look for the failed strstr
    if (tail > mid)
      break;
    http_request[std::string((char *)msg)
                     .substr(tail - (char *)msg, (mid)-tail)] =
        std::string((char *)msg)
            .substr(mid + 2 - (char *)msg, (head - 3) - mid);
  }
  cout << "parse the rest---after while" << endl;

  if (http_request["Method"] == "CONNECT") {
    int start = req.find("Host:");
    string rest = req.substr(start + 6);
    start = rest.find(":");
    string host = rest.substr(0, start);
    http_request["Host"] = host;
  }
  cout << "parse the rest---end" << endl;

  map<string, string>::iterator it;
  for (it = http_request.begin(); it != http_request.end(); ++it)
    std::cout << it->first << " => " << it->second << '\n';

  return http_request;
}

void handle_connect(int client_connection_fd, int socket_fd_to_server,
                    char req_buffer[], int thread_no,
                    map<string, string> http_request) {
  cout << "thread " << thread_no << " connected\n";
  cout << "host name is|" << http_request["Host"] << "|" << endl;
  fd_set fdset;
  string msg = "HTTP/1.1 200 OK\r\n\r\n";
  int status;
  // send Response 200 back to client

  status = write(client_connection_fd, msg.c_str(), msg.size());
  if (status < 0) {
    fprintf(log_erss, "%d: ERROR in write\n", thread_no);
    fflush(log_erss);
  }
  // set up SELECT to transfer data
  fprintf(log_erss, "%d: NOTE setting up turnel\n", thread_no);
  fflush(log_erss);
  fflush(log_erss);
  while (1) {
    vector<char> buffer(1024);
    // char buffer[200000];
    FD_ZERO(&fdset);
    FD_SET(client_connection_fd, &fdset);
    FD_SET(socket_fd_to_server, &fdset);
    int max_fd = max(client_connection_fd, socket_fd_to_server) + 1;
    // memset(&buffer, 0, sizeof(buffer));
    select(max_fd, &fdset, NULL, NULL, NULL);
    if (FD_ISSET(client_connection_fd, &fdset)) {
      // if client side has activity
      status = recv(client_connection_fd, &buffer.data()[0], 1024, 0);
      if (status < 0) {
        fprintf(log_erss, "%d: ERROR in connect:client\n", thread_no);
        fflush(log_erss);

      }

      else if (status == 0) {
        // perror("error in connect:client");
        // close the server side
      } else {
        int i = 0;
        for (; status == 1024; i++) {
          buffer.resize(buffer.size() + 1024);
          int pos1 = (i + 1) * 1024;
          status = recv(client_connection_fd, &buffer.data()[pos1], 1024, 0);
        }
        send(socket_fd_to_server, &buffer.data()[0], 1024 * i + status, 0);
      }
    } else if (FD_ISSET(socket_fd_to_server, &fdset)) {
      // if server side has activity
      status = recv(socket_fd_to_server, &buffer.data()[0], 1024, 0);
      if (status < 0) {
        fprintf(log_erss, "%d: ERROR in connect:client\n", thread_no);
        fflush(log_erss);
      }

      else if (status == 0) {
        // perror("error in connect:client");
        // close the server side
      } else {
        int i = 0;
        for (; status == 1024; i++) {
          buffer.resize(buffer.size() + 1024);
          int pos_1 = (i + 1) * 1024;
          status = recv(socket_fd_to_server, &buffer.data()[pos_1], 1024, 0);
        }
        send(client_connection_fd, &buffer.data()[0], 1024 * i + status, 0);
      }
    }
  }

  fprintf(log_erss, "%d: Tunnel closed\n", thread_no);

  fflush(log_erss);
  return;
}

void handle_get(int client_connection_fd, int socket_fd_to_server,
                char req_buffer[], int thread_no,
                map<string, string> http_request, string first_line) {
  cout << "request is===============" << req_buffer << endl;

  string req_buffer_str(req_buffer);

  cache_mutex.lock();
  int found = search_cache(req_buffer_str, client_connection_fd, thread_no);
  cache_mutex.unlock();

  if (found == 1) {
    return;
  }

  cout << "thread " << thread_no << " connected\n";

  send(socket_fd_to_server, req_buffer, 1024, 0);

  fprintf(log_erss, "%d: Requesting \"%s\" from %s\n", thread_no,
          first_line.c_str(), http_request["Host"].c_str());
  fflush(log_erss);

  cout << "req_buffer sent\n";

  vector<char> resp_buffer(1024);
  int status =
      recv(socket_fd_to_server, &resp_buffer.data()[0], 1024, MSG_WAITALL);
  if (status == -1) {
    fprintf(log_erss, "%d: ERROR in connect:server\n", thread_no);
    fflush(log_erss);
  }

  //	for (int i = 1; status == 1024; i++) {
  //		resp_buffer.resize(resp_buffer.size() + 1024);
  //		cout << resp_buffer.size() << endl;
  //		status = recv(socket_fd_to_server, &resp_buffer.data()[1024 *
  // i], 1024, 0);
  //	}
  //
  //	//what I need: parse the cache-control line!
  //	string cache_control = parse_cache_control(resp_buffer);
  //
  //
  //	cache_mutex.lock();
  //	save_to_cache(req_buffer_str, cache_control, resp_buffer, thread_no);
  //	cache_mutex.unlock();
  //
  //
  //
  //
  //
  //	status = send(client_connection_fd, &resp_buffer.data()[0],
  // resp_buffer.size(), 0); 	if (status < 0) { 		perror("write");
  //	}
  //	return;

  cout << "----------------1\n";
  int i = 0;
  int pos1 = 0;
  for (; status == 1024; i++) {
    resp_buffer.resize(resp_buffer.size() + 1024);
    pos1 = (i + 1) * 1024;
    status =
        recv(socket_fd_to_server, &resp_buffer.data()[pos1], 1024, MSG_WAITALL);
    cout << status << endl;
  }
  cout << "----------------2\n";

  string resp(resp_buffer.begin(), resp_buffer.end());
  int first_end = resp.find("\r\n");
  string first_line_resp = resp.substr(0, first_end);

  fprintf(log_erss, "%d: Received \"%s\" from %s\n", thread_no,
          first_line_resp.c_str(), http_request["Host"].c_str());
  fflush(log_erss);
  // what I need: parse the cache-control line!
  string cache_control = parse_cache_control(resp_buffer);

  cout << "----------------3\n";
  cache_mutex.lock();
  save_to_cache(req_buffer_str, cache_control, resp_buffer, thread_no);
  cache_mutex.unlock();

  cout << "----------------4\n";
  status =
      send(client_connection_fd, &resp_buffer.data()[0], 1024 * i + status, 0);
  cout << "----------------5\n";
  if (status < 0) {
    fprintf(log_erss, "%d: ERROR in write\n", thread_no);
    fflush(log_erss);
  }

  fprintf(log_erss, "%d: Responding \"%s\"\n", thread_no,
          first_line_resp.c_str());
  fflush(log_erss);
  return;
}

void handle_post(int client_connection_fd, int socket_fd_to_server,
                 char req_buffer[], int thread_no,
                 map<string, string> http_request) {
  cout << "request is===============" << req_buffer << endl;

  cout << "thread " << thread_no << " connected\n";

  send(socket_fd_to_server, req_buffer, 1024, 0);

  cout << "req_buffer sent\n";

  return;
}

void newConnection(int client_connection_fd, string request, char req_buffer[],
                   int thread_no, char hoststr[]) {
  cout << "thread :" << thread_no << endl;
  cout << "----------------" << endl;
  cout << "request is " << request << endl;
  cout << "----------------" << endl;
  map<string, string> http_request = parse(request);

  int first_end = request.find("\r\n");
  string first_line = request.substr(0, first_end);
  time_t now = time(0);
  struct tm *timeinfo;
  timeinfo = gmtime(&now);
  fprintf(log_erss, "%d: %s from %s @ %s", thread_no, first_line.c_str(),
          hoststr, asctime(timeinfo));
  fflush(log_erss);

  int status;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = http_request["Host"].c_str();
  const char *port;
  if (http_request["Method"] == "CONNECT")
    port = "443";
  else
    port = "80";
  int socket_fd_to_server;
  // establish connection between proxy and server
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return;
  }
  socket_fd_to_server =
      socket(host_info_list->ai_family, host_info_list->ai_socktype,
             host_info_list->ai_protocol);
  if (socket_fd_to_server == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return;
  }
  status = connect(socket_fd_to_server, host_info_list->ai_addr,
                   host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return;
  }

  if (http_request["Method"] == "CONNECT") {
    // handle connect
    cout << "CONECTTTTTT\n";
    handle_connect(client_connection_fd, socket_fd_to_server, req_buffer,
                   thread_no, http_request);
  } else if (http_request["Method"] == "GET") {
    cout << "GETTTTTTT\n";
    // handle other request
    handle_get(client_connection_fd, socket_fd_to_server, req_buffer, thread_no,
               http_request, first_line);
  } else if (http_request["Method"] == "POST") {
    cout << "POSTTTTTTTT\n";
    handle_post(client_connection_fd, socket_fd_to_server, req_buffer,
                thread_no, http_request);
  }

  return;
}

int main(void) {

  /* Our process ID and Session ID */
  pid_t pid;

  /* Fork off the parent process */
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* On success: The child process becomes session leader */
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  /* Fork off for the second time*/
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* Set new file permissions */
  umask(0);

  /* Change the working directory to the root directory */
  /* or another appropriated directory */
  chdir("/");

  /* Close all open file descriptors */
  int x;
  for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
    close(x);
  }

  /* Open the log file */
  stdin = fopen("/dev/null", "r");
  stdout = fopen("/dev/null", "w+");
  stderr = fopen("/dev/null", "w+");
  const char *pathname = "/var/log/erss";
  const char *pathname_log = "/var/log/erss/proxy.log";
  struct stat info;

  if (stat(pathname, &info) != 0) {
    exit(1);
  } else if (info.st_mode & S_IFDIR) {

  } else {
    int check = mkdir(pathname, S_IRWXU);
    // check if directory is created or not
    if (!check)
      printf("Directory created\n");
    else {
      printf("Unable to create directory\n");
      exit(1);
    }
  }

  /* Daemon-specific initialization goes here */
  // openlog ("/var/log/erss/proxy.log", LOG_PID, LOG_DAEMON);
  /* The Big Loop */

  log_erss = fopen("/var/log/erss/proxy.log", "w");

  /* daemon works...needs to write to log */

  /* ...all done, close the file */

  while (1) {
    /* Do some task here ... */
    // syslog (LOG_NOTICE, "First daemon started.");
    int status;
    int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    const char *hostname = NULL;
    const char *port = "12345";
    // key is request, Respond_Cache contains the respond and time

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
    status =
        bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      cerr << "Error: cannot bind socket" << endl;
      cerr << "  (" << hostname << "," << port << ")" << endl;
      return -1;
    } // if

    status = listen(socket_fd, 65536);
    if (status == -1) {
      cerr << "Error: cannot listen on socket" << endl;
      cerr << "  (" << hostname << "," << port << ")" << endl;
      return -1;
    } // if
    // thread mythreads[100];
    ThreadPool pool(100);

    for (int i = 0;; i++) {
      cout << "Waiting for connection on port " << port << endl;
      struct sockaddr_storage socket_addr;
      socklen_t socket_addr_len = sizeof(socket_addr);
      int client_connection_fd;
      client_connection_fd =
          accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);

      // get ip addr
      char hoststr[NI_MAXHOST];
      char portstr[NI_MAXSERV];
      int rc = getnameinfo((struct sockaddr *)&socket_addr, socket_addr_len,
                           hoststr, sizeof(hoststr), portstr, sizeof(portstr),
                           NI_NUMERICHOST | NI_NUMERICSERV);

      if (client_connection_fd == -1) {
        cerr << "Error: cannot accept connection on socket" << endl;
        return -1;
      } // if

      char req_buffer[1024];
      bzero(req_buffer, sizeof req_buffer);
      recv(client_connection_fd, req_buffer, sizeof(req_buffer), 0);
      string request(req_buffer);

      pool.enqueue(newConnection, client_connection_fd, request, req_buffer, i,
                   hoststr);

      //		mythreads[i] = thread(newConnection,
      // client_connection_fd,
      // request, req_buffer, i);
    }
    freeaddrinfo(host_info_list);
    close(socket_fd);
  }
  fclose(log_erss);
  exit(EXIT_SUCCESS);
}
