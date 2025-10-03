#pragma once
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

// number, string, map, set
using Value =
    std::variant<std::string, std::unordered_map<std::string, std::string>,
                 std::unordered_set<std::string>>;

namespace yanhon {
class db {
public:
  db() {}
  bool set(const std::string &key, const Value &value);
  std::optional<Value> get(const std::string &key);
  bool del(const std::string &key);
  bool exists(const std::string &key);

private:
  std::unordered_map<std::string, Value> database;
  std::shared_mutex mutex;
};
} // namespace yanhon
