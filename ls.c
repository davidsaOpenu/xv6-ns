#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "param.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char cg_file_name[MAX_CGROUP_FILE_NAME_LENGTH];
  char proc_file_name[MAX_PROC_FILE_NAME_LENGTH];

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;

  case T_CGFILE:
    printf(1, "%s %d %d\n", fmtname(path), st.type, st.size);
    break;

  case T_CGDIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, cg_file_name, sizeof(cg_file_name)) == MAX_CGROUP_FILE_NAME_LENGTH && cg_file_name[0] != ' '){
      memmove(p, cg_file_name, MAX_CGROUP_FILE_NAME_LENGTH);
      p[MAX_CGROUP_FILE_NAME_LENGTH] = 0;
      int i = MAX_CGROUP_FILE_NAME_LENGTH - 1;
      while (p[i] == ' ')
          i--;
      p[i + 1] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      p[i + 1] = ' ';
      printf(1, "%s %d %d\n", fmtname(buf), st.type, st.size);
    }
    break;

  case T_PROCFILE:
    printf(1, "%s %d %d\n", fmtname(path), st.type, st.size);
    break;

  case T_PROCDIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, proc_file_name, sizeof(proc_file_name)) == MAX_PROC_FILE_NAME_LENGTH && proc_file_name[0] != ' '){
      memmove(p, proc_file_name, MAX_PROC_FILE_NAME_LENGTH);
      p[MAX_PROC_FILE_NAME_LENGTH] = 0;
      int i = MAX_PROC_FILE_NAME_LENGTH - 1;
      while (p[i] == ' ')
          i--;
      p[i + 1] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      p[i + 1] = ' ';
      printf(1, "%s %d %d\n", fmtname(buf), st.type, st.size);
    }
    break;

  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(1);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
