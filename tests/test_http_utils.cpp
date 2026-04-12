#include <gtest/gtest.h>
#include "http_util.hpp"

TEST(HttpUtil, ParseBookIdValid) {
  uint64_t id = 0;
  EXPECT_TRUE(parse_book_id("/books/123", id));
  EXPECT_EQ(id, 123u);
}

TEST(HttpUtil, ParseBookIdInvalid) {
  uint64_t id = 0;
  EXPECT_FALSE(parse_book_id("/books/", id));
  EXPECT_FALSE(parse_book_id("/books/abc", id));
  EXPECT_FALSE(parse_book_id("/other/123", id));
}

TEST(HttpUtil, ParseJsonBodyValid) {
  boost::json::value v;
  EXPECT_TRUE(parse_json_body("{\"a\":1}", v));
  EXPECT_TRUE(v.is_object());
}

TEST(HttpUtil, ParseJsonBodyInvalid) {
  boost::json::value v;
  EXPECT_FALSE(parse_json_body("{not json}", v));
}
