#ifndef chfs_client_h
#define chfs_client_h

#include <string>
#include <vector>

#include "extent_client.h"

class chfs_client {
  extent_client *ec;

 public:
  typedef unsigned long long inum;
  enum xxstatus {
    OK,
    RPCERR,
    NOENT,  // No such file or directory
    IOERR,
    EXIST
  };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    chfs_client::inum inum;
  };
  struct syminfo {
    std::string slink;
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };

 public:
  chfs_client(std::string);

  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getsymlink(inum, syminfo &);  // TODO ?

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum, const char *);
  int mkdir(inum, const char *, mode_t, inum &);
  int symlink(inum, const char *, const char *, inum &);
  int readlink(inum, std::string &);
};

#endif
