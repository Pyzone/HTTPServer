/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <sys/stat.h>    
#include <fcntl.h>

#define PORT "3490" // the port client will be connecting to 



#define MAXDATASIZE 100 // max number of bytes we can get at once 



#define HTTP_PORT "80"
#define BUFFER_SIZE 4096

int STILL_READING_HEADER = 1; 
int FINISHED_READING = 0; 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * After receive the data from server
 * it writes to the output file.
 * This function assumes that file_descriptor is already open and ready to write
 * buffer is the data received, file_descriptor is the fd for the function write to
 * btyes_in_buffer is number of bytes in data
 * Returns: number of bytes written to the file
 */
int write_to_output_file(char * buffer, int file_descriptor, size_t bytes_in_buffer)
{
	if (FINISHED_READING) return 0;
	size_t bytes_left = bytes_in_buffer; 
	char * start_position = buffer; 
	char * header_terminator;
	if (STILL_READING_HEADER) {
		header_terminator = strstr(buffer, "\r\n\r\n");
		if (header_terminator == NULL) return 0; //"\r\n\r\n" not found yet
		else { // found "\r\n\r\n"
			bytes_left = bytes_in_buffer - (size_t)(header_terminator - buffer) -  4;
			STILL_READING_HEADER = !STILL_READING_HEADER; 
			start_position = header_terminator + 4; 
		} 
	} 

	char * body_terminator = strstr(start_position, "\r\n\r\n");
	if (body_terminator != NULL) {
		bytes_left = (size_t) (body_terminator - start_position);
		FINISHED_READING = 1; 
	}

	return write(file_descriptor, buffer, bytes_left);
}

int there_is_a_port(char * url)
{	
	char * colon = strstr(url+7, ":");
	if (colon == NULL) return 0;
	else return 1; 
}

int get_the_port_in_the_url(char * url_raw)
{
	if (! there_is_a_port(url_raw)) return 80;

	char * url = url_raw + 7; 
	size_t start = 0, end;
	for (; start < strlen(url); start++){
		if (url[start] == ':') break;
	}
	for (end = start; end < strlen(url); end++){
		if (url[end] == '/') break; 
	}
	char port_str[8];
	int i = 0; 
	for (start += 1; start < end; start++){
		port_str[i++] = url[start];
	}
	port_str[i] = 0;
	return atoi(port_str); 
}

char * get_path_to_file_in_the_url(char * url_raw)
{
	char * url = url_raw + 7;
	return strstr(url, "/");
}

int get_hostname(char * url_raw, char * hostname){
	char * url = url_raw + 7;
	size_t i = 0;
	for (; i < strlen(url); i++){
		if (url[i] == ':' || url[i] == '/') {
			break;
		} else {
			hostname[i] = url[i];
		}
	}
	hostname[i] = 0;
	return 0;
}

/**
 * Given the argument passed into the main function
 * generate the GET HTTP message 
 * the get message will be put into get message
 */
int process_argument(int argc, char ** argv, char * get_message, char * port_str, char * hostname)
{
	if (argc != 2) return -1;
	int port = get_the_port_in_the_url(argv[1]); 
	char * path_to_file = get_path_to_file_in_the_url(argv[1]); 
	get_hostname(argv[1], hostname);
	sprintf(get_message, "GET %s HTTP/1.1\r\n\r\n", path_to_file); 
	sprintf(port_str, "%d", port);
	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd, outputfd, numbytes;  
	char buf[BUFFER_SIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	
	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	//process argument to form GET message
	process_argument(argc, argv, buf);

	//send GET message
	send(sockfd, buf, strlen(buf), 0);

	freeaddrinfo(servinfo); // all done with this structure

	//Create output file. Clear it if it already exists.
	if ((outputfd = open("output", O_WRONLY | O_CREAT | O_TRUNC, S_IWRITE)) == -1) {
		perror("open file");
		exit(1);
	}
	
	//receive http message and write the object to file.
	while( (numbytes = recv(sockfd, buf, BUFFER_SIZE, 0)) > 0 ) {

		write_to_output_file(buf, outputfd, (size_t) numbytes);

	}

	if (numbytes == -1) {
		perror("recv");
		exit(1);
	}
	
	close(outputfd);
	
	close(sockfd);

	return 0;
}

