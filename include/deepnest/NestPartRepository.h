#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/NestPolygon.h"
#include "deepnest/QtGeometryConverters.h"

#include <QHash>
#include <QPainterPath>
#include <QString>
#include <vector>

namespace deepnest {

class NestPartRepository {
 public:
  explicit NestPartRepository(const DeepNestConfig& config);

  void LoadParts(const QHash<QString, QPainterPath>& parts);
  const std::vector<NestPolygon>& items() const { return parts_; }

 private:
  const DeepNestConfig& config_;
  std::vector<NestPolygon> parts_;
};

}  // namespace deepnest
