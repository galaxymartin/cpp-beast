#include "book.hpp"
#include <boost/log/trivial.hpp>

void to_json(boost::json::value& jv, Book const& b) {
  jv = {
    {"id", b.id},
    {"title", b.title},
    {"author", b.author},
    {"year", b.year}
  };
}

void from_json(boost::json::value const& jv, Book& b) {
  try {
    auto const& obj = jv.as_object();
    if (obj.contains("id") && obj.at("id").is_int64()) b.id = static_cast<uint64_t>(obj.at("id").as_int64());
    if (obj.contains("title") && obj.at("title").is_string()) b.title = std::string(obj.at("title").as_string()); else b.title.clear();
    if (obj.contains("author") && obj.at("author").is_string()) b.author = std::string(obj.at("author").as_string()); else b.author.clear();
    if (obj.contains("year") && obj.at("year").is_int64()) b.year = static_cast<int>(obj.at("year").as_int64());
  } catch (std::exception const& e) {
    BOOST_LOG_TRIVIAL(error) << "from_json: error: " << e.what();
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "from_json: unknown error";
  }
}
