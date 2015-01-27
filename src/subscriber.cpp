/**
* Redis C++11 wrapper.
*/

#include <string.h>
#include "subscriber.hpp"

using namespace std;

namespace redox {

Subscriber::Subscriber(
    const std::string& host, const int port,
    std::function<void(int)> connection_callback,
    std::ostream& log_stream, log::Level log_level
) : rdx_(host, port, connection_callback, log_stream, log_level),
    logger_(rdx_.logger_) {}

Subscriber::Subscriber(
    const std::string& path,
    std::function<void(int)> connection_callback,
    std::ostream& log_stream, log::Level log_level
) : rdx_(path, connection_callback, log_stream, log_level),
    logger_(rdx_.logger_) {}


// For debugging only
void debugReply(Command<redisReply*> c) {

  redisReply* reply = c.reply();

  cout << "------" << endl;
  cout << c.cmd() << " " << (reply->type == REDIS_REPLY_ARRAY) << " " << (reply->elements) << endl;
  for(size_t i = 0; i < reply->elements; i++) {
    redisReply* r = reply->element[i];
    cout << "element " << i << ", reply type = " << r->type << " ";
    if(r->type == REDIS_REPLY_STRING) cout << r->str << endl;
    else if(r->type == REDIS_REPLY_INTEGER) cout << r->integer << endl;
    else cout << "some other type" << endl;
  }
  cout << "------" << endl;
}

void Subscriber::subscribeBase(const string cmd_name, const string topic,
    function<void(const string&, const string&)> msg_callback,
    function<void(const string&)> sub_callback,
    function<void(const string&)> unsub_callback,
    function<void(const string&, int)> err_callback
) {

  rdx_.commandLoop<redisReply*>(cmd_name + " " + topic,
      [this, topic, msg_callback, err_callback, sub_callback, unsub_callback](Command<redisReply*>& c) {

        if (!c.ok()) {
          if (err_callback) err_callback(topic, c.status());
          return;
        }

        redisReply* reply = c.reply();

        // TODO cancel this command on unsubscription?

        // If the last entry is an integer, then it is a [p]sub/[p]unsub command
        if ((reply->type == REDIS_REPLY_ARRAY) &&
            (reply->element[reply->elements - 1]->type == REDIS_REPLY_INTEGER)) {

          if (!strncmp(reply->element[0]->str, "sub", 3)) {
            subscribed_topics_.insert(topic);
            if (sub_callback) sub_callback(topic);

          } else if (!strncmp(reply->element[0]->str, "psub", 4)) {
            psubscribed_topics_.insert(topic);
            if (sub_callback) sub_callback(topic);

          } else if (!strncmp(reply->element[0]->str, "uns", 3)) {
            subscribed_topics_.erase(topic);
            if (unsub_callback) unsub_callback(topic);

          } else if (!strncmp(reply->element[0]->str, "puns", 4)) {
            psubscribed_topics_.erase(topic);
            if (unsub_callback) unsub_callback(topic);
          }

          else logger_.error() << "Unknown pubsub message: " << reply->element[0]->str;
        }

          // Message for subscribe
        else if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 3)) {
          char* msg = reply->element[2]->str;
          if (msg && msg_callback) msg_callback(topic, reply->element[2]->str);
        }

          // Message for psubscribe
        else if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 4)) {
          char* msg = reply->element[2]->str;
          if (msg && msg_callback) msg_callback(reply->element[2]->str, reply->element[3]->str);
        }

        else logger_.error() << "Unknown pubsub message of type " << reply->type;
      },
      1e10 // To keep the command around for a few hundred years
  );
}

void Subscriber::subscribe(const string topic,
    function<void(const string&, const string&)> msg_callback,
    function<void(const string&)> sub_callback,
    function<void(const string&)> unsub_callback,
    function<void(const string&, int)> err_callback
) {
  if(subscribed_topics_.find(topic) != subscribed_topics_.end()) {
    logger_.warning() << "Already subscribed to " << topic << "!";
    return;
  }
  subscribeBase("SUBSCRIBE", topic, msg_callback, sub_callback, unsub_callback, err_callback);
}

void Subscriber::psubscribe(const string topic,
    function<void(const string&, const string&)> msg_callback,
    function<void(const string&)> sub_callback,
    function<void(const string&)> unsub_callback,
    function<void(const string&, int)> err_callback
) {
  if(psubscribed_topics_.find(topic) != psubscribed_topics_.end()) {
    logger_.warning() << "Already psubscribed to " << topic << "!";
    return;
  }
  subscribeBase("PSUBSCRIBE", topic, msg_callback, sub_callback, unsub_callback, err_callback);
}

void Subscriber::unsubscribeBase(const string cmd_name, const string topic,
    function<void(const string&, int)> err_callback
) {
  rdx_.command<redisReply*>(cmd_name + " " + topic,
      [topic, err_callback](Command<redisReply*>& c) {
        if(!c.ok()) {
          if (err_callback) err_callback(topic, c.status());
        }
      }
  );
}

void Subscriber::unsubscribe(const string topic,
    function<void(const string&, int)> err_callback
) {
  if(subscribed_topics_.find(topic) == subscribed_topics_.end()) {
    logger_.warning() << "Cannot unsubscribe from " << topic << ", not subscribed!";
    return;
  }
  unsubscribeBase("UNSUBSCRIBE", topic, err_callback);
}

void Subscriber::punsubscribe(const string topic,
    function<void(const string&, int)> err_callback
) {
  if(psubscribed_topics_.find(topic) == psubscribed_topics_.end()) {
    logger_.warning() << "Cannot punsubscribe from " << topic << ", not psubscribed!";
    return;
  }
  unsubscribeBase("PUNSUBSCRIBE", topic, err_callback);
}

} // End namespace
