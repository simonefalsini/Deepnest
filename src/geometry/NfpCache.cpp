#include "deepnest/NfpCache.h"

#include <QCryptographicHash>

#include <algorithm>
#include <cstring>

namespace deepnest {

namespace {
QString MakeKeyString(const NfpKey& key) {
  return QString::fromLatin1("%1|%2|%3|%4|%5")
      .arg(key.a)
      .arg(key.b)
      .arg(key.rotation_a)
      .arg(key.rotation_b)
      .arg(key.inside ? 1 : 0);
}
}  // namespace

bool NfpKey::operator==(const NfpKey& other) const {
  return a == other.a && b == other.b && rotation_a == other.rotation_a &&
         rotation_b == other.rotation_b && inside == other.inside;
}

std::size_t NfpKeyHasher::operator()(const NfpKey& key) const {
  auto str = MakeKeyString(key).toUtf8();
  auto hash = QCryptographicHash::hash(str, QCryptographicHash::Sha1);
  std::size_t value = 0;
  std::memcpy(&value, hash.constData(),
              std::min(sizeof(value), static_cast<size_t>(hash.size())));
  return value;
}

void NfpCache::Store(const NfpKey& key, const PolygonWithHoles& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.insert(MakeKeyString(key), value);
}

bool NfpCache::TryGet(const NfpKey& key, PolygonWithHoles* value) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto key_string = MakeKeyString(key);
  auto it = cache_.constFind(key_string);
  if (it == cache_.constEnd()) {
    return false;
  }
  if (value) {
    *value = it.value();
  }
  return true;
}

void NfpCache::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

}  // namespace deepnest
