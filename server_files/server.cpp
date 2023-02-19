// COSC 3360 Programming Assignment 2: distributed, multithreaded Shannon-Fano-Elias encoder
/** References:
 * Sockets Tutorial by Robert Ingalls,
 * fireman function by Professor Rincon,
 * programming assignment 1 by Noah Rhodes
 */
#include <iostream>
#include <sys/types.h> // data types used in system calls
#include <sys/socket.h> // structures needed for sockets
#include <sys/wait.h>
#include <netinet/in.h> // constants and structs needed for internet domain addresses
#include <unistd.h> // fork(), read(), write(), close(), _exit(0)
#include <cmath> // log2(), ceil()
#include <string.h> // bzero(), strcpy()

void fireman(int) {
   /** wait for a particular process based on pid value. -1 waits for ANY child process, 
   using waitpid bc need last arg WNOHANG so that we are not suspended */
   while (waitpid(-1, NULL, WNOHANG) > 0) 
      std::cout << ""; // "A child process ended\n"
}

int Length(double _prob) { // length of encoding
    return ceil(log2(1 / _prob)) + 1;
};

std::string BinaryExpansion(int _length, double _cumProb) { // binary expansion of Fbar
    std::string binCode = "";
    for(int i=0; i<_length; i++) {
        _cumProb *= 2;
        if(_cumProb >= 1) {
            _cumProb -= 1;
            binCode += "1";
        }
        else if(_cumProb < 1)
            binCode += "0";
    }
    return binCode;
};

int main(int argc, char* argv[]) {
   // if user fails to pass in port number, exit
   if(argc < 2) {
      std::cerr << "ERROR: No port provided\n";
      exit(1);
   }
   int sockfd, newsockfd; // file descriptors. store values returned by the socket system call and the accept system call
   int portno; // stores the port number on which the server accepts connections
   int clilen; // stores the size of the address of the client. needed for accept system call
   int n; // return value for the read() & write() calls. contains the number of characters read or written
   struct sockaddr_in serv_addr, cli_addr; /** sockaddr_in is structure containing internet address.
   serv_addr will contain address of server. cli_addr will contain the address of the client which connects to server */
   
   /** socket() system call creates a new socket. 
    *takes 3 arguments:
    *    1. address domain of socket. 2 choices: AF_INET for any 2 hosts on the internet
    *       or AF_UNIX for 2 processes which share a common file system
    *    2. type of socket. 2 choices: SOCK_STREAM where characters are read in a continuous stream as if from file or pipe
    *       or SOCK_DGRAM where messages are read in chunks
    *    3. protocol. if 0, operating system chooses appropriately (TCP for SOCK_STREAM and UDP for SOCK_DGRAM) */
   sockfd = socket(AF_INET, SOCK_STREAM, 0); // Internet domain, stream socket, TCP protocol chosen by OS
   
   /** socket() system call returns an entry into the file descriptor table. this value used for all subsequent references
    * to this socket. if socket call fails, returns -1. */
   if(sockfd < 0)
      std::cerr << "ERROR opening socket\n";    
   
   bzero((char*)&serv_addr, sizeof(serv_addr)); // initializes all values in serv_addr to 0s
   portno = atoi(argv[1]); // assigns port number using atoi to convert string of digits into integer
   serv_addr.sin_family = AF_INET; // struct sockaddr_in has 4 fields, short sin_family contains a code for the address family
   serv_addr.sin_port = htons(portno); /** second field of sockaddr_in is unsigned short sin_port which contains port number.
   need to convert portno to network byte order using htons() which converts port number in host byte order to port number
   in network byte order */
   serv_addr.sin_addr.s_addr = INADDR_ANY; /** third field of sockaddr_in is struct in_addr which contains a single field 
   unsigned long s_addr. this contains IP address of host. for server code this is always the IP address of the 
   machine on which the server is running. the symbolic constant INADDR_ANY gets this address. */
   
   /** bind() system call binds a socket to an address. 
    * takes 3 arguments:
    *    1. socket file descriptor
    *    2. address to which it is bound. &serv_addr is type sockaddr_in, so it must be cast to type (sockaddr*)
    *    3. size of address to which it is bound 
    * Most likey reason for failure is socket already in use on this machine */
   if(bind(sockfd, (sockaddr*) &serv_addr, sizeof(serv_addr))) {
      std::cerr << "ERROR on binding\n";
      exit(1);
   }
   
   /** listen() system call allows the process to listen on the socket for connections. 
    * takes 2 arguments:
    *    1. socket file descriptor
    *    2. size of backlog queue, which is the number of connections that can be waiting while the process is handling a 
    *       particular connection. should be set to 5, which is max size for most systems */
   listen(sockfd, 5);
   signal(SIGCHLD, fireman); // called the moment a child process ends its execution
   clilen = sizeof(cli_addr); // size of address of client. needed for accept() system call
   
   while(true) { /** Moodle kills server program after each test case -> we can use infinite loop */
      /** accept() system call causes the process block until a client connects to the sever. "wakes up" the process when a 
       * connection from the client has been successfully established.
       * takes 3 arguments:
       *    1. (returns a) new file descriptor. all communication on this connection should be done using this new file descriptor.
       *    2. reference pointer to the address of the client on other end of connection
       *    3. size of address of client. clilen is type int -> must be type cast as (socklen_t*)
       */
      newsockfd = accept(sockfd,(sockaddr*) &cli_addr, (socklen_t*) &clilen);
      if(fork() == 0) { // create child process per request received from client
         if(newsockfd < 0) {
            std::cerr << "ERROR on accept\n";
            exit(1);
         }
         // only get here after successful connection. once this happens, both ends can read/write to connection

         // size of info always 2
         static const int SYMBOL_INFO = 2;
         double *symbInfo = new double[SYMBOL_INFO];
         bzero(symbInfo, SYMBOL_INFO + 1);
         n = read(newsockfd, symbInfo, SYMBOL_INFO * sizeof(double)); /** read from socket using newsockfd, blocks until client 
         executes a write */
         if(n < 0) {
            std::cerr << "ERROR reading from socket\n";
            exit(1);
         }

         // calculate code length
         int length = Length(symbInfo[0]); // symbInfo[0] is probability
         // generate code        
         std::string code = BinaryExpansion(length, symbInfo[1]); // symbInfo[1] is cumulative probability
         delete [] symbInfo;
         char* codeArr = new char[length];
         bzero(codeArr, length + 1);
         strcpy(codeArr, code.c_str());

         // send length of code
         n = write(newsockfd, &length, sizeof(int));
         if(n<0) {
            std::cerr << "ERROR writing to socket";
            exit(1);
         }

         // send code
         n = write(newsockfd, codeArr, length /** sizeof(char)*/);
         if (n < 0) {
               std::cerr << "ERROR writing to socket";
               exit(1);
         }

         delete [] codeArr; length = 0; close(newsockfd); _exit(0);
      }
   }
   close(sockfd); return 0;
}