// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <condition_variable>
#include <mutex>
#include <string>

#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

class lock_server {
 protected:
  int nacquire;
  std::mutex m_;
  std::condition_variable cv_;
  std::map<lock_protocol::lockid_t, int> lock_pool_;

 public:
  lock_server();

  ~lock_server(){};

  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid,
                                int &ignore);

  lock_protocol::status release(int clt, lock_protocol::lockid_t lid,
                                int &ignore);
};

#endif