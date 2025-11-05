#include "deepnest/DeepNestSolver.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QSizeF>
#include <QSvgRenderer>
#include <QTransform>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

using namespace deepnest;

namespace {

bool ParseBool(const QString& value, bool fallback) {
  if (value.isEmpty()) {
    return true;
  }
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("true") || normalized == QStringLiteral("1") ||
      normalized == QStringLiteral("yes") || normalized == QStringLiteral("on")) {
    return true;
  }
  if (normalized == QStringLiteral("false") || normalized == QStringLiteral("0") ||
      normalized == QStringLiteral("no") || normalized == QStringLiteral("off")) {
    return false;
  }
  return fallback;
}

QPainterPath LoadSvgPath(const QString& path, double raster_scale) {
  QSvgRenderer renderer(path);
  if (!renderer.isValid()) {
    return QPainterPath();
  }

  QRectF view_box = renderer.viewBoxF();
  if (view_box.isNull()) {
    const QSize default_size = renderer.defaultSize();
    if (default_size.isEmpty()) {
      return QPainterPath();
    }
    view_box = QRectF(QPointF(0.0, 0.0), default_size);
  }

  raster_scale = std::max(1.0, raster_scale);
  const int width = std::max(1, static_cast<int>(std::ceil(view_box.width() * raster_scale)));
  const int height = std::max(1, static_cast<int>(std::ceil(view_box.height() * raster_scale)));

  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QTransform transform;
    transform.translate(-view_box.left(), -view_box.top());
    transform.scale(raster_scale, raster_scale);
    painter.setTransform(transform);
    renderer.render(&painter);
  }

  QPainterPath raster_path;
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* scan_line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    int x = 0;
    while (x < image.width()) {
      while (x < image.width() && qAlpha(scan_line[x]) == 0) {
        ++x;
      }
      if (x >= image.width()) {
        break;
      }
      const int start = x;
      while (x < image.width() && qAlpha(scan_line[x]) > 0) {
        ++x;
      }
      raster_path.addRect(QRectF(QPointF(start, y), QSizeF(x - start, 1)));
    }
  }

  QTransform inverse;
  inverse.scale(1.0 / raster_scale, 1.0 / raster_scale);
  inverse.translate(view_box.left(), view_box.top());
  raster_path = inverse.map(raster_path);
  return raster_path.simplified();
}

QPainterPath RandomPolygonInRect(const QRectF& rect) {
  QRandomGenerator* generator = QRandomGenerator::global();
  const QPointF center = rect.center();
  const int vertex_count = 5 + generator->bounded(4);

  QVector<QPointF> points;
  points.reserve(vertex_count);
  for (int i = 0; i < vertex_count; ++i) {
    const double x = generator->bounded(rect.left(), rect.right());
    const double y = generator->bounded(rect.top(), rect.bottom());
    points.append(QPointF(x, y));
  }

  std::sort(points.begin(), points.end(), [center](const QPointF& lhs, const QPointF& rhs) {
    const double lhs_angle = std::atan2(lhs.y() - center.y(), lhs.x() - center.x());
    const double rhs_angle = std::atan2(rhs.y() - center.y(), rhs.x() - center.x());
    return lhs_angle < rhs_angle;
  });

  QPainterPath path;
  if (!points.isEmpty()) {
    path.moveTo(points.front());
    for (int i = 1; i < points.size(); ++i) {
      path.lineTo(points[i]);
    }
    path.closeSubpath();
  }
  return path.simplified();
}

