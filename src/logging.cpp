#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include "logging.hpp"

void init_logging() {
  namespace logging = boost::log;
  logging::add_console_log(std::clog, boost::log::keywords::format = "%TimeStamp% [%Severity%] %Message%");
  logging::add_common_attributes();
}
