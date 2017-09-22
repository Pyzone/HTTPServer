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

#define PORT "3490" // the port client will be connecting to 



#define MAXDATASIZE 100 // max number of bytes we can get at once 



#define HTTP_PORT "80"
#define BUFFER_SIZE 4096

int STILL_READING_HEADER = 1; 


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
	size_t bytes_left = bytes_in_buffer; 
	if (STILL_READING_HEADER) {
		char * header_terminator = strstr(buffer, "\r\n\r\n");
		if (header_terminator == NULL) return 0; //"\r\n\r\n" not found yet
		else { // found "\r\n\r\n"
			bytes_left = bytes_in_buffer - (size_t)(header_terminator - buffer) -  4;
			STILL_READING_HEADER = !STILL_READING_HEADER; 
			return write(file_descriptor, header_terminator+4, bytes_left);
		} 
	} else {
		return write(file_descriptor, buffer, bytes_left);
	}
}

/**
 * use select() or poll() to check whether there is more data coming from the 
 * server side
 */
int more_data_to_come()
{

}

/**
 * Given the argument passed into the main function
 * generate the GET HTTP message 
 * the get message will be put into get message
 */
void process_argument(int argc, char ** argv, char * get_message)
{

}



int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
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

	//send GET message

	freeaddrinfo(servinfo); // all done with this structure

	while(more_data_coming()) {
		recv(); 
		/*
		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		    perror("recv");
		    exit(1);
		}
		*/
		write_to_output_file();

	}

	

	buf[numbytes] = '\0';

	printf("client: received '%s'\n",buf);

	close(sockfd);

	return 0;
}

