#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  if(pipe(p) < 0){
    fprintf(2, "[pipe] creation failed\n");
    exit(1);
  }
  
  int pid = fork();
  char c;
  
  if(pid==-1){
    fprintf(2, "[fork] creation failed\n");
    exit(1);
  }
  else if(pid == 0){
    if(read(p[0], &c, 1)!=1){
      fprintf(2, "[read] child not recieved response\n");
      exit(1);
    }

    printf("%d: received ping\n", getpid());
    write(p[1], "a", 1);
  }
  else{
    write(p[1], "a", 1);
    wait(0);
    if(read(p[0], &c, 1)!=1){
      fprintf(2, "[read] parent not recieved response\n");
      exit(1);
    }
    printf("%d: received pong\n", getpid());
  }
  close(p[0]);
  close(p[1]);

  exit(0);
}
