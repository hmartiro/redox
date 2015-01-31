/*
* Simple stream-based logger for C++11.
*
* Adapted from
*   http://vilipetek.com/2014/04/17/thread-safe-simple-logger-in-c11/
*/

#pragma once

#include <string>
#include <sstream>
#include <mutex>
#include <memory>
#include <fstream>

namespace redox {
namespace log {

// Log message levels
enum Level {
  Trace, Debug, Info, Warning, Error, Fatal, Off
};

// Forward declaration
class Logger;

/**
* A class representing one log line.
*/
class Logstream : public std::ostringstream {
public:
  Logstream(Logger &logger, Level l);
  Logstream(const Logstream &ls);
  ~Logstream();
private:
  Logger &m_logger;
  Level m_loglevel;
};

/**
* A simple stream-based logger.
*/
class Logger {
public:

  Logger(std::string filename, Level loglevel = Level::Info);
  Logger(std::ostream &outfile, Level loglevel = Level::Info);

  virtual ~Logger();

  void level(Level l) { m_loglevel = l; }
  Level level() { return m_loglevel; }

  void log(Level l, std::string oMessage);

  Logstream operator()(Level l = Level::Info) { return Logstream(*this, l); }

  // Helpers
  Logstream trace() { return (*this)(Level::Trace); }
  Logstream debug() { return (*this)(Level::Debug); }
  Logstream info() { return (*this)(Level::Info); }
  Logstream warning() { return (*this)(Level::Warning); }
  Logstream error() { return (*this)(Level::Error); }
  Logstream fatal() { return (*this)(Level::Fatal); }

private:
  const tm *getLocalTime();

private:
  std::mutex m_lock;

  std::ofstream m_file;
  std::ostream &m_stream;

  tm m_time;

  Level m_loglevel;
};

} // End namespace
} // End namespace
