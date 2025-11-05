#include "deepnest/GeneticAlgorithm.h"

#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <utility>

#include <boost/thread/future.hpp>

namespace deepnest {

namespace {

std::vector<int> BuildSeedOrder(const std::vector<NestPolygon>& parts) {
  std::vector<int> order(parts.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&parts](int lhs, int rhs) {
    const Coordinate lhs_area = std::abs(ComputeArea(parts[lhs].geometry()));
    const Coordinate rhs_area = std::abs(ComputeArea(parts[rhs].geometry()));
    if (AlmostEqual(lhs_area, rhs_area)) {
      return lhs < rhs;
    }
    return lhs_area > rhs_area;
  });
  return order;
}

}  // namespace

GeneticAlgorithm::GeneticAlgorithm(const DeepNestConfig& config,
                                   PlacementWorker* worker)
    : config_(config), worker_(worker),
      random_(std::random_device {}()) {}

void GeneticAlgorithm::set_progress_callback(ProgressCallback callback) {
  progress_callback_ = std::move(callback);
}

double GeneticAlgorithm::MutationProbability() const {
  return std::clamp(config_.mutation_rate() * 0.01, 0.0, 1.0);
}

double GeneticAlgorithm::RandomRotation() const {
  const double step = config_.rotation_step() > 0.0
                          ? config_.rotation_step()
                          : (config_.rotations() > 0
                                 ? 360.0 / std::max(1, config_.rotations())
                                 : 360.0);
  if (step <= 0.0) {
    return 0.0;
  }
  const int steps = static_cast<int>(std::round(360.0 / step));
  std::uniform_int_distribution<int> distribution(0, std::max(0, steps - 1));
  return distribution(random_) * step;
}

std::vector<double> GeneticAlgorithm::GenerateRotations(std::size_t count) const {
  std::vector<double> rotations;
  rotations.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    rotations.push_back(RandomRotation());
  }
  return rotations;
}

Individual GeneticAlgorithm::CreateSeed(const std::vector<int>& base_order) const {
  Individual seed;
  seed.order = base_order;
  seed.rotations = GenerateRotations(base_order.size());
  seed.evaluated = false;
  return seed;
}

std::vector<NestPolygon> GeneticAlgorithm::BuildArrangement(
    const Individual& individual, const std::vector<NestPolygon>& parts) const {
  std::vector<NestPolygon> arrangement;
  arrangement.reserve(individual.order.size());
  for (std::size_t i = 0; i < individual.order.size(); ++i) {
    const int index = individual.order[i];
    NestPolygon clone = parts[index].Clone();
    if (!individual.rotations.empty() && i < individual.rotations.size()) {
      const double rotation = individual.rotations[i];
      if (!AlmostEqual(rotation, 0.0)) {
        clone.Rotate(rotation);
      }
      clone.set_rotation(rotation);
    }
    arrangement.push_back(std::move(clone));
  }
  return arrangement;
}

Individual GeneticAlgorithm::Mutate(const Individual& individual) {
  Individual mutant = individual;
  mutant.evaluated = false;
  mutant.result = PlacementResult();
  if (mutant.order.empty()) {
    return mutant;
  }

  std::uniform_real_distribution<double> probability(0.0, 1.0);
  const double threshold = MutationProbability();

  for (std::size_t i = 0; i + 1 < mutant.order.size(); ++i) {
    if (probability(random_) < threshold) {
      const std::size_t j = (i + 1 + mutant.order.size() - 1) % mutant.order.size();
      std::swap(mutant.order[i], mutant.order[j]);
    }
    if (!mutant.rotations.empty() && probability(random_) < threshold) {
      mutant.rotations[i] = RandomRotation();
    }
  }

  if (!mutant.rotations.empty() && probability(random_) < threshold) {
    mutant.rotations.back() = RandomRotation();
  }

  return mutant;
}

PlacementResult GeneticAlgorithm::Evaluate(
    Individual* individual, const std::vector<NestPolygon>& parts) {
  if (!worker_ || !individual) {
    return PlacementResult();
  }
  std::vector<NestPolygon> arranged = BuildArrangement(*individual, parts);
  PlacementResult result = worker_->Place(arranged);
  individual->result = result;
  individual->evaluated = true;
  return result;
}

PlacementResult GeneticAlgorithm::Run(const std::vector<NestPolygon>& parts) {
  population_.clear();
  if (parts.empty()) {
    return PlacementResult();
  }

  const std::vector<int> base_order = BuildSeedOrder(parts);
  const int population_size = std::max(1, config_.population_size());

  population_.push_back(CreateSeed(base_order));
  while (static_cast<int>(population_.size()) < population_size) {
    population_.push_back(Mutate(population_.front()));
  }

  PlacementResult best_result;
  double best_fitness = std::numeric_limits<double>::infinity();

  for (int generation = 0; generation < std::max(1, config_.generations());
       ++generation) {
    if (thread_pool_ && config_.threads() > 1) {
      std::vector<std::pair<Individual*, boost::future<PlacementResult>>> futures;
      futures.reserve(population_.size());

      for (auto& individual : population_) {
        if (!individual.evaluated) {
          Individual* target = &individual;
          futures.emplace_back(target, thread_pool_->Submit([this, target, &parts]() {
            return Evaluate(target, parts);
          }));
        } else if (individual.result.cost.fitness < best_fitness) {
          best_fitness = individual.result.cost.fitness;
          best_result = individual.result;
        }
      }

      for (auto& entry : futures) {
        PlacementResult result = entry.second.get();
        if (result.cost.fitness < best_fitness) {
          best_fitness = result.cost.fitness;
          best_result = result;
        }
      }
    } else {
      for (auto& individual : population_) {
        if (!individual.evaluated) {
          PlacementResult result = Evaluate(&individual, parts);
          if (result.cost.fitness < best_fitness) {
            best_fitness = result.cost.fitness;
            best_result = result;
          }
        } else if (individual.result.cost.fitness < best_fitness) {
          best_fitness = individual.result.cost.fitness;
          best_result = individual.result;
        }
      }
    }

    std::sort(population_.begin(), population_.end(),
              [](const Individual& lhs, const Individual& rhs) {
                return lhs.result.cost.fitness < rhs.result.cost.fitness;
              });

    if (generation + 1 >= std::max(1, config_.generations())) {
      break;
    }

    if (progress_callback_) {
      progress_callback_(generation, best_result);
    }

    std::vector<Individual> next_population;
    next_population.reserve(population_.size());
    next_population.push_back(population_.front());

    while (next_population.size() < population_.size()) {
      const std::size_t parent_index =
          next_population.size() % population_.size();
      next_population.push_back(Mutate(population_[parent_index]));
    }

    population_ = std::move(next_population);
  }

  if (progress_callback_) {
    const int final_generation = std::max(0, std::max(1, config_.generations()) - 1);
    progress_callback_(final_generation, best_result);
  }

  return best_result;
}

}  // namespace deepnest
