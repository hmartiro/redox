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
    const std::function<void(const std::string&, const ReplyT&)>& callback,
    const std::function<void(const std::string&, int status)>& error_callback,
    double repeat, double after, bool free_memory, log::Logger& logger
) : rdx_(rdx), id_(id), cmd_(cmd), repeat_(repeat), after_(after), free_memory_(free_memory),
    success_callback_(callback), error_callback_(error_callback), logger_(logger) {
  timer_guard_.lock();
}

template<class ReplyT>
void Command<ReplyT>::processReply(redisReply* r) {

  free_guard_.lock();

  reply_obj_ = r;
  handleCallback();

  pending_--;

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
const ReplyT& Command<ReplyT>::reply() {
  if (!ok()) {
    logger_.warning() << cmd_ << ": Accessing value of reply with status != OK.";
  }
  return reply_val_;
}

template<class ReplyT>
bool Command<ReplyT>::isErrorReply() {

  if (reply_obj_->type == REDIS_REPLY_ERROR) {
    logger_.error() << cmd_ << ": " << reply_obj_->str;
    return true;
  }
  return false;
}

template<class ReplyT>
bool Command<ReplyT>::isNilReply() {

  if (reply_obj_->type == REDIS_REPLY_NIL) {
    logger_.warning() << cmd_ << ": Nil reply.";
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
// Specializations of handleCallback for all data types
// ----------------------------------------------------------------------------

template<>
void Command<redisReply*>::handleCallback() {
  invokeSuccess(reply_obj_);
}

template<>
void Command<string>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);
  else if(isNilReply()) invokeError(REDOX_NIL_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_STRING && reply_obj_->type != REDIS_REPLY_STATUS) {
    logger_.error() << cmd_ << ": Received non-string reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    string s(reply_obj_->str, reply_obj_->len);
    invokeSuccess(s);
  }
}

template<>
void Command<char*>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);
  else if(isNilReply()) invokeError(REDOX_NIL_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_STRING && reply_obj_->type != REDIS_REPLY_STATUS) {
    logger_.error() << cmd_ << ": Received non-string reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    invokeSuccess(reply_obj_->str);
  }
}

template<>
void Command<int>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);
  else if(isNilReply()) invokeError(REDOX_NIL_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_INTEGER) {
    logger_.error() << cmd_ << ": Received non-integer reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    invokeSuccess((int) reply_obj_->integer);
  }
}

template<>
void Command<long long int>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);
  else if(isNilReply()) invokeError(REDOX_NIL_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_INTEGER) {
    logger_.error() << cmd_ << ": Received non-integer reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    invokeSuccess(reply_obj_->integer);
  }
}

template<>
void Command<nullptr_t>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_NIL) {
    logger_.error() << cmd_ << ": Received non-nil reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    invokeSuccess(nullptr);
  }
}


template<>
void Command<vector<string>>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_ARRAY) {
    logger_.error() << cmd_ << ": Received non-array reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    vector<string> v;
    size_t count = reply_obj_->elements;
    for(size_t i = 0; i < count; i++) {
      redisReply* r = *(reply_obj_->element + i);
      if(r->type != REDIS_REPLY_STRING) {
        logger_.error() << cmd_ << ": Received non-array reply.";
        invokeError(REDOX_WRONG_TYPE);
      }
      v.emplace_back(r->str, r->len);
    }
    invokeSuccess(v);
  }
}

template<>
void Command<unordered_set<string>>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_ARRAY) {
    logger_.error() << cmd_ << ": Received non-array reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    unordered_set<string> v;
    size_t count = reply_obj_->elements;
    for(size_t i = 0; i < count; i++) {
      redisReply* r = *(reply_obj_->element + i);
      if(r->type != REDIS_REPLY_STRING) {
        logger_.error() << cmd_ << ": Received non-array reply.";
        invokeError(REDOX_WRONG_TYPE);
      }
      v.emplace(r->str, r->len);
    }
    invokeSuccess(v);
  }
}

template<>
void Command<set<string>>::handleCallback() {

  if(isErrorReply()) invokeError(REDOX_ERROR_REPLY);

  else if(reply_obj_->type != REDIS_REPLY_ARRAY) {
    logger_.error() << cmd_ << ": Received non-array reply.";
    invokeError(REDOX_WRONG_TYPE);

  } else {
    set<string> v;
    size_t count = reply_obj_->elements;
    for(size_t i = 0; i < count; i++) {
      redisReply* r = *(reply_obj_->element + i);
      if(r->type != REDIS_REPLY_STRING) {
        logger_.error() << cmd_ << ": Received non-array reply.";
        invokeError(REDOX_WRONG_TYPE);
      }
      v.emplace(r->str, r->len);
    }
    invokeSuccess(v);
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
