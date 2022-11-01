// the lock server implementation

#include "lock_server.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

lock_server::lock_server() : nacquire(0) {}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid,
                                        int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid,
                                           int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2B part2 code goes here
  return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid,
                                           int &r) {
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2B part2 code goes here
  return ret;
}