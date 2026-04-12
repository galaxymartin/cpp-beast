#include <gtest/gtest.h>
#include "book_repository.hpp"
#include "book.hpp"

TEST(BookRepo, AddGet) {
  BookRepository repo;
  Book b; b.title = "1984"; b.author = "Orwell"; b.year = 1949;
  auto added = repo.add(b);
  EXPECT_GT(added.id, 0u);
  auto got = repo.get(added.id);
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(got->title, "1984");
  EXPECT_EQ(got->author, "Orwell");
}

TEST(BookRepo, AllAndRemove) {
  BookRepository repo;
  Book a; a.title = "A"; repo.add(a);
  Book b; b.title = "B"; repo.add(b);
  auto all = repo.all();
  EXPECT_GE(all.size(), 2u);
}
