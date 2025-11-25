#include "../../include/deepnest/config/DeepNestConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>
#include <ctime>

namespace deepnest {

// Initialize static member
std::unique_ptr<DeepNestConfig> DeepNestConfig::instance = nullptr;

DeepNestConfig::DeepNestConfig() {
    resetToDefaults();
}

DeepNestConfig& DeepNestConfig::getInstance() {
    if (!instance) {
        instance = std::unique_ptr<DeepNestConfig>(new DeepNestConfig());
    }
    return *instance;
}

void DeepNestConfig::resetToDefaults() {
    // Default values from deepnest.js (lines 20-33)
    clipperScale = 10000000.0;
    curveTolerance = 0.3;
    spacing = 0.0;
    rotations = 4;
    populationSize = 10;
    mutationRate = 10;
    threads = 4;
    placementType = "gravity";
    mergeLines = true;
    timeRatio = 0.5;
    scale = 72.0;
    simplify = false;
    overlapTolerance = 0.0001;  // High precision overlap detection

    // Additional parameters from svgnest.js
    useHoles = false;
    exploreConcave = false;

    // Additional runtime parameters
    maxIterations = 0;  // 0 = unlimited
    timeoutSeconds = 0;  // 0 = no timeout
    progressive = false;
    gravityDirection = GravityDirection::LEFT; // Default: prefer leftmost positions
    randomSeed = 0;  // 0 = use system time
}

void DeepNestConfig::loadFromJson(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open configuration file: " + path.toStdString());
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        throw std::runtime_error("Invalid JSON configuration file");
    }

    QJsonObject obj = doc.object();

    // Load parameters with validation (similar to svgnest.js config() method)
    if (obj.contains("clipperScale")) {
        clipperScale = obj["clipperScale"].toDouble(clipperScale);
    }

    if (obj.contains("curveTolerance")) {
        double val = obj["curveTolerance"].toDouble();
        if (val > 0) {
            curveTolerance = val;
        }
    }

    if (obj.contains("spacing")) {
        spacing = obj["spacing"].toDouble(spacing);
    }

    if (obj.contains("rotations")) {
        int val = obj["rotations"].toInt();
        if (val > 0) {
            rotations = val;
        }
    }

    if (obj.contains("populationSize")) {
        int val = obj["populationSize"].toInt();
        if (val > 2) {
            populationSize = val;
        }
    }

    if (obj.contains("mutationRate")) {
        int val = obj["mutationRate"].toInt();
        if (val > 0 && val <= 100) {
            mutationRate = val;
        }
    }

    if (obj.contains("threads")) {
        int val = obj["threads"].toInt();
        if (val > 0) {
            threads = val;
        }
    }

    if (obj.contains("placementType")) {
        placementType = obj["placementType"].toString().toStdString();
    }

    if (obj.contains("mergeLines")) {
        mergeLines = obj["mergeLines"].toBool();
    }

    if (obj.contains("timeRatio")) {
        timeRatio = obj["timeRatio"].toDouble(timeRatio);
    }

    if (obj.contains("scale")) {
        scale = obj["scale"].toDouble(scale);
    }

    if (obj.contains("simplify")) {
        simplify = obj["simplify"].toBool();
    }

    if (obj.contains("useHoles")) {
        useHoles = obj["useHoles"].toBool();
    }

    if (obj.contains("exploreConcave")) {
        exploreConcave = obj["exploreConcave"].toBool();
    }

    if (obj.contains("maxIterations")) {
        maxIterations = obj["maxIterations"].toInt(maxIterations);
    }

    if (obj.contains("timeoutSeconds")) {
        timeoutSeconds = obj["timeoutSeconds"].toInt(timeoutSeconds);
    }

    if (obj.contains("progressive")) {
        progressive = obj["progressive"].toBool();
    }

    if (obj.contains("randomSeed")) {
        randomSeed = obj["randomSeed"].toInt(randomSeed);
    }
}

void DeepNestConfig::saveToJson(const QString& path) const {
    QJsonObject obj;

    obj["clipperScale"] = clipperScale;
    obj["curveTolerance"] = curveTolerance;
    obj["spacing"] = spacing;
    obj["rotations"] = rotations;
    obj["populationSize"] = populationSize;
    obj["mutationRate"] = mutationRate;
    obj["threads"] = threads;
    obj["placementType"] = QString::fromStdString(placementType);
    obj["mergeLines"] = mergeLines;
    obj["timeRatio"] = timeRatio;
    obj["scale"] = scale;
    obj["simplify"] = simplify;
    obj["useHoles"] = useHoles;
    obj["exploreConcave"] = exploreConcave;
    obj["maxIterations"] = maxIterations;
    obj["timeoutSeconds"] = timeoutSeconds;
    obj["progressive"] = progressive;
    obj["randomSeed"] = static_cast<int>(randomSeed);

    QJsonDocument doc(obj);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        throw std::runtime_error("Failed to open file for writing: " + path.toStdString());
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

// Setter methods with validation

void DeepNestConfig::setClipperScale(double value) {
    if (value > 0) {
        clipperScale = value;
    } else {
        throw std::invalid_argument("Clipper scale must be positive");
    }
}

void DeepNestConfig::setSpacing(double value) {
    if (value >= 0) {
        spacing = value;
    } else {
        throw std::invalid_argument("Spacing must be non-negative");
    }
}

void DeepNestConfig::setRotations(int value) {
    if (value > 0) {
        rotations = value;
    } else {
        throw std::invalid_argument("Rotations must be positive");
    }
}

void DeepNestConfig::setPopulationSize(int value) {
    if (value > 2) {
        populationSize = value;
    } else {
        throw std::invalid_argument("Population size must be greater than 2");
    }
}

void DeepNestConfig::setMutationRate(int value) {
    if (value >= 0 && value <= 100) {
        mutationRate = value;
    } else {
        throw std::invalid_argument("Mutation rate must be between 0 and 100");
    }
}

void DeepNestConfig::setThreads(int value) {
    if (value > 0) {
        threads = value;
    } else {
        throw std::invalid_argument("Threads count must be positive");
    }
}

} // namespace deepnest
