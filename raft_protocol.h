#ifndef raft_protocol_h
#define raft_protocol_h

#include "raft_state_machine.h"
#include "rpc.h"

enum raft_rpc_opcodes {
  op_request_vote = 0x1212,
  op_append_entries = 0x3434,
  op_install_snapshot = 0x5656
};

enum raft_rpc_status { OK, RETRY, RPCERR, NOENT, IOERR };

class request_vote_args {
 public:
  int term_;
  int candidate_id_;
  int weak_commit_size;
  int weak_commit_term;
};

marshall &operator<<(marshall &m, const request_vote_args &args);
unmarshall &operator>>(unmarshall &u, request_vote_args &args);

class request_vote_reply {
 public:
  int term_{};
  int vote_granted_{};
};

marshall &operator<<(marshall &m, const request_vote_reply &reply);
unmarshall &operator>>(unmarshall &u, request_vote_reply &reply);

template <typename command>
class log_entry {
 public:
  std::vector<command> entries_;
};

template <typename command>
marshall &operator<<(marshall &m, const log_entry<command> &entry) {
  // Lab3: Your code here
  m << entry.entries_;
  return m;
}

template <typename command>
unmarshall &operator>>(unmarshall &u, log_entry<command> &entry) {
  // Lab3: Your code here
  u >> entry.entries_;
  return u;
}

template <typename command>
class append_entries_args {
 public:
  int term_;
  int leader_id_;
  int strong_commit;
  int weak_commit_size;
  int weak_commit_term;
  log_entry<command> entries_;
};

template <typename command>
marshall &operator<<(marshall &m, const append_entries_args<command> &args) {
  // Lab3: Your code here
  m << args.term_;
  m << args.leader_id_;
  m << args.strong_commit;
  m << args.weak_commit_size;
  m << args.weak_commit_term;
  m << args.entries_;
  return m;
}

template <typename command>
unmarshall &operator>>(unmarshall &u, append_entries_args<command> &args) {
  u >> args.term_;
  u >> args.leader_id_;
  u >> args.strong_commit;
  u >> args.weak_commit_size;
  u >> args.weak_commit_term;
  u >> args.entries_;
  return u;
}

class append_entries_reply {
 public:
  int term_;
  int success_;
};

marshall &operator<<(marshall &m, const append_entries_reply &reply);
unmarshall &operator>>(unmarshall &m, append_entries_reply &reply);

class install_snapshot_args {
 public:
  int term_;
  int leader_id_;
  int strong_commit;
  std::vector<char> data;
};

marshall &operator<<(marshall &m, const install_snapshot_args &args);
unmarshall &operator>>(unmarshall &m, install_snapshot_args &args);

class install_snapshot_reply {
 public:
  int term_;
};

marshall &operator<<(marshall &m, const install_snapshot_reply &reply);
unmarshall &operator>>(unmarshall &m, install_snapshot_reply &reply);

#endif  // raft_protocol_h