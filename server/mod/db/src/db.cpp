#include "db/db.hpp"

namespace yanhon {
bool db::set(const std::string &key, const Value &value) {
  std::unique_lock lock(mutex);
  database[key] = value;
  return true;
}

std::optional<Value> db::get(const std::string &key) {
  std::shared_lock lock(mutex);
  auto it = database.find(key);
  if (it != database.end()) {
    return it->second;
  } else {
    return std::nullopt;
  }
}

bool db::del(const std::string &key) {
  std::unique_lock lock(mutex);
  return database.erase(key) > 0;
}

bool db::exists(const std::string &key) {
  std::shared_lock lock(mutex);
  return database.find(key) != database.end();
}

} // namespace yanhon
