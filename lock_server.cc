// the lock server implementation

#include "lock_server.h"

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

  std::cout << __PRETTY_FUNCTION__ << ": clt " << clt << " try to get lock "
            << lid << std::endl;

  while (lock_pool_.count(lid)) {
    cv_.wait(l);
  }

  lock_pool_.insert({lid, clt});

  std::cout << __PRETTY_FUNCTION__ << ": clt " << clt << " get lock " << lid
            << std::endl;

  return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid,
                                           int &) {
  std::unique_lock<std::mutex> l(m_);

  auto lock = lock_pool_.find(lid);
  if (lock != lock_pool_.end() && lock->second == clt) {
    lock_pool_.erase(lock);
    std::cout << __PRETTY_FUNCTION__ << ": clt " << clt << " release lock "
              << lid << std::endl;
    l.unlock();
    cv_.notify_all();
  }

  return lock_protocol::OK;
}