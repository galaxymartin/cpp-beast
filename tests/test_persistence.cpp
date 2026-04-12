#include <gtest/gtest.h>
#include "book_repository.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>

TEST(Persistence, SaveAndLoad) {
  auto tmp = std::filesystem::temp_directory_path() / ("books_test_" + std::to_string(getpid()) + ".json");
  std::string path = tmp.string();
  std::error_code ec; std::filesystem::remove(path, ec);

  {
    BookRepository repo(path);
    Book b1; b1.title = "T1"; b1.author = "A1"; b1.year = 2001;
    Book b2; b2.title = "T2"; b2.author = "A2"; b2.year = 2002;
    auto a1 = repo.add(b1);
    auto a2 = repo.add(b2);
    std::ifstream ifs(path);
    ASSERT_TRUE(ifs.good());
    std::string contents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.size(), 0u);
  }

  {
    BookRepository repo2(path);
    auto all = repo2.all();
    EXPECT_EQ(all.size(), 2u);
    std::vector<std::string> titles;
    for (auto &b : all) titles.push_back(b.title);
    EXPECT_TRUE(std::find(titles.begin(), titles.end(), "T1") != titles.end());
    EXPECT_TRUE(std::find(titles.begin(), titles.end(), "T2") != titles.end());
  }

  std::filesystem::remove(path, ec);
}

TEST(Persistence, RemovePersists) {
  auto tmp = std::filesystem::temp_directory_path() / ("books_test_" + std::to_string(getpid()) + "_rm.json");
  std::string path = tmp.string();
  std::error_code ec; std::filesystem::remove(path, ec);

  {
    BookRepository repo(path);
    Book b; b.title = "Only"; b.author = "X"; b.year = 1999;
    auto a = repo.add(b);
    bool removed = repo.remove(a.id);
    EXPECT_TRUE(removed);
  }

  {
    BookRepository repo2(path);
    auto all = repo2.all();
    EXPECT_EQ(all.size(), 0u);
  }

  std::filesystem::remove(path, ec);
}
