#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extent_server.h"
#include "rpc.h"

// Main loop of extent server

int main(int argc, char *argv[]) {
  int count = 0;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  char *count_env = getenv("RPC_COUNT");
  if (count_env != NULL) {
    count = atoi(count_env);
  }

  rpcs server(atoi(argv[1]), count);
  extent_server ls;

  server.reg(extent_protocol::get, &ls, &extent_server::get);
  server.reg(extent_protocol::getattr, &ls, &extent_server::getattr);
  server.reg(extent_protocol::put, &ls, &extent_server::put);
  server.reg(extent_protocol::remove, &ls, &extent_server::remove);
  server.reg(extent_protocol::create, &ls, &extent_server::create);
  server.reg(extent_protocol::start_tx, &ls, &extent_server::start_tx);
  server.reg(extent_protocol::abort_tx, &ls, &extent_server::abort_tx);
  server.reg(extent_protocol::commit_tx, &ls, &extent_server::commit_tx);

  while (1) {
    sleep(1000);
  }
}
