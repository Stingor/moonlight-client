#include "loading/loading_plans.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::LoadStep;
using ro::core::LoadStepWeight;

// ⚠ LES DURÉES SONT CELLES DU JOURNAL, en Release, sur une grande ville
// (morocc, umbala) : les lignes « chargement … : N ms » et « carte … : lue en
// N ms ». Le Debug les multiplie toutes à peu près d'autant, et le rapport,
// seul à compter, tient.
constexpr std::array<LoadStepWeight, 2> kStartupPlan = {
    LoadStepWeight{.step = LoadStep::kInterface, .expected_ms = 15.0F},
    LoadStepWeight{.step = LoadStep::kSound, .expected_ms = 40.0F},
};
constexpr std::array<LoadStepWeight, 1> kWardrobePlan = {
    LoadStepWeight{.step = LoadStep::kWardrobe, .expected_ms = 100.0F},
};
constexpr float kTablesMs = 500.0F;
constexpr std::array<LoadStepWeight, 1> kTablesPlan = {
    LoadStepWeight{.step = LoadStep::kTables, .expected_ms = kTablesMs},
};
// Le sol se bâtit en quinze millisecondes à plat, en deux cents arrondi.
constexpr float kFlatGroundMs = 15.0F;
constexpr float kRoundedGroundMs = 200.0F;
constexpr std::array<LoadStepWeight, 9> kMapPlanWithoutGround = {
    LoadStepWeight{.step = LoadStep::kMap, .expected_ms = 20.0F},
    LoadStepWeight{.step = LoadStep::kLights, .expected_ms = 10.0F},
    LoadStepWeight{.step = LoadStep::kTerrain, .expected_ms = 25.0F},
    LoadStepWeight{.step = LoadStep::kWater, .expected_ms = 5.0F},
    LoadStepWeight{.step = LoadStep::kModels, .expected_ms = 90.0F},
    LoadStepWeight{.step = LoadStep::kMapEffects, .expected_ms = 10.0F},
    LoadStepWeight{.step = LoadStep::kWalkGrid, .expected_ms = 20.0F},
    LoadStepWeight{.step = LoadStep::kUpload, .expected_ms = 10.0F},
    LoadStepWeight{.step = LoadStep::kModelUpload, .expected_ms = 30.0F},
};
constexpr LoadStepWeight kGameEffects{.step = LoadStep::kEffects, .expected_ms = 5.0F};

constexpr float kPercent = 100.0F;
constexpr float kPercentRoundingSlack = 0.001F;
constexpr std::size_t kLineCapacity = 128;

}  // namespace

std::span<const LoadStepWeight> StartupPlan() noexcept { return kStartupPlan; }
std::span<const LoadStepWeight> WardrobePlan() noexcept { return kWardrobePlan; }
std::span<const LoadStepWeight> TablesPlan() noexcept { return kTablesPlan; }

std::vector<LoadStepWeight> MapPlan(bool rounded_ground) {
  std::vector<LoadStepWeight> plan(kMapPlanWithoutGround.begin(), kMapPlanWithoutGround.end());
  plan.push_back(LoadStepWeight{.step = LoadStep::kGround,
                                .expected_ms = rounded_ground ? kRoundedGroundMs : kFlatGroundMs});
  return plan;
}

std::vector<LoadStepWeight> GameOpenPlan(bool rounded_ground, bool tables_pending) {
  std::vector<LoadStepWeight> plan = MapPlan(rounded_ground);
  if (tables_pending) {
    plan.insert(plan.end(), kTablesPlan.begin(), kTablesPlan.end());
  }
  plan.push_back(kGameEffects);
  return plan;
}

const char* LoadStepLabel(LoadStep step) {
  switch (step) {
    case LoadStep::kInterface:
      return ro::core::Tr("Interface");
    case LoadStep::kSound:
      return ro::core::Tr("Son");
    case LoadStep::kWardrobe:
      return ro::core::Tr("Garde-robe");
    case LoadStep::kTables:
      return ro::core::Tr("Tables du jeu");
    case LoadStep::kMap:
      return ro::core::Tr("Carte");
    case LoadStep::kLights:
      return ro::core::Tr("Lumières");
    case LoadStep::kTerrain:
      return ro::core::Tr("Terrain");
    case LoadStep::kGround:
      return ro::core::Tr("Sol");
    case LoadStep::kWater:
      return ro::core::Tr("Eau");
    case LoadStep::kModels:
      return ro::core::Tr("Modèles");
    case LoadStep::kMapEffects:
      return ro::core::Tr("Effets de la carte");
    case LoadStep::kWalkGrid:
      return ro::core::Tr("Grille de marche");
    case LoadStep::kUpload:
      return ro::core::Tr("Envoi à la carte graphique");
    case LoadStep::kModelUpload:
      return ro::core::Tr("Envoi des modèles");
    case LoadStep::kEffects:
      return ro::core::Tr("Effets");
  }
  return "";
}

std::string LoadStepLine(const ro::core::LoadProgress& progress) {
  if (!progress.step().has_value()) {
    return {};
  }
  std::string line = LoadStepLabel(*progress.step());
  if (progress.total() > 0) {
    std::array<char, kLineCapacity> counts{};
    std::snprintf(counts.data(), counts.size(), " %zu / %zu", progress.done(), progress.total());
    line += counts.data();
  }
  return line;
}

std::string LoadPercentText(float fraction) {
  // ⚠ ARRONDI PAR DÉFAUT, pour ne pas dire « 100 % » avant la fin — mais avec
  // une marge : une somme de flottants complète vaut parfois 0,9999999.
  const int percent = static_cast<int>(
      std::floor(std::clamp(fraction, 0.0F, 1.0F) * kPercent + kPercentRoundingSlack));
  std::array<char, kLineCapacity> text{};
  std::snprintf(text.data(), text.size(), ro::core::Tr("%d %%"), percent);
  return std::string(text.data());
}

}  // namespace moonclient
