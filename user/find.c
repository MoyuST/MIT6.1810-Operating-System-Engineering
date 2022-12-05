#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

void find(const char * path, const char * f){
  int fd;
  char buf[512], *p;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type!=T_DIR){
    fprintf(2, "find: path not directory (%s)\n", path);
    exit(1);
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
    fprintf(1, "find: path too long\n");
    exit(0);
  }

  strcpy(buf, path);
  p = buf+strlen(buf);
  *p++ = '/';

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;
    // avoid search itself
    if(strcmp(de.name, ".")==0 || strcmp(de.name, "..")==0){
      continue;
    }
    
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if(stat(buf, &st) < 0){
      fprintf(1, "find: cannot stat %s\n", buf);
      continue;
    }

    if(st.type==T_FILE){
      if(strcmp(de.name, f)==0){
        printf("%s\n", buf);
      }
    }
    else if(st.type==T_DIR){
      find(buf, f);
    }

  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc<3){
    panic("[syntax error] find (dir) (filename)");
  }

  find(argv[1], argv[2]);

  exit(0);
}
