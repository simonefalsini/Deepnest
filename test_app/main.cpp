#include "deepnest/DeepNestSolver.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSizeF>
#include <QSvgRenderer>

#include <iostream>

using namespace deepnest;

namespace {
QPainterPath LoadSvgPath(const QString& path) {
  QSvgRenderer renderer(path);
  if (!renderer.isValid()) {
    return QPainterPath();
  }
  QRectF bounds(QPointF(0, 0), renderer.defaultSize());
  QPainterPath painter_path;
  painter_path.addRect(bounds);
  return painter_path;
}

std::vector<QPainterPath> GenerateRandomSlices(const QRectF& sheet_rect,
                                               int slice_count) {
  std::vector<QPainterPath> slices;
  QRandomGenerator* generator = QRandomGenerator::global();
  for (int i = 0; i < slice_count; ++i) {
    double width = sheet_rect.width() * generator->bounded(0.1, 0.5);
    double height = sheet_rect.height() * generator->bounded(0.1, 0.5);
    double x = generator->bounded(sheet_rect.left(), sheet_rect.right() - width);
    double y = generator->bounded(sheet_rect.top(), sheet_rect.bottom() - height);
    QPainterPath path;
    path.addRect(QRectF(QPointF(x, y), QSizeF(width, height)));
    slices.push_back(path);
  }
  return slices;
}

QJsonArray SerializeResult(
    const QList<QHash<QString, QPair<QPointF, double>>>& result) {
  QJsonArray sheets;
  for (const auto& sheet : result) {
    QJsonObject sheet_obj;
    QJsonArray placements;
    for (auto it = sheet.constBegin(); it != sheet.constEnd(); ++it) {
      QJsonObject placement;
      placement.insert("id", it.key());
      placement.insert("x", it.value().first.x());
      placement.insert("y", it.value().first.y());
      placement.insert("rotation", it.value().second);
      placements.push_back(placement);
    }
    sheet_obj.insert("placements", placements);
    sheets.push_back(sheet_obj);
  }
  return sheets;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("DeepNestTest");
  QCoreApplication::setApplicationVersion("0.1");

  QCommandLineParser parser;
  parser.setApplicationDescription("Simulatore DeepNest C++");
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption width_option({"w", "width"}, "Larghezza del foglio", "width", "1000");
  QCommandLineOption height_option({"h", "height"}, "Altezza del foglio", "height", "1000");
  QCommandLineOption slices_option({"s", "slices"},
                                   "Numero di suddivisioni casuali del foglio",
                                   "slices", "5");
  QCommandLineOption population_option("population", "Dimensione popolazione", "population",
                                       "10");
  QCommandLineOption generations_option("generations", "Numero generazioni", "generations",
                                        "20");
  QCommandLineOption spacing_option("spacing", "Spaziatura tra parti", "spacing", "5");
  QCommandLineOption rotation_option("rotation", "Rotazione massima", "rotation", "90");
  QCommandLineOption output_option({"o", "output"}, "File JSON di output", "output");

  parser.addOption(width_option);
  parser.addOption(height_option);
  parser.addOption(slices_option);
  parser.addOption(population_option);
  parser.addOption(generations_option);
  parser.addOption(spacing_option);
  parser.addOption(rotation_option);
  parser.addOption(output_option);
  parser.addPositionalArgument("svg", "File SVG da importare", "[svg...]");

  parser.process(app);

  double width = parser.value(width_option).toDouble();
  double height = parser.value(height_option).toDouble();
  int slices = parser.value(slices_option).toInt();

  QRectF sheet_rect(0, 0, width, height);
  QPainterPath sheet_path;
  sheet_path.addRect(sheet_rect);

  DeepNestConfig config;
  config.set_population_size(parser.value(population_option).toInt());
  config.set_generations(parser.value(generations_option).toInt());
  config.set_spacing(parser.value(spacing_option).toDouble());
  config.set_rotation_step(parser.value(rotation_option).toDouble());

  QHash<QString, QPainterPath> parts;
  auto random_parts = GenerateRandomSlices(sheet_rect, slices);
  for (int i = 0; i < random_parts.size(); ++i) {
    parts.insert(QStringLiteral("random_%1").arg(i), random_parts[static_cast<size_t>(i)]);
  }

  const auto svg_files = parser.positionalArguments();
  for (const auto& svg : svg_files) {
    QPainterPath path = LoadSvgPath(svg);
    if (!path.isEmpty()) {
      parts.insert(svg, path);
    }
  }

  DeepNestSolver solver;
  solver.SetConfig(config);
  auto result = solver.Solve(sheet_path, parts);

  auto json_array = SerializeResult(result);
  QJsonDocument doc(json_array);

  if (parser.isSet(output_option)) {
    QFile file(parser.value(output_option));
    if (file.open(QIODevice::WriteOnly)) {
      file.write(doc.toJson());
      file.close();
      std::cout << "Risultato scritto su " << parser.value(output_option).toStdString()
                << std::endl;
    } else {
      std::cerr << "Impossibile scrivere il file di output" << std::endl;
    }
  } else {
    std::cout << doc.toJson().toStdString() << std::endl;
  }

  return 0;
}
