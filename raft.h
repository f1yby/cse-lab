#ifndef raft_h
#define raft_h

#include <stdarg.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <set>
#include <thread>

#include "raft_protocol.h"
#include "raft_state_machine.h"
#include "raft_storage.h"
#include "rpc.h"

template <typename state_machine, typename command>
class raft {
  static_assert(std::is_base_of<raft_state_machine, state_machine>(),
                "state_machine must inherit from raft_state_machine");
  static_assert(std::is_base_of<raft_command, command>(),
                "command must inherit from raft_command");

  friend class thread_pool;

  //#define RAFT_LOG(fmt, args...) \
//    do {                       \
//    } while (0);

#define RAFT_LOG(fmt, args...)                                                 \
  do {                                                                         \
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(          \
                   std::chrono::system_clock::now().time_since_epoch())        \
                   .count();                                                   \
    printf("[%ld][%s:%d][node %d term %d] " fmt "\n", now, __FILE__, __LINE__, \
           my_id, current_term, ##args);                                       \
  } while (0);

 public:
  raft(rpcs *rpc_server, std::vector<rpcc *> rpc_clients, int idx,
       raft_storage<command> *storage, state_machine *state);
  ~raft();

  // start the raft node.
  // Please make sure all of the rpc request handlers have been registered
  // before this method.
  void start();

  // stop the raft node.
  // Please make sure all of the background threads are joined in this method.
  // Notice: you should check whether is server should be stopped by calling
  // is_stopped().
  //         Once it returns true, you should break all of your long-running
  //         loops in the background threads.
  void stop();

  // send a new command to the raft nodes.
  // This method returns true if this raft node is the leader that successfully
  // appends the log. If this node is not the leader, returns false.
  bool new_command(command cmd, int &term, int &index);

  // returns whether this node is the leader, you should also set the current
  // term;
  bool is_leader(int &term);

  // save a snapshot of the state machine and compact the log.
  bool save_snapshot();

 private:
  std::mutex mtx;  // A big lock to protect the whole data structure
  ThrPool *thread_pool;
  raft_storage<command> *storage;  // To persist the raft log
  state_machine
      *state;  // The state machine that applies the raft log, e.g. a kv store

  rpcs *rpc_server;  // RPC server to recieve and handle the RPC requests
  std::vector<rpcc *>
      rpc_clients;  // RPC clients of all raft nodes including this node

  std::atomic_bool stopped;

  enum raft_role { follower, candidate, leader };

  std::thread *background_election;
  std::thread *background_ping;
  std::thread *background_commit;
  std::thread *background_apply;

  // Your code here:

  /* ----Persistent state on all server----  */
  ///@brief The index of this node in rpc_clients, start from 0
  int my_id;
  raft_role role = follower;
  int current_term = 0;

  ///@note If node has leader,leader id is positive and equals leader's id, else
  /// leader id is -1
  int leader_id = -1;

  ///@note If node has voted for other node in current term, vote for equals to
  /// voted node's id, else vote for is -1
  int vote_for = -1;
  int weak_commit = 0;
  int strong_commit = 0;
  int weak_commit_term = 0;

  int committed_in_memory;

  /* ---- Volatile state on all server----  */
  std::set<int> vote_for_me{};
  std::chrono::time_point<std::chrono::system_clock> last_seen_leader =
      std::chrono::system_clock::now();
  std::vector<command> entries{};

  /* ---- Volatile state on leader----  */
  std::set<int> commit_success;
  std::set<int> need_snapshot;

 private:
  // RPC handlers
  int request_vote(request_vote_args arg, request_vote_reply &reply);

  int append_entries(append_entries_args<command> arg,
                     append_entries_reply &reply);

  int install_snapshot(install_snapshot_args arg,
                       install_snapshot_reply &reply);

  // RPC helpers
  void send_request_vote(int target, request_vote_args arg);
  void handle_request_vote_reply(int target, const request_vote_args &arg,
                                 const request_vote_reply &reply);

  void send_append_entries(int target, append_entries_args<command> arg);
  void handle_append_entries_reply(int target,
                                   const append_entries_args<command> &arg,
                                   const append_entries_reply &reply);

  void send_install_snapshot(int target, install_snapshot_args arg);
  void handle_install_snapshot_reply(int target,
                                     const install_snapshot_args &arg,
                                     const install_snapshot_reply &reply);

 private:
  bool is_stopped();
  int num_nodes() { return rpc_clients.size(); }

  // background workers

  ///@brief Periodically send empty append_entries RPC to the followers.
  ///@note Only work for the leader.
  void run_background_ping();

  ///@brief Periodically check the liveness of the leader.
  ///@note Work for followers and candidates.
  void run_background_election();

  ///@brief Periodically send logs to the follower.
  ///@note Only work for the leader.
  void run_background_commit();

  ///@brief Periodically apply committed logs to the state machine
  ///@note Work for all the nodes.
  void run_background_apply();

  // Your code here:
  void check_term(int term);
  void init_new_term();
};

template <typename state_machine, typename command>
raft<state_machine, command>::raft(rpcs *server, std::vector<rpcc *> clients,
                                   int idx, raft_storage<command> *storage,
                                   state_machine *state)
    : stopped(false),
      rpc_server(server),
      rpc_clients(clients),
      my_id(idx),
      storage(storage),
      state(state),
      background_election(nullptr),
      background_ping(nullptr),
      background_commit(nullptr),
      background_apply(nullptr),
      current_term(0),
      role(follower) {
  thread_pool = new ThrPool(32);

  // Register the rpcs.
  rpc_server->reg(raft_rpc_opcodes::op_request_vote, this, &raft::request_vote);
  rpc_server->reg(raft_rpc_opcodes::op_append_entries, this,
                  &raft::append_entries);
  rpc_server->reg(raft_rpc_opcodes::op_install_snapshot, this,
                  &raft::install_snapshot);

  // Your code here:
  // Do the initialization

  // Load data
  if (storage->valid) {
    role = (raft_role)storage->role;
    current_term = storage->current_term;
    leader_id = storage->leader_id;
    vote_for = storage->vote_for;
    weak_commit = storage->weak_commit;
    strong_commit = storage->strong_commit;
    weak_commit_term = storage->weak_commit_term;
    entries = storage->weak;
    if (!storage->data.empty()) {
      state->apply_snapshot(storage->data);
    }
  } else {
    storage->role = role;
    storage->current_term = current_term;
    storage->leader_id = leader_id;
    storage->vote_for = vote_for;
    storage->weak_commit = weak_commit;
    storage->strong_commit = strong_commit;
    storage->weak_commit_term = weak_commit_term;
    storage->strong_commit_size = 0;
    storage->flush_metadata();
  }

  committed_in_memory = 0;

  RAFT_LOG("init");
}

template <typename state_machine, typename command>
raft<state_machine, command>::~raft() {
  delete background_ping;
  delete background_election;
  delete background_commit;
  delete background_apply;
  delete thread_pool;
}

/******************************************************************

                        Public Interfaces

*******************************************************************/

template <typename state_machine, typename command>
void raft<state_machine, command>::stop() {
  stopped.store(true);
  background_ping->join();
  background_election->join();
  background_commit->join();
  background_apply->join();
  thread_pool->destroy();
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::is_stopped() {
  return stopped.load();
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::is_leader(int &term) {
  term = current_term;
  return role == leader;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::start() {
  // Lab3: Your code here

  RAFT_LOG("start");
  this->background_election =
      new std::thread(&raft::run_background_election, this);
  this->background_ping = new std::thread(&raft::run_background_ping, this);
  this->background_commit = new std::thread(&raft::run_background_commit, this);
  this->background_apply = new std::thread(&raft::run_background_apply, this);
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::new_command(command cmd, int &term,
                                               int &index) {
  std::unique_lock<std::mutex> l(mtx);
  if (role != leader) {
    return false;
  }
  term = current_term;

  entries.push_back(cmd);
  storage->flush_log(entries, committed_in_memory);
  index = entries.size() - committed_in_memory + strong_commit;
  RAFT_LOG("receive new %d", index);

  return true;
}

template <typename state_machine, typename command>
bool raft<state_machine, command>::save_snapshot() {
  // Lab3: Your code here
  return true;
}

/******************************************************************

                         RPC Related

*******************************************************************/
template <typename state_machine, typename command>
int raft<state_machine, command>::request_vote(request_vote_args args,
                                               request_vote_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(args.term_);

  // Hard Reject
  if (args.weak_commit_term < weak_commit_term ||
      args.weak_commit_size < weak_commit || leader_id != -1 ||
      args.term_ < current_term) {
    reply = request_vote_reply{current_term, -2};
    return 0;
  }

  // Soft Reject
  if ((vote_for != args.candidate_id_ && vote_for != -1)) {
    reply = request_vote_reply{current_term, -1};
    return 0;
  }

  storage->vote_for = vote_for;
  storage->flush_metadata();
  vote_for = args.candidate_id_;
  reply = request_vote_reply{current_term, vote_for};
  RAFT_LOG("grant vote to %d", args.candidate_id_);
  return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(
    int target, const request_vote_args &arg, const request_vote_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(reply.term_);
  RAFT_LOG("receive vote from %d in term %d", target, reply.term_);

  // Vote from old term
  if (reply.term_ < current_term) {
    return;
  }

  // Hard reject
  if (reply.vote_granted_ == -2) {
    storage->role = follower;
    storage->flush_metadata();
    role = follower;
    vote_for_me.clear();
    return;
  }

  // Soft reject
  if (reply.vote_granted_ == -1) {
    vote_for_me.erase(target);
    return;
  }

  if (role == leader) {
    return;
  }

  vote_for_me.insert(target);

  if (vote_for_me.size() > rpc_clients.size() / 2) {
    // Got majority votes, become the leader
    storage->role = leader;
    storage->flush_metadata();
    role = leader;
    RAFT_LOG("become leader")
    vote_for_me.clear();
    return;
  }
}

template <typename state_machine, typename command>
int raft<state_machine, command>::append_entries(
    append_entries_args<command> arg, append_entries_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(arg.term_);

  last_seen_leader = std::chrono::system_clock::now();

  // Sender is out-dated
  if (arg.term_ < current_term) {
    RAFT_LOG("sender is outdated");
    reply.term_ = current_term;
    reply.success_ = false;
    return 0;
  }

  // Receiver is kicked back into follower
  if (role == candidate) {
    storage->role = follower;
    storage->leader_id = arg.leader_id_;
    storage->flush_metadata();
    role = follower;
    leader_id = arg.leader_id_;
  }

  // Data are outdated
  ///@note leader always has the newest committed log
  if (arg.strong_commit > strong_commit &&
      (arg.weak_commit_size != weak_commit ||
       arg.weak_commit_term != weak_commit_term)) {
    RAFT_LOG("data is outdated");
    reply.term_ = current_term;
    reply.success_ = false;
    return 0;
  }

  // Commits are up-to-date
  if (arg.weak_commit_size == weak_commit &&
      arg.weak_commit_term == weak_commit_term) {
    // Try Apply
    if (arg.strong_commit > strong_commit) {
      for (int i = 0; i < arg.strong_commit - strong_commit; ++i) {
        state->apply_log(entries[i + committed_in_memory]);
      }
      storage->data = state->snapshot();
      storage->flush_snapshot();

      storage->strong_commit = arg.strong_commit;
      storage->update_strong_commit_size(entries, committed_in_memory,
                                         arg.strong_commit - strong_commit);
      storage->flush_metadata();

      committed_in_memory += arg.strong_commit - strong_commit;
      strong_commit = arg.strong_commit;

      RAFT_LOG("apply: %d -> %d", strong_commit, arg.strong_commit);
    }

    reply.term_ = current_term;
    reply.success_ = true;
    return 0;
  }

  // Update Commit
  if (strong_commit == arg.strong_commit) {
    if (arg.entries_.entries_.size() > entries.size() - committed_in_memory) {
      entries.resize(arg.entries_.entries_.size() + committed_in_memory);
    }

    for (int i = committed_in_memory, j = 0; j < arg.entries_.entries_.size();
         ++i, ++j) {
      entries[i] = arg.entries_.entries_[j];
    }

    storage->flush_log(entries, committed_in_memory);
    storage->weak_commit = arg.weak_commit_size;
    storage->flush_metadata();

    RAFT_LOG("commit: %d -> %d", weak_commit, arg.weak_commit_size);

    weak_commit = arg.weak_commit_size;

    reply.term_ = current_term;
    reply.success_ = true;

    return 0;
  }

  reply.term_ = current_term;
  reply.success_ = true;
  return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(
    int target, const append_entries_args<command> &arg,
    const append_entries_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(reply.term_);

  // Reply from previous term, ignore
  if (reply.term_ < current_term) {
    RAFT_LOG("reply from previous term %d", reply.term_);
    return;
  }

  if (reply.success_ == false) {
    RAFT_LOG("%d need snapshot", target);
    need_snapshot.insert(target);
    return;
  }

  if (arg.weak_commit_size == weak_commit) {
    commit_success.insert(target);
  }

  if (commit_success.size() > rpc_clients.size() / 2) {
    commit_success.clear();
    // Commit new
    if (strong_commit == weak_commit) {
      storage->flush_log(entries, committed_in_memory);

      storage->weak_commit =
          entries.size() - committed_in_memory + strong_commit;
      storage->flush_metadata();

      weak_commit = storage->weak_commit;

      RAFT_LOG("commit %d -> %d", strong_commit, weak_commit);
    }
    // Apply committed
    else {
      for (auto i = committed_in_memory;
           i < weak_commit - strong_commit + committed_in_memory; ++i) {
        state->apply_log(entries[i]);
      }
      storage->data = state->snapshot();
      storage->flush_snapshot();

      storage->strong_commit = weak_commit;
      storage->update_strong_commit_size(entries, committed_in_memory,
                                         weak_commit - strong_commit);
      storage->flush_metadata();

      committed_in_memory += weak_commit - strong_commit;
      strong_commit = weak_commit;

      RAFT_LOG("apply: %d -> %d", strong_commit, weak_commit);
    }
  }
}

template <typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(
    install_snapshot_args args, install_snapshot_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(args.term_);
  if (args.term_ < current_term) {
    reply.term_ = current_term;
    return 0;
  }

  storage->data = args.data;
  storage->flush_snapshot();

  storage->clear_log();

  storage->strong_commit = args.strong_commit;
  storage->strong_commit_size = 0;
  storage->flush_metadata();

  state->apply_snapshot(args.data);

  reply.term_ = current_term;

  RAFT_LOG("install snapshot: %d -> %d", strong_commit, args.strong_commit)

  strong_commit = args.strong_commit;

  return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(
    int node, const install_snapshot_args &arg,
    const install_snapshot_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(reply.term_);
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_request_vote(int target,
                                                     request_vote_args arg) {
  request_vote_reply reply;
  if (rpc_clients[target]->call(raft_rpc_opcodes::op_request_vote, arg,
                                reply) == 0) {
    handle_request_vote_reply(target, arg, reply);
  } else {
    // RPC fails
  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_append_entries(
    int target, append_entries_args<command> arg) {
  append_entries_reply reply{};
  if (rpc_clients[target]->call(raft_rpc_opcodes::op_append_entries, arg,
                                reply) == 0) {
    handle_append_entries_reply(target, arg, reply);
  } else {
    // RPC fails
  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::send_install_snapshot(
    int target, install_snapshot_args arg) {
  install_snapshot_reply reply;
  if (rpc_clients[target]->call(raft_rpc_opcodes::op_install_snapshot, arg,
                                reply) == 0) {
    handle_install_snapshot_reply(target, arg, reply);
  } else {
    // RPC fails
  }
}

/******************************************************************

                        Background Workers

*******************************************************************/

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_election() {
  while (true) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(97 * my_id % 200 + 200));
    if (is_stopped()) {
      return;
    }

    std::unique_lock<std::mutex> l(mtx);

    // Follower becomes a candidate
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - last_seen_leader)
            .count() > 1000) {
      RAFT_LOG("timeout start new vote")
      storage->role = candidate;
      storage->flush_metadata();
      role = candidate;

      // Start new vote
      storage->current_term = current_term + 1;
      storage->vote_for = my_id;
      storage->flush_metadata();
      ++current_term;

      init_new_term();

      vote_for_me.insert(my_id);

      storage->vote_for = my_id;
      storage->flush_metadata();
      vote_for = my_id;

      auto args = request_vote_args{
          current_term,
          my_id,
          weak_commit,
          weak_commit_term,
      };
      for (auto target = 0; target < rpc_clients.size(); ++target) {
        thread_pool->template addObjJob(this, &raft::send_request_vote, target,
                                        args);
      }
    }
  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_commit() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (is_stopped()) return;
    std::unique_lock<std::mutex> l(mtx);

    // Only leader can commit
    if (role != leader) {
      continue;
    }

    auto args = append_entries_args<command>{
        current_term,
        my_id,
        strong_commit,
        weak_commit,
        weak_commit_term,
        {{entries.begin() + committed_in_memory, entries.end()}},
    };

    auto snapshot_args = install_snapshot_args{
        current_term,
        my_id,
        strong_commit,
        state->snapshot(),
    };

    for (auto target = 0; target < rpc_clients.size(); ++target) {
      if (need_snapshot.count(target)) {
        thread_pool->addObjJob(this, &raft::send_install_snapshot, target,
                               snapshot_args);
        need_snapshot.erase(target);
        continue;
      }

      thread_pool->addObjJob(this, &raft::send_append_entries, target, args);
    }
  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {}

/******************************************************************

                        Other functions

*******************************************************************/

template <typename state_machine, typename command>
void raft<state_machine, command>::check_term(int term) {
  if (term > current_term) {
    storage->current_term = term;
    storage->flush_metadata();
    current_term = term;

    init_new_term();
  }
}
template <typename state_machine, typename command>
void raft<state_machine, command>::init_new_term() {
  vote_for_me.clear();

  storage->vote_for = -1;
  storage->leader_id = -1;
  storage->role = follower;
  storage->flush_metadata();
  vote_for = -1;
  leader_id = -1;
  role = follower;
  commit_success.clear();
}

#endif  // raft_h
