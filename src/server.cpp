#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

std::string _target(std::string);
void http_get(int, std::vector<std::string>, std::string);
void http_post(int, std::string, std::string, std::vector<std::string>);
void* handle_connection(void*);
struct Request {
  int client_sock;
  char* file_directory;
};

int main(int argc, char **argv) {
  std::string file_directory {"./"};
  std::cout << "Preprocessing Args..\n";
  for (int a{1}; a < argc; ++a) {
    if (std::string(argv[a]) == "--directory") {
      if (a + 1 < argc) {
        a++;
        file_directory = argv[a]; 
      } else {
        std::cerr << "--directory needs an argument.\n";
      }
    }
  }
  const int length = file_directory.length();
  char* file_dir = new char[length + 1];
  strcpy(file_dir, file_directory.c_str());

  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221); // htons stores integer in network byte order
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  int count{0};
  std::vector<pthread_t> connection_instances;
    
  while(1) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";
    
    int client_sock = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    Request req;
    req.client_sock = client_sock;
    req.file_directory = file_dir;  

    if (client_sock != -1) {
      pthread_t pid;
      connection_instances.push_back(pid);
      pthread_create(&connection_instances[count], NULL, &handle_connection, (void*) &req);
    }

    count ++;
  }

  for (int p{0}; p < connection_instances.size(); p++) {
    pthread_join(connection_instances[p], NULL);
  }

  close(server_fd);
  return 0;
}

void* handle_connection(void* arg) {
  // pthread_detach(pthread_self());
  Request req = *(Request *) arg;

  std::cout << "Client connected "<< req.client_sock << "\n";
  
  char buffer [1024]{ '\0' };
  recv(req.client_sock, buffer, 1024, 0);
  
  std::stringstream ss_request{buffer};
  std::vector<std::string> split_request;
  std::string body, segment;
  while(std::getline(ss_request, segment, ' ')) {
    split_request.push_back(segment);
  }
  int body_in = 0;
  for (int k{0}; k < split_request.size(); k++) {
    if(split_request[k].find("\r\n\r\n") != std::string::npos) {
      std::cout << "Found Body.\n"; 
      body_in = k;
    }
  }
  body = split_request[body_in].substr(split_request[body_in].find("\r\n\r\n") + 4);
  body_in++;
  for (int k = body_in; k < split_request.size(); k++) {
    body += " " + split_request[k];
  }

  if (split_request[0] == "GET") {
    http_get(req.client_sock, split_request, req.file_directory);
  } else if (split_request[0] == "POST") {
    http_post(req.client_sock, req.file_directory, body, split_request);
  }
  
  pthread_exit(NULL);
}
void http_get(int client_socket, std::vector<std::string> split_request, std::string file_directory) {
  const std::string RES_200 {"HTTP/1.1 200 OK\r\n"};
  const std::string RES_404 {"HTTP/1.1 404 Not Found\r\n\r\n"};
  std::string message, target = _target( split_request[1]);

  std::cout << "[Target]" << target << "\n";
  if (target == "/") {
    message = RES_200 + "\r\n";

  } else if (target == "/echo") {
    std::string m_return{split_request[1].substr(split_request[1].rfind('/') + 1)};
    std::cout << "[Response Body]" << m_return << "\n";
    // Status Line
    message = RES_200 +
    // Headers 
    "Content-Type: text/plain" + "\r\n" + 
    "Content-Length: " + std::to_string(m_return.size()) + "\r\n" + 
    "\r\n" + 
    //Response Body
    m_return;
    
    std::cout << "[Message]" << message << "\n";

  } else if (target == "/user-agent") {
    std::string user_agent = split_request[split_request.size() - 1];
    user_agent.erase(user_agent.end() - 4, user_agent.end());
    std::cout << user_agent << "\n";
    // Status Line
    message = RES_200 +
    // Headers
    "Content-Type: text/plain" + "\r\n" + 
    "Content-Length: " + std::to_string(user_agent.size()) + "\r\n" + 
    "\r\n" + 
    // User Agent
    user_agent;

  } else if (target == "/files") {
    std::string target_file_name = split_request[1].substr(split_request[1].rfind("/") + 1);
    std::string expected_file_path = file_directory + target_file_name;
    std::cout << "[Fetching File]" << expected_file_path << "\n";
    
    std::ifstream ifs(expected_file_path, std::ifstream::in);
    std::cout << "[File Status]" << ifs.is_open() << "\n";
    
    if (ifs.good()) {
      std::string file_contents{""}, file_line;
      while(std::getline(ifs, file_line)){
        file_contents += file_line;
      }

      std::cout << "[File Contents]" << file_contents << "\n";
      message = RES_200 + 
      "Content-Type: application/octet-stream\r\n" + 
      "Content-Length: " + std::to_string(file_contents.size()) + "\r\n\r\n" + 
      file_contents;
      ifs.close();
    } else {
      message = RES_404;
    }

  } else {  
    message = RES_404;
  
  }
  
  send(client_socket, message.c_str(), message.size(), 0);
}

void http_post(int client_socket, std::string file_directory, std::string body, std::vector<std::string> split_request) {
  std::string RES_201 {"HTTP/1.1 201 Created\r\n\r\n"};
  std::string message, target = _target(split_request[1]);
  int content_length{std::stoi(split_request[4])};

  if (target == "/files") {
    std::string new_file_name {split_request[1].substr(split_request[1].rfind('/') + 1)};

    std::ofstream ofs(file_directory + new_file_name, std::ofstream::out);
    if(ofs.is_open()) {
      ofs.write(body.c_str(), content_length);
    }
    message = RES_201;
  }

  send(client_socket, message.c_str(), message.size(), 0);

}

std::string _target(std::string split_request) {
  return (split_request.rfind("/") == 0 )? split_request : split_request.substr(0, split_request.rfind("/"));
}