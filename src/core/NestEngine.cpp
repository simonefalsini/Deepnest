#include "deepnest/NestEngine.h"

#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <utility>

#include <boost/thread/future.hpp>

namespace deepnest {

namespace {

double NormalizeRotation(double rotation) {
  double normalized = std::fmod(rotation, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }
  return normalized;
}

}  // namespace

NestEngine::NestEngine(const DeepNestConfig& config)
    : config_(config),
      cache_(),
      generator_(&cache_, config_),
      repository_(config_),
      worker_(&generator_, config_),
      algorithm_(config_, &worker_) {
  ConfigureThreadPool();
}

void NestEngine::Initialize(const QPainterPath& sheet,
                            const QHash<QString, QPainterPath>& parts) {
  cache_.SyncConfig(config_);
  repository_.LoadSheet(sheet);
  repository_.LoadParts(parts);
  parts_ = repository_.items();
  sheet_ = repository_.sheet();
  worker_.SetSheets({sheet_});
  PrecomputeNfps();
}

PlacementResult NestEngine::Run() { return RunWithParts(parts_); }

PlacementResult NestEngine::RunWithParts(const std::vector<NestPolygon>& parts) {
  return algorithm_.Run(parts);
}

void NestEngine::UpdateConfig(const DeepNestConfig& config) {
  if (config_ == config) {
    return;
  }

  config_ = config;
  RebuildComponents();
  Reset();
}

void NestEngine::SetProgressCallback(GeneticAlgorithm::ProgressCallback callback) {
  progress_callback_ = std::move(callback);
  algorithm_.set_progress_callback(progress_callback_);
}

void NestEngine::ClearCache() { cache_.Clear(); }

void NestEngine::Reset() {
  parts_.clear();
  sheet_ = NestPolygon();
  std::vector<NestPolygon> empty_sheets;
  worker_.SetSheets(empty_sheets);
}

std::vector<double> NestEngine::CandidateRotations() const {
  std::vector<double> rotations;
  const int steps = std::max(1, config_.rotations());
  const double step = config_.rotation_step() > 0.0
                          ? config_.rotation_step()
                          : 360.0 / static_cast<double>(steps);
  rotations.reserve(static_cast<std::size_t>(steps));

  for (int i = 0; i < steps; ++i) {
    double angle = NormalizeRotation(step * static_cast<double>(i));
    const bool exists = std::any_of(rotations.begin(), rotations.end(),
                                    [angle](double value) {
                                      return AlmostEqual(value, angle);
                                    });
    if (!exists) {
      rotations.push_back(angle);
    }
  }

  if (rotations.empty()) {
    rotations.push_back(0.0);
  }

  return rotations;
}

void NestEngine::PrecomputeNfps() {
  if (parts_.empty()) {
    return;
  }

  const bool parallel = thread_pool_ && config_.threads() > 1;
  const std::vector<double> rotations = CandidateRotations();
  std::vector<boost::unique_future<void>> futures;
  if (parallel) {
    futures.reserve(parts_.size() * parts_.size() * rotations.size() *
                    rotations.size());
  }

  const auto schedule = [&](auto&& callable) {
    if (parallel) {
      futures.push_back(thread_pool_->Submit(std::forward<decltype(callable)>(callable)));
    } else {
      callable();
    }
  };

  for (const auto& part : parts_) {
    const NestPolygon* part_ptr = &part;
    for (double rotation : rotations) {
      schedule([this, part_ptr, rotation]() {
        NestPolygon rotated = part_ptr->Clone();
        if (!AlmostEqual(rotation, 0.0)) {
          rotated.Rotate(rotation);
        }
        rotated.set_rotation(rotation);
        (void)generator_.Compute(sheet_, rotated, /*inside=*/true);
      });
    }
  }

  for (std::size_t i = 0; i < parts_.size(); ++i) {
    const NestPolygon* placed_ptr = &parts_[i];
    for (std::size_t j = 0; j < parts_.size(); ++j) {
      if (i == j) {
        continue;
      }
      const NestPolygon* candidate_ptr = &parts_[j];
      for (double rotation_a : rotations) {
        for (double rotation_b : rotations) {
          schedule([this, placed_ptr, candidate_ptr, rotation_a, rotation_b]() {
            NestPolygon placed = placed_ptr->Clone();
            if (!AlmostEqual(rotation_a, 0.0)) {
              placed.Rotate(rotation_a);
            }
            placed.set_rotation(rotation_a);

            NestPolygon candidate = candidate_ptr->Clone();
            if (!AlmostEqual(rotation_b, 0.0)) {
              candidate.Rotate(rotation_b);
            }
            candidate.set_rotation(rotation_b);

            (void)generator_.Compute(placed, candidate, /*inside=*/false);
          });
        }
      }
    }
  }

  if (parallel) {
    for (auto& future : futures) {
      future.get();
    }
  }
}

void NestEngine::RebuildComponents() {
  thread_pool_.reset();

  generator_.~NfpGenerator();
  new (&generator_) NfpGenerator(&cache_, config_);

  repository_.~NestPartRepository();
  new (&repository_) NestPartRepository(config_);

  worker_.~PlacementWorker();
  new (&worker_) PlacementWorker(&generator_, config_);

  algorithm_.~GeneticAlgorithm();
  new (&algorithm_) GeneticAlgorithm(config_, &worker_);

  ConfigureThreadPool();
  algorithm_.set_progress_callback(progress_callback_);
}

void NestEngine::ConfigureThreadPool() {
  if (config_.threads() > 1) {
    thread_pool_ = std::make_unique<ThreadPool>(config_.threads());
    algorithm_.set_thread_pool(thread_pool_.get());
  } else {
    thread_pool_.reset();
    algorithm_.set_thread_pool(nullptr);
  }
}

}  // namespace deepnest
