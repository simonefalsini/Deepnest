#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/NestPolygon.h"

#include <QHash>
#include <QString>
#include <mutex>

namespace deepnest {

struct NfpKey {
  QString a;
  QString b;
  double rotation_a {0.0};
  double rotation_b {0.0};
  bool inside {false};

  bool operator==(const NfpKey& other) const;
};

struct NfpKeyHasher {
  std::size_t operator()(const NfpKey& key) const;
};

class NfpCache {
 public:
  NfpCache();

  void SyncConfig(const DeepNestConfig& config);

  void Store(const NfpKey& key, const PolygonCollection& value);
  bool TryGet(const NfpKey& key, PolygonCollection* value) const;
  void Clear();

 private:
  std::size_t config_signature_ {0};
  mutable std::mutex mutex_;
  QHash<QString, PolygonCollection> cache_;
};

}  // namespace deepnest