std::vector<QRectF> SubdivideSheet(const QRectF& sheet_rect, int target_count) {
  std::vector<QRectF> rects;
  rects.push_back(sheet_rect);
  if (target_count <= 1) {
    return rects;
  }

  QRandomGenerator* generator = QRandomGenerator::global();
  int safety = std::max(8, target_count * 4);
  while (static_cast<int>(rects.size()) < target_count && safety-- > 0) {
    auto largest = std::max_element(rects.begin(), rects.end(), [](const QRectF& lhs, const QRectF& rhs) {
      return lhs.width() * lhs.height() < rhs.width() * rhs.height();
    });
    if (largest == rects.end()) {
      break;
    }
    QRectF current = *largest;
    if (current.width() < 4.0 || current.height() < 4.0) {
      break;
    }

    const bool split_vertical = generator->bounded(2) == 0;
    const double ratio = generator->bounded(0.35, 0.65);

    if (split_vertical && current.width() > 6.0) {
      const double split_x = current.left() + current.width() * ratio;
      QRectF first(current.left(), current.top(), split_x - current.left(), current.height());
      QRectF second(split_x, current.top(), current.right() - split_x, current.height());
      if (first.width() < 2.0 || second.width() < 2.0) {
        continue;
      }
      *largest = first;
      rects.push_back(second);
    } else if (!split_vertical && current.height() > 6.0) {
      const double split_y = current.top() + current.height() * ratio;
      QRectF first(current.left(), current.top(), current.width(), split_y - current.top());
      QRectF second(current.left(), split_y, current.width(), current.bottom() - split_y);
      if (first.height() < 2.0 || second.height() < 2.0) {
        continue;
      }
      *largest = first;
      rects.push_back(second);
    } else {
      break;
    }
  }

  if (rects.size() > static_cast<std::size_t>(target_count)) {
    rects.resize(static_cast<std::size_t>(target_count));
  }
  return rects;
}

std::vector<QPainterPath> GenerateRandomSlices(const QRectF& sheet_rect, int slice_count) {
  if (slice_count <= 0) {
    return {};
  }
  std::vector<QPainterPath> slices;
  const auto subdivisions = SubdivideSheet(sheet_rect, slice_count);
  slices.reserve(subdivisions.size());
  for (const auto& rect : subdivisions) {
    slices.push_back(RandomPolygonInRect(rect));
  }
  return slices;
}

