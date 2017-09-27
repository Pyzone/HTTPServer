/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mman.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define REQUEST_BUFFER_SIZE 4096
#define PATH_BUFFER_SIZE 4096

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void send_content(int client_socket, FILE * fp) // file is already opened; 
{
	fseek(fp, 0L, SEEK_END); 
	long file_size = ftell(fp); 
	rewind(fp); 
	char * file_content_addr = mmap((void*)0, file_size, PROT_READ, MAP_SHARED, fileno(fp), 0);
	send(client_socket, file_content_addr, file_size, 0); 
	//send(client_socket, "\r\n\r\n", 4, 0); 
	return; 
}

void send_good_header(int client_socket)
{
	char * good_header = "HTTP/1.1 200 OK\r\n\r\n"; 
	send(client_socket, good_header, strlen(good_header), 0); 
}

void send_bad_header(int client_socket) 
{
	char * bad_header = "HTTP/1.1 404 Not Found.\r\n\r\n";
	send(client_socket, bad_header, strlen(bad_header), 0); 
}

void parse_client_request(char * client_request_buffer, char * path) {
	char *start, *end;
	if ((start = strstr(client_request_buffer, "GET ")) != NULL) {
		start += 5;
		if ((end = strstr(client_request_buffer, " HTTP/")) != NULL) {
			for (char* p = start; p < end; p++) {
				*path = *p;
				path++;
			}
			*path = '\0';
			return;
		}
	}
	sprintf(path, "Bad Request!\n");
}

void process_client_request(int socket)
{
	char request_buffer[REQUEST_BUFFER_SIZE]; 
	size_t bytes_read = recv(socket, request_buffer, REQUEST_BUFFER_SIZE-1, 0); //write to a buffer, null terminated
	if (bytes_read >= REQUEST_BUFFER_SIZE) {
		perror("request buffer overflow"); 
		exit(-1);
	}
	request_buffer[bytes_read] = 0;
	char path[PATH_BUFFER_SIZE]; 
	parse_client_request(request_buffer, path);
	FILE * fp = fopen(path, "r");
	if (fp != NULL) {
		send_good_header(socket); //null ternimated
		send_content(socket, fp); // 
		fclose(fp); 
	} else {
		send_bad_header(socket);
	}
	return; 
}

int main(int argc, char* argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, (argc==0)?"80":argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			process_client_request(new_fd); 
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

