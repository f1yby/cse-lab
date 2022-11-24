#ifndef raft_storage_h
#define raft_storage_h

#include <fcntl.h>

#include <mutex>

#include "raft_protocol.h"

template <typename command>
class raft_storage {
 public:
  raft_storage(const std::string& file_dir);
  ~raft_storage();
  // Lab3: Your code here

  //  void append(command );
  //  void truncate(int);
  int size() {
    // TODO read from meta file
    return 0;
  }

 private:
  std::mutex mtx;
  // Lab3: Your code here
};

template <typename command>
raft_storage<command>::raft_storage(const std::string& dir) {
  // Lab3: Your code here
}

template <typename command>
raft_storage<command>::~raft_storage() {
  // Lab3: Your code here
}

#endif  // raft_storage_h