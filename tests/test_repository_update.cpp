#include <gtest/gtest.h>
#include "book_repository.hpp"

TEST(BookRepo, UpdateAndDelete) {
  BookRepository repo;
  Book b; b.title = "Old"; b.author = "A"; b.year = 1900;
  auto added = repo.add(b);
  Book upd; upd.title = "New"; // only change title
  auto updated = repo.update(added.id, upd);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(updated->title, "New");

  bool removed = repo.remove(added.id);
  EXPECT_TRUE(removed);
  auto got = repo.get(added.id);
  EXPECT_FALSE(got.has_value());
}

TEST(BookRepo, PutReplacesAllFields) {
  BookRepository repo;
  Book b; b.title = "Old"; b.author = "A"; b.year = 1900;
  auto added = repo.add(b);
  Book replacement; replacement.title = "Brand"; replacement.author = "B"; replacement.year = 2000;
  auto updated = repo.update(added.id, replacement);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(updated->title, "Brand");
  EXPECT_EQ(updated->author, "B");
  EXPECT_EQ(updated->year, 2000);
}
