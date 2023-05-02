#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <sys/wait.h>

#define MESSAGESIZE 500

struct message
{
    char message[MESSAGESIZE];
    char symbol;
};

void fireman(int)
{
   while (waitpid(-1, NULL, WNOHANG) > 0) /** waitpid wait for a particular process based on pid value. -1 waits for ANY child
   process, using waitpid bc need last arg WNOHAND so that we are not suspended */
      std::cout << "A child process ended" << std::endl;
}

int main()
{

   signal(SIGCHLD, fireman); /** called the moment a child process ends its execution */       
   while (true)
   {
      
      if (fork() == 0)
      {
         std::cout << "A child process started" << std::endl;
         sleep(5);
         _exit(0);
      }
      std::cout << "Press enter to continue" << std::endl;
      std::cin.get(); /** even though the parent process is here waiting for standard input, the fireman function is 
      called automatically when a child process ends its execution 
      */
   }
   return 0;
}