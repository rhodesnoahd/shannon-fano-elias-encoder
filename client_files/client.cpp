// COSC 3360 Programming Assignment 2: distributed, multithreaded Shannon-Fano-Elias encoder
/** References:
 * Sockets Tutorial by Robert Ingalls,
 * fireman function by Professor Rincon,
 * programming assignment 1 by Noah Rhodes
 */
#include <iostream>
#include <map>
#include <vector>
#include <algorithm> // sort()
#include <pthread.h> // POSIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // sockets, constants and structs needed for internet domain addresses
#include <netdb.h> // defines structure hostnet, which defines a host computer on the internet
#include <unistd.h> // write(), read(), close()
#include <string.h> // bzero(), strcpy()

/** This procedure inverts map into a vector of pairs then sorts the vector by decreasing frequency, increasing
 * lexicographic order.
 * Sources: https://en.cppreference.com/w/cpp/utility/pair/make_pair
 *          https://en.cppreference.com/w/cpp/algorithm/sort
 *          https://en.cppreference.com/w/cpp/language/lambda
 *          https://thispointer.com/how-to-sort-an-array-of-structs-in-c/#using-sort-with-a-function-as-comparator */
template <typename Key, typename Value>
void sortProbLexico(std::map<Key, Value> const &_alphabet, std::vector<std::pair<Value, Key>> &_alphaVec)
{
    for (std::pair<Key, Value> const &_pair : _alphabet) {
        _alphaVec.push_back(std::make_pair(_pair.second, _pair.first)); // stores inverted pairs <Value,Key>
        // sort's complexity: O(Nlog(N))
        std::sort(_alphaVec.begin(), _alphaVec.end(),
                  // lambda expression compares (1st) freq, (2nd) settles ties lexicographically
                  [](std::pair<Value, Key> const &_leftPair, std::pair<Value, Key> const &_rightPair) {
                      bool result = true;
                      if ((_leftPair.first < _rightPair.first)  ||
                         ((_leftPair.first == _rightPair.first) && (_leftPair.second > _rightPair.second)))
                          result = false;
                      return result;
                  });
    }
}

struct threadArgs {
    char* hostName;
    char* portNumber;
    double* probArr;
    int index;
    std::string code;
    threadArgs() {hostName = nullptr; portNumber = nullptr; probArr = nullptr; index = -1; code = "";}
};

void *threadSubroutine(void *void_arg_ptr) {
    threadArgs *ptr = (threadArgs *)void_arg_ptr;
    double cumulativeProb = 0.0;
    for(int i=0; i<ptr->index; i++) // for PRIOR symbols
        cumulativeProb += ptr->probArr[i]; // add p(xsubi)
    cumulativeProb += (1/2.0) * ptr->probArr[ptr->index]; // for CURRENT sybmol add 1/2 * p(x)
    
    int sockfd, portno, n;
    struct sockaddr_in serv_addr; // will contain the address of the server to which we want to connect
    struct hostent *server;
    portno = atoi(ptr->portNumber);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        std::cerr << "ERROR opening socket\n";
    server = gethostbyname(ptr->hostName);
    if (server == nullptr)
        std::cerr << "ERROR host not found\n";
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // bc server->h_addr is a character string we used bcopy()
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "ERROR connecting\n";
        exit(1);
    }

    static const int SYMBOL_INFO = 2; // size of info always 2
    double *symbInfo = new double[SYMBOL_INFO];
    bzero(symbInfo, SYMBOL_INFO + 1);
    symbInfo[0] = ptr->probArr[ptr->index]; // probability
    symbInfo[1] = cumulativeProb; // cumulative probability
    
    // send symbol's info to server
    n = write(sockfd, symbInfo, SYMBOL_INFO * sizeof(double));
    if (n < 0) {
        std::cerr << "ERROR writing to socket\n";
        exit(1);
    }
    delete [] symbInfo;
    
    // read length of code
    int codeLength = 0;
    n = read(sockfd, &codeLength, sizeof(int));
    if (n < 0) {
        std::cerr << "ERROR reading from socket in client";
        exit(1);
    }

    char* codeArr = new char[codeLength + 1];
    bzero(codeArr, codeLength + 1);
    // read code
    n = read(sockfd, codeArr, codeLength * sizeof(char));
    if (n < 0) {
        std::cerr << "ERROR reading from socket in client";
        exit(1);
    }
    ptr->code = codeArr;

    delete [] codeArr; close(sockfd); return nullptr;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "usage " << argv[0] << "hostname port\n";
        exit(0);
    }
    // parse input
    std::map<char, int> alphabet; // convenient for determining alphabet & frequencies
    std::string line;
    getline(std::cin, line);
    int numChars = 0;
    for (char &symbol : line) {
        alphabet[symbol]++;
        numChars++; // increment total count to determine probablility
    }
    std::vector<std::pair<int, char>> alphaVec;
    sortProbLexico(alphabet, alphaVec);
    int NTHREADS = alphaVec.size(); // NTHREADS = number of symbols, frequencies, threads
    double *probabilites = new double[NTHREADS];

    // populate probabilites array
    for(int i=0; i < NTHREADS; i++) {
        probabilites[i] = ( alphaVec.at(i).first / (double)numChars);
    }
    // create threads
    pthread_t *tid = new pthread_t[NTHREADS];
    std::vector<threadArgs> v(NTHREADS);
    for (int i = 0; i < NTHREADS; i++) {
        v.at(i).hostName = argv[1];
        v.at(i).portNumber = argv[2];
        v.at(i).probArr = &probabilites[0];
        v.at(i).index = i;
        if (pthread_create(&tid[i], nullptr, threadSubroutine, &v.at(i)) != 0) {
            std::cerr << "Error creating thread\n";
            return 1;
        }
    }
    // join threads
    for(int i=0; i<NTHREADS; i++)
        pthread_join(tid[i], nullptr);
    // output
    std::cout << "SHANNON-FANO-ELIAS Codes:\n\n";
    for(int i=0; i<NTHREADS; i++)
    std::cout << "Symbol " << alphaVec.at(i).second << ", Code: " << v.at(i).code << '\n';

    delete [] probabilites; delete [] tid; return 0;
}