QJsonObject SerializeResult(const QList<QHash<QString, QPair<QPointF, double>>>& sheets,
                            const QStringList& unplaced,
                            const QRectF& sheet_rect,
                            const DeepNestConfig& config) {
  QJsonObject root;

  QJsonArray sheet_array;
  int index = 0;
  for (const auto& sheet : sheets) {
    QJsonObject sheet_obj;
    sheet_obj.insert(QStringLiteral("index"), index++);
    QJsonArray placements;
    for (auto it = sheet.constBegin(); it != sheet.constEnd(); ++it) {
      QJsonObject placement;
      placement.insert(QStringLiteral("id"), it.key());
      placement.insert(QStringLiteral("x"), it.value().first.x());
      placement.insert(QStringLiteral("y"), it.value().first.y());
      placement.insert(QStringLiteral("rotation"), it.value().second);
      placements.push_back(placement);
    }
    sheet_obj.insert(QStringLiteral("placements"), placements);
    sheet_array.push_back(sheet_obj);
  }
  root.insert(QStringLiteral("sheets"), sheet_array);

  QJsonArray unplaced_array;
  for (const auto& id : unplaced) {
    unplaced_array.push_back(id);
  }
  root.insert(QStringLiteral("unplaced"), unplaced_array);

  QJsonObject sheet_info;
  sheet_info.insert(QStringLiteral("x"), sheet_rect.x());
  sheet_info.insert(QStringLiteral("y"), sheet_rect.y());
  sheet_info.insert(QStringLiteral("width"), sheet_rect.width());
  sheet_info.insert(QStringLiteral("height"), sheet_rect.height());
  root.insert(QStringLiteral("sheet"), sheet_info);

  root.insert(QStringLiteral("config"), QJsonObject::fromVariantMap(config.ToVariantMap()));
  return root;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("DeepNestTest"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.2"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("Simulatore DeepNest C++"));
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption width_option({QStringLiteral("w"), QStringLiteral("width")},
                                  QStringLiteral("Larghezza del foglio"), QStringLiteral("width"),
                                  QStringLiteral("1000"));
  QCommandLineOption height_option({QStringLiteral("h"), QStringLiteral("height")},
                                   QStringLiteral("Altezza del foglio"), QStringLiteral("height"),
                                   QStringLiteral("1000"));
  QCommandLineOption slices_option({QStringLiteral("s"), QStringLiteral("slices")},
                                   QStringLiteral("Numero di suddivisioni casuali del foglio"),
                                   QStringLiteral("slices"), QStringLiteral("5"));
  QCommandLineOption population_option(QStringLiteral("population"),
                                       QStringLiteral("Dimensione popolazione"),
                                       QStringLiteral("population"));
  QCommandLineOption generations_option(QStringLiteral("generations"),
                                        QStringLiteral("Numero generazioni"),
                                        QStringLiteral("generations"));
  QCommandLineOption threads_option(QStringLiteral("threads"),
                                    QStringLiteral("Numero di thread"),
                                    QStringLiteral("threads"));
  QCommandLineOption spacing_option(QStringLiteral("spacing"),
                                    QStringLiteral("Spaziatura tra parti"),
                                    QStringLiteral("spacing"));
  QCommandLineOption rotations_option(QStringLiteral("rotations"),
                                      QStringLiteral("Numero di rotazioni discrete"),
                                      QStringLiteral("rotations"));
  QCommandLineOption rotation_step_option(QStringLiteral("rotation-step"),
                                          QStringLiteral("Passo di rotazione in gradi"),
                                          QStringLiteral("degrees"));
  QCommandLineOption mutation_option(QStringLiteral("mutation-rate"),
                                     QStringLiteral("Percentuale di mutazione"),
                                     QStringLiteral("rate"));
  QCommandLineOption curve_option(QStringLiteral("curve-tolerance"),
                                  QStringLiteral("Tolleranza semplificazione curve"),
                                  QStringLiteral("value"));
  QCommandLineOption endpoint_option(QStringLiteral("endpoint-tolerance"),
                                     QStringLiteral("Tolleranza punti terminali"),
                                     QStringLiteral("value"));
  QCommandLineOption clipper_option(QStringLiteral("clipper-scale"),
                                    QStringLiteral("Fattore di scala per Clipper"),
                                    QStringLiteral("value"));
  QCommandLineOption placement_type_option(QStringLiteral("placement-type"),
                                           QStringLiteral("Tipo di posizionamento (gravity, box, convex)"),
                                           QStringLiteral("type"));
  QCommandLineOption merge_lines_option(QStringLiteral("merge-lines"),
                                        QStringLiteral("Abilita/Disabilita la fusione delle linee (true/false)"),
                                        QStringLiteral("bool"));
  QCommandLineOption simplify_option(QStringLiteral("simplify"),
                                     QStringLiteral("Abilita la semplificazione delle geometrie (true/false)"),
                                     QStringLiteral("bool"));
  QCommandLineOption holes_option(QStringLiteral("use-holes"),
                                  QStringLiteral("Considera i fori delle parti (true/false)"),
                                  QStringLiteral("bool"));
  QCommandLineOption concave_option(QStringLiteral("explore-concave"),
                                    QStringLiteral("Esplora concavità durante il posizionamento (true/false)"),
                                    QStringLiteral("bool"));
  QCommandLineOption timeratio_option(QStringLiteral("time-ratio"),
                                      QStringLiteral("Rapporto di peso per merge linee"),
                                      QStringLiteral("ratio"));
  QCommandLineOption scale_option(QStringLiteral("scale"),
                                  QStringLiteral("Scala interna (dpi)"),
                                  QStringLiteral("scale"));
  QCommandLineOption raster_option(QStringLiteral("raster-scale"),
                                   QStringLiteral("Fattore di rasterizzazione per gli SVG"),
                                   QStringLiteral("scale"), QStringLiteral("4.0"));
  QCommandLineOption output_option({QStringLiteral("o"), QStringLiteral("output")},
                                   QStringLiteral("File JSON di output"),
                                   QStringLiteral("output"));
  QCommandLineOption progress_option(QStringLiteral("progress"),
                                     QStringLiteral("Mostra l'avanzamento dell'algoritmo genetico"));

  parser.addOption(width_option);
  parser.addOption(height_option);
  parser.addOption(slices_option);
  parser.addOption(population_option);
  parser.addOption(generations_option);
  parser.addOption(threads_option);
  parser.addOption(spacing_option);
  parser.addOption(rotations_option);
  parser.addOption(rotation_step_option);
  parser.addOption(mutation_option);
  parser.addOption(curve_option);
  parser.addOption(endpoint_option);
  parser.addOption(clipper_option);
  parser.addOption(placement_type_option);
  parser.addOption(merge_lines_option);
  parser.addOption(simplify_option);
  parser.addOption(holes_option);
  parser.addOption(concave_option);
  parser.addOption(timeratio_option);
  parser.addOption(scale_option);
  parser.addOption(raster_option);
  parser.addOption(output_option);
  parser.addOption(progress_option);
  parser.addPositionalArgument(QStringLiteral("svg"),
                               QStringLiteral("File SVG da importare"),
                               QStringLiteral("[svg...]"));

  parser.process(app);

  const double width = parser.value(width_option).toDouble();
  const double height = parser.value(height_option).toDouble();
  const int slices = parser.value(slices_option).toInt();
  const double raster_scale = std::max(1.0, parser.value(raster_option).toDouble());

  QRectF sheet_rect(0.0, 0.0, width, height);
  QPainterPath sheet_path;
  sheet_path.addRect(sheet_rect);

  DeepNestConfig config;
  if (parser.isSet(population_option)) {
    config.set_population_size(parser.value(population_option).toInt());
  }
  if (parser.isSet(generations_option)) {
    config.set_generations(parser.value(generations_option).toInt());
  }
  if (parser.isSet(threads_option)) {
    config.set_threads(parser.value(threads_option).toInt());
  }
  if (parser.isSet(spacing_option)) {
    config.set_spacing(parser.value(spacing_option).toDouble());
  }
  if (parser.isSet(rotations_option)) {
    config.set_rotations(parser.value(rotations_option).toInt());
  }
  if (parser.isSet(rotation_step_option)) {
    config.set_rotation_step(parser.value(rotation_step_option).toDouble());
  }
  if (parser.isSet(mutation_option)) {
    config.set_mutation_rate(parser.value(mutation_option).toInt());
  }
  if (parser.isSet(curve_option)) {
    config.set_curve_tolerance(parser.value(curve_option).toDouble());
  }
  if (parser.isSet(endpoint_option)) {
    config.set_endpoint_tolerance(parser.value(endpoint_option).toDouble());
  }
  if (parser.isSet(clipper_option)) {
    config.set_clipper_scale(parser.value(clipper_option).toDouble());
  }
  if (parser.isSet(placement_type_option)) {
    config.set_placement_type(parser.value(placement_type_option));
  }
  if (parser.isSet(merge_lines_option)) {
    config.set_merge_lines(ParseBool(parser.value(merge_lines_option), config.merge_lines()));
  }
  if (parser.isSet(simplify_option)) {
    config.set_simplify(ParseBool(parser.value(simplify_option), config.simplify()));
  }
  if (parser.isSet(holes_option)) {
    config.set_use_holes(ParseBool(parser.value(holes_option), config.use_holes()));
  }
  if (parser.isSet(concave_option)) {
    config.set_explore_concave(ParseBool(parser.value(concave_option), config.explore_concave()));
  }
  if (parser.isSet(timeratio_option)) {
    config.set_time_ratio(parser.value(timeratio_option).toDouble());
  }
  if (parser.isSet(scale_option)) {
    config.set_scale(parser.value(scale_option).toDouble());
  }

  QHash<QString, QPainterPath> parts;
  const auto random_parts = GenerateRandomSlices(sheet_rect, slices);
  for (int i = 0; i < static_cast<int>(random_parts.size()); ++i) {
    parts.insert(QStringLiteral("random_%1").arg(i), random_parts[static_cast<std::size_t>(i)]);
  }

  const auto svg_files = parser.positionalArguments();
  for (const auto& svg : svg_files) {
    const QPainterPath path = LoadSvgPath(svg, raster_scale);
    if (!path.isEmpty()) {
      parts.insert(svg, path);
    } else {
      std::cerr << "Impossibile caricare la geometria da " << svg.toStdString() << std::endl;
    }
  }

  DeepNestSolver solver;
  solver.SetConfig(config);
  if (parser.isSet(progress_option)) {
    solver.SetProgressCallback([](int generation, const PlacementResult& result) {
      std::cout << "Generazione " << generation
                << ": fitness = " << result.cost.fitness
                << ", parti posizionate = " << result.placements.size() << std::endl;
    });
  }

  const auto result = solver.Solve(sheet_path, parts);
  const QJsonObject json_object = SerializeResult(result, solver.unplaced_ids(), sheet_rect, config);
  const QJsonDocument doc(json_object);

  if (parser.isSet(output_option)) {
    QFile file(parser.value(output_option));
    if (file.open(QIODevice::WriteOnly)) {
      file.write(doc.toJson());
      file.close();
      std::cout << "Risultato scritto su " << parser.value(output_option).toStdString()
                << std::endl;
    } else {
      std::cerr << "Impossibile scrivere il file di output" << std::endl;
      return 1;
    }
  } else {
    std::cout << doc.toJson().toStdString() << std::endl;
  }

  if (!solver.unplaced_ids().isEmpty()) {
    std::cerr << "Parti non posizionate: " << solver.unplaced_ids().join(", ").toStdString()
              << std::endl;
  }

  return 0;
}
