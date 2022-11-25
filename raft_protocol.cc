#include "raft_protocol.h"

marshall &operator<<(marshall &m, const request_vote_args &args) {
  m << args.term_;
  m << args.candidate_id_;
  m << args.weak_commit_size;
  m << args.weak_commit_term;
  return m;
}
unmarshall &operator>>(unmarshall &u, request_vote_args &args) {
  u >> args.term_;
  u >> args.candidate_id_;
  u >> args.weak_commit_size;
  u >> args.weak_commit_term;
  return u;
}

marshall &operator<<(marshall &m, const request_vote_reply &reply) {
  m << reply.term_;
  m << reply.vote_granted_;
  return m;
}

unmarshall &operator>>(unmarshall &u, request_vote_reply &reply) {
  u >> reply.term_;
  u >> reply.vote_granted_;
  return u;
}

marshall &operator<<(marshall &m, const append_entries_reply &args) {
  m << args.term_;
  m << args.success_;
  return m;
}

unmarshall &operator>>(unmarshall &m, append_entries_reply &args) {
  m >> args.term_;
  m >> args.success_;
  return m;
}

marshall &operator<<(marshall &m, const install_snapshot_args &args) {
  // Lab3: Your code here
  return m;
}

unmarshall &operator>>(unmarshall &u, install_snapshot_args &args) {
  // Lab3: Your code here
  return u;
}

marshall &operator<<(marshall &m, const install_snapshot_reply &reply) {
  // Lab3: Your code here
  return m;
}

unmarshall &operator>>(unmarshall &u, install_snapshot_reply &reply) {
  // Lab3: Your code here
  return u;
}