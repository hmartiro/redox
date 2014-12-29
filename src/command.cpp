/**
* Redis C++11 wrapper.
*/

#include "command.hpp"

namespace redox {

template<class ReplyT>
bool Command<ReplyT>::is_error_reply() {

  if (reply_obj->type == REDIS_REPLY_ERROR) {
    std::cerr << "[ERROR] " << cmd << ": " << reply_obj->str << std::endl;
    return true;
  }
  return false;
}

template<class ReplyT>
bool Command<ReplyT>::is_nil_reply() {

  if (reply_obj->type == REDIS_REPLY_NIL) {
    std::cerr << "[WARNING] " << cmd << ": Nil reply." << std::endl;
    return true;
  }
  return false;
}

template<>
void Command<redisReply*>::invoke_callback() {
  invoke(reply_obj);
}

template<>
void Command<std::string>::invoke_callback() {

  if(is_error_reply()) invoke_error(REDOX_ERROR_REPLY);
  else if(is_nil_reply()) invoke_error(REDOX_NIL_REPLY);

  else if(reply_obj->type != REDIS_REPLY_STRING && reply_obj->type != REDIS_REPLY_STATUS) {
    std::cerr << "[ERROR] " << cmd << ": Received non-string reply." << std::endl;
    invoke_error(REDOX_WRONG_TYPE);

  } else {
    std::string s = reply_obj->str;
    invoke(s);
  }
}

template<>
void Command<char*>::invoke_callback() {

  if(is_error_reply()) invoke_error(REDOX_ERROR_REPLY);
  else if(is_nil_reply()) invoke_error(REDOX_NIL_REPLY);

  else if(reply_obj->type != REDIS_REPLY_STRING && reply_obj->type != REDIS_REPLY_STATUS) {
    std::cerr << "[ERROR] " << cmd << ": Received non-string reply." << std::endl;
    invoke_error(REDOX_WRONG_TYPE);

  } else {
    invoke(reply_obj->str);
  }
}

template<>
void Command<int>::invoke_callback() {
//  std::cout << "invoking int callback" << std::endl;
  if(is_error_reply()) invoke_error(REDOX_ERROR_REPLY);
  else if(is_nil_reply()) invoke_error(REDOX_NIL_REPLY);

  else if(reply_obj->type != REDIS_REPLY_INTEGER) {
    std::cerr << "[ERROR] " << cmd << ": Received non-integer reply." << std::endl;
    invoke_error(REDOX_WRONG_TYPE);

  } else {
    invoke((int) reply_obj->integer);
  }
}

template<>
void Command<long long int>::invoke_callback() {

  if(is_error_reply()) invoke_error(REDOX_ERROR_REPLY);
  else if(is_nil_reply()) invoke_error(REDOX_NIL_REPLY);

  else if(reply_obj->type != REDIS_REPLY_INTEGER) {
    std::cerr << "[ERROR] " << cmd << ": Received non-integer reply." << std::endl;
    invoke_error(REDOX_WRONG_TYPE);

  } else {
    invoke(reply_obj->integer);
  }
}

template<>
void Command<std::nullptr_t>::invoke_callback() {

  if(is_error_reply()) invoke_error(REDOX_ERROR_REPLY);

  else if(reply_obj->type != REDIS_REPLY_NIL) {
    std::cerr << "[ERROR] " << cmd << ": Received non-nil reply." << std::endl;
    invoke_error(REDOX_WRONG_TYPE);

  } else {
    invoke(nullptr);
  }
}

} // End namespace redox
