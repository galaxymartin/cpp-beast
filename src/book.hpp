#pragma once

#include <string>
#include <cstdint>
#include <boost/json.hpp>

struct Book {
  uint64_t id{};
  std::string title;
  std::string author;
  int year{};
};

void to_json(boost::json::value& jv, Book const& b);
void from_json(boost::json::value const& jv, Book& b);
