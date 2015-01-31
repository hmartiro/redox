/*
* Simple stream-based logger for C++11.
*
* Adapted from
*   http://vilipetek.com/2014/04/17/thread-safe-simple-logger-in-c11/
*/

#include "utils/logger.hpp"
#include <iostream>
#include <iomanip>

// needed for MSVC
#ifdef WIN32
#define localtime_r(_Time, _Tm) localtime_s(_Tm, _Time)
#endif // localtime_r

namespace redox {
namespace log {

// Convert date and time info from tm to a character string
// in format "YYYY-mm-DD HH:MM:SS" and send it to a stream
std::ostream &operator<<(std::ostream &stream, const tm *tm) {
// I had to muck around this section since GCC 4.8.1 did not implement std::put_time
//	return stream << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
  return stream << 1900 + tm->tm_year << '-' <<
    std::setfill('0') << std::setw(2) << tm->tm_mon + 1 << '.'
    << std::setfill('0') << std::setw(2) << tm->tm_mday << ' '
    << std::setfill('0') << std::setw(2) << tm->tm_hour << ':'
    << std::setfill('0') << std::setw(2) << tm->tm_min << ':'
    << std::setfill('0') << std::setw(2) << tm->tm_sec;
}

// --------------------
// Logstream
// --------------------

Logstream::Logstream(Logger &logger, Level loglevel) :
  m_logger(logger), m_loglevel(loglevel) {
}

Logstream::Logstream(const Logstream &ls) :
  m_logger(ls.m_logger), m_loglevel(ls.m_loglevel) {
  // As of GCC 8.4.1 basic_stream is still lacking a copy constructor
  // (part of C++11 specification)
  //
  // GCC compiler expects the copy constructor even thought because of
  // RVO this constructor is never used
}

Logstream::~Logstream() {
  if(m_logger.level() <= m_loglevel)
    m_logger.log(m_loglevel, this->str());
}

// --------------------
// Logger
// --------------------

Logger::Logger(std::string filename, Level loglevel) :
  m_file(filename, std::fstream::out | std::fstream::app | std::fstream::ate),
  m_stream(m_file), m_loglevel(loglevel) {}

Logger::Logger(std::ostream &outfile, Level loglevel) :
  m_stream(outfile), m_loglevel(loglevel) {}

Logger::~Logger() {
  m_stream.flush();
}

const tm *Logger::getLocalTime() {
  auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  localtime_r(&in_time_t, &m_time);
  return &m_time;
}

void Logger::log(Level l, std::string oMessage) {
  const static char *LevelStr[] = {
    "[Trace]  ", "[Debug]  ", "[Info]   ", "[Warning]", "[Error]  ", "[Fatal]  "
  };

  m_lock.lock();
  m_stream << '(' << getLocalTime() << ") "
    << LevelStr[l] << "\t"
    << oMessage << std::endl;
  m_lock.unlock();
}

} // End namespace
} // End namespace
