#pragma once

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
  void Store(const NfpKey& key, const PolygonWithHoles& value);
  bool TryGet(const NfpKey& key, PolygonWithHoles* value) const;
  void Clear();

 private:
  mutable std::mutex mutex_;
  QHash<QString, PolygonWithHoles> cache_;
};

}  // namespace deepnest
