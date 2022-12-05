#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define MAXARGLEN 50

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

int
main(int argc, char *argv[])
{
  if(argc<1){
    panic("[syntax error] xargs (executable) [(args)]*");
  }
  if(argc>MAXARG+2){
    fprintf(0, "argument size exceeds MAXARG %d\n", MAXARG);
    exit(1);
  }

  char args_buf[MAXARG][MAXARGLEN], c;
  int a_1=0;
  for(int i=1;i<argc;i++){
    if(sizeof(argv[i])>MAXARGLEN){
      fprintf(0, "argument size exceeds %d\n", MAXARGLEN);
      exit(1);
    }
    strcpy(args_buf[a_1++], argv[i]);
  }

  int a_end=a_1, cur_len=0, in_break=0;

  while(read(0, &c, 1)>0){
    if(c=='\n' || c==' ' || c=='\t' || c=='\v'){
      if(in_break==0){
        in_break=1;
        args_buf[a_end][cur_len]='\0';
        cur_len=0;
      }

      if(c!='\n'){
        continue;
      }

      char * args_buf_ptrs[a_end+2];
      for(int i=0;i<=a_end;i++){
        args_buf_ptrs[i] = args_buf[i];
      }
      
      args_buf_ptrs[a_end+1]=0;

      if(fork1()==0){
        exec(argv[1], args_buf_ptrs);
        exit(1);
      }
      else{
        wait(0);
        a_end=a_1, cur_len=0, in_break=0;
      }
    }
    else{
      if(in_break==1){
        in_break=0;
        a_end++;
        if(a_end>=MAXARG){
          fprintf(0, "argument (from pipe) exceeds MAXARG %d\n", MAXARG);
          exit(1);
        }
      }
      args_buf[a_end][cur_len++]=c;
      if(cur_len>=MAXARGLEN){
        fprintf(0, "argument size (from pipe) exceeds MAXARGLENG %d\n", MAXARGLEN);
        exit(1);
      }
    }
  }

  exit(0);
}
