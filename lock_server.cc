// the lock server implementation

#include "lock_server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

lock_server::lock_server() : nacquire(0) {}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid,
                                        int &r) {
  std::unique_lock<std::mutex> l(m_);

  r = nacquire;

  return lock_protocol::OK;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid,
                                           int &) {
  std::unique_lock<std::mutex> l(m_);

  while (lock_pool_.count(lid)) {
    cv_.wait(l);
  }

  lock_pool_.insert({lid, clt});

  std::cout << __PRETTY_FUNCTION__ << ": grant lock " << lid << " to " << clt
            << std::endl;

  return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid,
                                           int &) {
  std::unique_lock<std::mutex> l(m_);

  auto lock = lock_pool_.find(lid);
  if (lock != lock_pool_.end() && lock->second == clt) {
    lock_pool_.erase(lock);
    std::cout << __PRETTY_FUNCTION__ << ": release lock " << lid << " from "
              << clt << std::endl;
    l.unlock();
    cv_.notify_all();
  }

  return lock_protocol::OK;
}