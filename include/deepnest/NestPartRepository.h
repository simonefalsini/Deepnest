#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/NestPolygon.h"
#include "deepnest/QtGeometryConverters.h"

#include <QHash>
#include <QPainterPath>
#include <QString>
#include <optional>
#include <vector>

namespace deepnest {

class NestPartRepository {
 public:
  explicit NestPartRepository(const DeepNestConfig& config);

  void LoadSheet(const QPainterPath& sheet);
  void LoadParts(const QHash<QString, QPainterPath>& parts);

  const NestPolygon& sheet() const { return sheet_; }
  bool has_sheet() const { return sheet_loaded_; }
  const std::vector<NestPolygon>& items() const { return parts_; }

 private:
  NestPolygon PreparePolygon(const QString& id, const QPainterPath& path,
                             bool is_sheet);
  PolygonWithHoles SimplifyPolygon(const PolygonWithHoles& polygon,
                                   bool inside) const;
  PolygonWithHoles ApplySpacing(const PolygonWithHoles& polygon,
                                Coordinate delta) const;
  void PopulateChildren(NestPolygon* polygon) const;

  const DeepNestConfig& config_;
  NestPolygon sheet_;
  bool sheet_loaded_ {false};
  std::vector<NestPolygon> parts_;
};

}  // namespace deepnest
