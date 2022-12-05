#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

  if(argc<=1){
    write(2, "[syntax error] sleep time\n", 26);
    exit(0);
  }

  int sleep_time=atoi(argv[1]);

  sleep(sleep_time);
  exit(0);
}
