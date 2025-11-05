#include "deepnest/NfpCache.h"

#include <QCryptographicHash>

#include <algorithm>
#include <cstring>
#include <functional>

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

template <typename T>
void HashCombine(std::size_t* seed, const T& value) {
  std::hash<T> hasher;
  *seed ^= hasher(value) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
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

NfpCache::NfpCache() = default;

void NfpCache::SyncConfig(const DeepNestConfig& config) {
  std::size_t signature = 0;
  HashCombine(&signature, static_cast<int>(config.rotations()));
  HashCombine(&signature, static_cast<int>(config.threads()));
  HashCombine(&signature, static_cast<int>(config.mutation_rate()));
  HashCombine(&signature, config.curve_tolerance());
  HashCombine(&signature, config.endpoint_tolerance());
  HashCombine(&signature, config.clipper_scale());
  HashCombine(&signature, config.spacing());
  HashCombine(&signature, config.use_holes());
  HashCombine(&signature, config.explore_concave());

  std::lock_guard<std::mutex> lock(mutex_);
  if (signature != config_signature_) {
    cache_.clear();
    config_signature_ = signature;
  }
}

void NfpCache::Store(const NfpKey& key, const PolygonCollection& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.insert(MakeKeyString(key), value);
}

bool NfpCache::TryGet(const NfpKey& key, PolygonCollection* value) const {
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
