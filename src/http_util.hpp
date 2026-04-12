#pragma once

#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <system_error>
#include <boost/json.hpp>

// Small utilities for HTTP handling and parsing used by the server.

inline constexpr std::string_view ERR_INVALID_JSON = "invalid json";
inline constexpr std::string_view ERR_INVALID_ID = "invalid id";
inline constexpr std::string_view ERR_BOOK_NOT_FOUND = "book not found";
inline constexpr std::string_view ERR_NOT_FOUND = "not found";
inline constexpr std::string_view CONTENT_TYPE_JSON = "application/json";

// Parse an ID suffix from a request target of the form '/books/{id}'.
// Returns true and sets `id_out` on success, false otherwise.
inline bool parse_book_id(std::string_view target, uint64_t &id_out) {
  constexpr std::string_view prefix = "/books/";
  if (target.rfind(prefix, 0) != 0) return false;
  auto idstr = target.substr(prefix.size());
  if (idstr.empty()) return false;
  int64_t tmp = 0;
  auto parse_res = std::from_chars(idstr.data(), idstr.data() + idstr.size(), tmp);
  if (parse_res.ec != std::errc() || parse_res.ptr == idstr.data() || tmp < 0) return false;
  id_out = static_cast<uint64_t>(tmp);
  return true;
}

// Parse a JSON body into a boost::json::value. Returns true on success.
inline bool parse_json_body(const std::string& body, boost::json::value &out) {
  try { out = boost::json::parse(body); return true; } catch (...) { return false; }
}
