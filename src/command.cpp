/**
* Redis C++11 wrapper.
*/

#include <vector>
#include <set>
#include <unordered_set>

#include "command.hpp"
#include "redox.hpp"

using namespace std;

namespace redox {

template<class ReplyT>
Command<ReplyT>::Command(
    Redox* rdx,
    long id,
    const std::string& cmd,
    const std::function<void(Command<ReplyT>&)>& callback,
    double repeat, double after, bool free_memory, log::Logger& logger
) : rdx_(rdx), id_(id), cmd_(cmd), repeat_(repeat), after_(after), free_memory_(free_memory),
    callback_(callback),  logger_(logger) {
  timer_guard_.lock();
}

template<class ReplyT>
Command<ReplyT>& Command<ReplyT>::block() {
  std::unique_lock<std::mutex> lk(blocker_lock_);
  blocker_.wait(lk, [this]() {
    logger_.info() << "checking blocker: " << blocking_done_;
    return blocking_done_.load(); });
  logger_.info() << "returning from block";
  blocking_done_ = {false};
  return *this;
}

template<class ReplyT>
void Command<ReplyT>::processReply(redisReply* r) {

  free_guard_.lock();

  reply_obj_ = r;
  parseReplyObject();
  invoke();
//  logger_.info() << "reply status " << reply_status_;
  pending_--;

  blocking_done_ = true;
//  logger_.info() << "notifying blocker";
  blocker_.notify_all();

  // Allow free() method to free memory
  if (!free_memory_) {
//    logger.trace() << "Command memory not being freed, free_memory = " << free_memory;
    free_guard_.unlock();
    return;
  }

  freeReply();

  // Handle memory if all pending replies have arrived
  if (pending_ == 0) {

    // Just free non-repeating commands
    if (repeat_ == 0) {
      freeCommand(this);
      return;

      // Free repeating commands if timer is stopped
    } else {
      if ((long)(timer_.data) == 0) {
        freeCommand(this);
        return;
      }
    }
  }

  free_guard_.unlock();
}

template<class ReplyT>
void Command<ReplyT>::free() {

  free_guard_.lock();
  freeReply();
  free_guard_.unlock();

  freeCommand(this);
}

template<class ReplyT>
void Command<ReplyT>::freeReply() {

  if (reply_obj_ == nullptr) {
    logger_.error() << cmd_ << ": Attempting to double free reply object.";
    return;
  }

  freeReplyObject(reply_obj_);
  reply_obj_ = nullptr;
}

template<class ReplyT>
void Command<ReplyT>::freeCommand(Command<ReplyT>* c) {
  c->rdx_->template remove_active_command<ReplyT>(c->id_);
//  logger.debug() << "Deleted Command " << c->id << " at " << c;
  delete c;
}


template<class ReplyT>
const ReplyT& Command<ReplyT>::reply() const {
  if (!ok()) {
    logger_.warning() << cmd_ << ": Accessing value of reply with status != OK.";
  }
  return reply_val_;
}

template<class ReplyT>
bool Command<ReplyT>::isExpectedReply(int type) {

  if(reply_obj_->type == type) {
    reply_status_ = OK_REPLY;
    return true;
  }

  if(checkErrorReply() || checkNilReply()) return false;

  logger_.error() << cmd_ << ": Received reply of type " << reply_obj_->type
      << ", expected type " << type << ".";
  reply_status_ = WRONG_TYPE;
  return false;
}

template<class ReplyT>
bool Command<ReplyT>::isExpectedReply(int typeA, int typeB) {

  if((reply_obj_->type == typeA) || (reply_obj_->type == typeB)) {
    reply_status_ = OK_REPLY;
    return true;
  }

  if(checkErrorReply() || checkNilReply()) return false;

  logger_.error() << cmd_ << ": Received reply of type " << reply_obj_->type
      << ", expected type " << typeA << " or " << typeB << ".";
  reply_status_ = WRONG_TYPE;
  return false;
}

template<class ReplyT>
bool Command<ReplyT>::checkErrorReply() {

  if (reply_obj_->type == REDIS_REPLY_ERROR) {
    logger_.error() << cmd_ << ": " << reply_obj_->str;
    reply_status_ = ERROR_REPLY;
    return true;
  }
  return false;
}

template<class ReplyT>
bool Command<ReplyT>::checkNilReply() {

  if (reply_obj_->type == REDIS_REPLY_NIL) {
    logger_.warning() << cmd_ << ": Nil reply.";
    reply_status_ = NIL_REPLY;
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
// Specializations of parseReplyObject for all expected return types
// ----------------------------------------------------------------------------

template<>
void Command<redisReply*>::parseReplyObject() {
  if(!checkErrorReply()) reply_status_ = OK_REPLY;
  reply_val_ = reply_obj_;
}

template<>
void Command<string>::parseReplyObject() {
  if(!isExpectedReply(REDIS_REPLY_STRING, REDIS_REPLY_STATUS)) return;
  reply_val_ = {reply_obj_->str, static_cast<size_t>(reply_obj_->len)};
}

template<>
void Command<char*>::parseReplyObject() {
  if(!isExpectedReply(REDIS_REPLY_STRING, REDIS_REPLY_STATUS)) return;
  reply_val_ = reply_obj_->str;
}

template<>
void Command<int>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_INTEGER)) return;
  reply_val_ = (int) reply_obj_->integer;
}

template<>
void Command<long long int>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_INTEGER)) return;
  reply_val_ = reply_obj_->integer;
}

template<>
void Command<nullptr_t>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_NIL)) return;
  reply_val_ = nullptr;
}

template<>
void Command<vector<string>>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_ARRAY)) return;

  for(size_t i = 0; i < reply_obj_->elements; i++) {
    redisReply* r = *(reply_obj_->element + i);
    reply_val_.emplace_back(r->str, r->len);
  }
}

template<>
void Command<unordered_set<string>>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_ARRAY)) return;

  for(size_t i = 0; i < reply_obj_->elements; i++) {
    redisReply* r = *(reply_obj_->element + i);
    reply_val_.emplace(r->str, r->len);
  }
}

template<>
void Command<set<string>>::parseReplyObject() {

  if(!isExpectedReply(REDIS_REPLY_ARRAY)) return;

  for(size_t i = 0; i < reply_obj_->elements; i++) {
    redisReply* r = *(reply_obj_->element + i);
    reply_val_.emplace(r->str, r->len);
  }
}

// Explicit template instantiation for available types, so that the generated
// library contains them and we can keep the method definitions out of the
// header file.
template class Command<redisReply*>;
template class Command<string>;
template class Command<char*>;
template class Command<int>;
template class Command<long long int>;
template class Command<nullptr_t>;
template class Command<vector<string>>;
template class Command<set<string>>;
template class Command<unordered_set<string>>;

} // End namespace redox
