#include "kernel/types.h"
#include "user/user.h"

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

void feed_next(int p_left){
  int feed_int=0, p[2], divider;
  unsigned char forked=0;
  if(read(p_left, &feed_int, 4)<=0){
    exit(0);
  }

  if(feed_int==0){
    panic("prime is zero");
  }

  fprintf(1, "prime %d\n", feed_int);
  divider=feed_int;

  while(read(p_left, &feed_int, 4)==4){
    if(forked==0){
      forked=1;
      if(pipe(p) < 0){
        panic("pipe");
      }
      int pid = fork1();
      if(pid==0){
        close(p[1]);
        close(p_left);
        feed_next(p[0]);
        close(p[0]);
        exit(1);
      }
      else{
        close(p[0]);
      }
    }

    if(feed_int%divider!=0){
      write(p[1], &feed_int, 4);
    }
  }
  close(p[1]);
  wait(0);
  close(p_left);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  if(pipe(p) < 0){
    panic("pipe");
  }
  
  int pid = fork1();
  
  if(pid == 0){
    close(p[1]);
    feed_next(p[0]);
    close(p[0]);
    exit(1);
  }
  else{
    close(p[0]);
    for(int i=2;i<=35;i++){
      write(p[1], &i, 4);
    }
    close(p[1]);
    wait(0);
  }

  exit(0);
}
