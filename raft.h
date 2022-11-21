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

  ///@if node has leader,leader id is positive and equals leader's id
  ///@else leader id is -1
  int leader_id = -1;

  ///@if node has voted for other node in current term, vote for equals to voted
  /// node's id
  ///@else vote for is -1
  int vote_for = -1;
  int leader_commit = 0;

  /* ---- Volatile state on all server----  */
  int prev_log_term = 0;
  int prev_log_index = 0;
  std::set<int> vote_for_me{};
  std::chrono::time_point<std::chrono::system_clock> last_seen_leader =
      std::chrono::system_clock::now();

  /* ---- Volatile state on leader----  */

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
  // Lab3: Your code here
  term = current_term;
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
  if (args.last_log_term_ < prev_log_term ||
      args.last_log_index_ < prev_log_index || leader_id != -1 ||
      args.term_ < current_term) {
    reply = request_vote_reply{.term_ = current_term, .vote_granted_ = -2};
    return 0;
  }

  // Soft Reject
  if ((vote_for != args.candidate_id_ && vote_for != -1)) {
    reply = request_vote_reply{.term_ = current_term, .vote_granted_ = -1};
    return 0;
  }

  vote_for = args.candidate_id_;
  reply = request_vote_reply{.term_ = current_term, .vote_granted_ = vote_for};
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
  last_seen_leader = std::chrono::system_clock::now();

  check_term(arg.term_);
  // Sender is out-dated
  if (arg.term_ < current_term) {
    RAFT_LOG("sender is outdated");
    reply.term_ = current_term;
    reply.success_ = false;
    return 0;
  }

  // Receiver is kicked back into follower
  if (role == candidate && arg.term_ == current_term) {
    // TODO sync data
    role = follower;
    leader_id = arg.leader_id_;
  }

  // Data is not up-to-date
  if (arg.prev_log_term_ != prev_log_term ||
      arg.prev_log_index_ != prev_log_index) {
    reply.term_ = current_term;
    reply.success_ = false;
    return 0;
  }

  // TODO
  //  Data is up-to-dated, do append

  return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(
    int node, const append_entries_args<command> &arg,
    const append_entries_reply &reply) {
  std::unique_lock<std::mutex> l(mtx);
  check_term(arg.term_);

  if (reply.success_) {
    return;
  }

  // TODO handle reply fail
}

template <typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(
    install_snapshot_args args, install_snapshot_reply &reply) {
  // Lab3: Your code here
  return 0;
}

template <typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(
    int node, const install_snapshot_args &arg,
    const install_snapshot_reply &reply) {
  // Lab3: Your code here
  return;
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
  append_entries_reply reply;
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
        std::chrono::milliseconds(97 * my_id % 100 + 100));
    if (is_stopped()) {
      return;
    }

    std::unique_lock<std::mutex> l(mtx);

    // Follower becomes a candidate
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - last_seen_leader)
            .count() > 300) {
      RAFT_LOG("timeout start new vote")
      role = candidate;

      // Start new vote
      ++current_term;
      init_new_term();
      vote_for_me.insert(my_id);
      vote_for = my_id;

      auto args = request_vote_args{
          .term_ = current_term,
          .candidate_id_ = my_id,
          .last_log_index_ = prev_log_index,
          .last_log_term_ = prev_log_term,
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
  //  while (true) {
  //    if (is_stopped()) return;
  //    // std::unique_lock<std::mutex> l(mtx);
  //
  //    if (role != leader) {
  //      continue;
  //    }
  //    // TODO
  //  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {
  //  while (true) {
  //    if (is_stopped()) return;
  //    // Lab3: Your code here:
  //    // TODO
  //  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (is_stopped()) {
      return;
    }

    std::unique_lock<std::mutex> l(mtx);
    if (role != leader) {
      continue;
    }
    auto args = append_entries_args<command>{
        .term_ = current_term,
        .leader_id_ = my_id,
        .prev_log_index_ = prev_log_index,
        .prev_log_term_ = prev_log_term,
        .entries_ =
            log_entry<command>{
                .entries_ = {},
            },
        .leader_commit_ = leader_commit,
    };
    for (auto target = 0; target < rpc_clients.size(); ++target) {
      thread_pool->template addObjJob(this, &raft::send_append_entries, target,
                                      args);
    }
    RAFT_LOG("send heart beat");
  }
}

template <typename state_machine, typename command>
void raft<state_machine, command>::check_term(int term) {
  if (term > current_term) {
    current_term = term;
    init_new_term();
  }
}
template <typename state_machine, typename command>
void raft<state_machine, command>::init_new_term() {
  vote_for = -1;
  vote_for_me.clear();
  leader_id = -1;
  role = follower;
}

/******************************************************************

                        Other functions

*******************************************************************/

#endif  // raft_h