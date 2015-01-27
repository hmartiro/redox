/**
* Redis C++11 wrapper.
*/

#pragma once

#include "redox.hpp"

namespace redox {

class Subscriber {

public:

  /**
  * Initializes everything, connects over TCP to a Redis server.
  */
  Subscriber(
      const std::string& host = REDIS_DEFAULT_HOST,
      const int port = REDIS_DEFAULT_PORT,
      std::function<void(int)> connection_callback = nullptr,
      std::ostream& log_stream = std::cout,
      log::Level log_level = log::Info
  );

  /**
  * Initializes everything, connects over unix sockets to a Redis server.
  */
  Subscriber(
      const std::string& path,
      std::function<void(int)> connection_callback,
      std::ostream& log_stream = std::cout,
      log::Level log_level = log::Info
  );

  /**
  * Same as .connect() on a Redox instance.
  */
  bool connect() { return rdx_.connect(); }

  /**
  * Same as .disconnect() on a Redox instance.
  */
  void disconnect() { return rdx_.disconnect(); }

  /**
  * Same as .wait() on a Redox instance.
  */
  void wait() { return rdx_.wait(); }

  /**
  * Subscribe to a topic.
  *
  * msg_callback: invoked whenever a message is received.
  * sub_callback: invoked when successfully subscribed
  * err_callback: invoked on some error state
  */
  void subscribe(const std::string topic,
      std::function<void(const std::string&, const std::string&)> msg_callback,
      std::function<void(const std::string&)> sub_callback = nullptr,
      std::function<void(const std::string&)> unsub_callback = nullptr,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  /**
  * Subscribe to a topic with a pattern.
  *
  * msg_callback: invoked whenever a message is received.
  * sub_callback: invoked when successfully subscribed
  * err_callback: invoked on some error state
  */
  void psubscribe(const std::string topic,
      std::function<void(const std::string&, const std::string&)> msg_callback,
      std::function<void(const std::string&)> sub_callback = nullptr,
      std::function<void(const std::string&)> unsub_callback = nullptr,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  /**
  * Unsubscribe from a topic.
  *
  * err_callback: invoked on some error state
  */
  void unsubscribe(const std::string topic,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  /**
  * Unsubscribe from a topic with a pattern.
  *
  * err_callback: invoked on some error state
  */
  void punsubscribe(const std::string topic,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  /**
  * Return the topics that were subscribed() to.
  */
  const std::set<std::string>& subscribedTopics() { return subscribed_topics_; }

  /**
  * Return the topic patterns that were psubscribed() to.
  */
  const std::set<std::string>& psubscribedTopics() { return psubscribed_topics_; }

private:

  // Base for subscribe and psubscribe
  void subscribeBase(const std::string cmd_name, const std::string topic,
      std::function<void(const std::string&, const std::string&)> msg_callback,
      std::function<void(const std::string&)> sub_callback = nullptr,
      std::function<void(const std::string&)> unsub_callback = nullptr,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  // Base for unsubscribe and punsubscribe
  void unsubscribeBase(const std::string cmd_name, const std::string topic,
      std::function<void(const std::string&, int)> err_callback = nullptr
  );

  // Underlying Redis client
  Redox rdx_;

  // Keep track of topics because we can only unsubscribe
  // from subscribed topics and punsubscribe from
  // psubscribed topics, or hiredis leads to segfaults
  std::set<std::string> subscribed_topics_;
  std::set<std::string> psubscribed_topics_;

  // Reference to rdx_.logger_ for convenience
  log::Logger& logger_;
};

} // End namespace
