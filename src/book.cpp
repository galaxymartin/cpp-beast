#include "book.hpp"

void to_json(boost::json::value& jv, Book const& b) {
  jv = {
    {"id", b.id},
    {"title", b.title},
    {"author", b.author},
    {"year", b.year}
  };
}

void from_json(boost::json::value const& jv, Book& b) {
  auto const& obj = jv.as_object();
  if (obj.contains("id")) b.id = (uint64_t)obj.at("id").as_int64();
  b.title = obj.at("title").as_string().c_str();
  b.author = obj.at("author").as_string().c_str();
  if (obj.contains("year")) b.year = (int)obj.at("year").as_int64();
}
