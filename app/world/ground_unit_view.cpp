#include "world/ground_unit_view.h"

#include <array>
#include <memory>
#include <utility>

#include "ro/core/log.h"
#include "ro/formats/rsm.h"
#include "ro/render/map_scene.h"
#include "ro/render/skill_unit_table.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Ce que le placement d'un modèle attend : une position RELATIVE au centre de
// la carte, et non un point du monde. Voir ModelInstanceMatrix — la moitié d'un
// cube, et le retrait constant en profondeur que la référence applique.
constexpr float kHalfCube = 5.0F;
constexpr float kDepthBias = 10.0F;

// Le témoin d'une unité posée : le numéro de son modèle et sa case dans un seul
// nombre. Deux pièges différents sur la même case le font changer, un même
// piège déplacé aussi.
constexpr std::uint64_t kCellShift = 20;
constexpr std::uint64_t kUnitShift = 40;

std::uint64_t Witness(std::uint32_t model_unit_id, const ro::sim::CellPosition& cell) {
  return (static_cast<std::uint64_t>(model_unit_id) << kUnitShift) |
         (static_cast<std::uint64_t>(cell.y) << kCellShift) | static_cast<std::uint64_t>(cell.x);
}

// Une unité qui n'a pas de modèle à montrer : cachée, ou muette — ce par quoi
// le serveur cache ce qu'il ne veut pas montrer. Elle occupe une case, et rien
// de plus.
bool ShowsNoModel(const ro::sim::SkillUnit& unit) {
  return !unit.visible || unit.unit_id == ro::sim::kSilentSkillUnit;
}

// Le placement qui pose un modèle à ce point du monde, sans rotation ni mise à
// l'échelle.
//
// ModelInstanceMatrix compose la position en partant du CENTRE de la carte puis
// retourne l'axe de profondeur ; on inverse ici ce chemin plutôt que d'écrire
// une seconde version de la matrice, qui divergerait de la première au premier
// correctif.
ro::formats::RswModel PlacementAt(const std::array<float, 3>& ground, std::int32_t gnd_width,
                                  std::int32_t gnd_height) {
  ro::formats::RswModel placement;
  placement.position[0] = ground[0] - kHalfCube * static_cast<float>(gnd_width);
  placement.position[1] = -ground[1];
  placement.position[2] = -ground[2] + kDepthBias + kHalfCube * static_cast<float>(gnd_height);
  placement.scale = {1.0F, 1.0F, 1.0F};
  return placement;
}

}  // namespace

const GroundUnits::Model* GroundUnits::ModelFor(std::uint32_t unit_id) {
  const auto known = models_.find(unit_id);
  if (known != models_.end()) {
    return known->second.mesh.textures.empty() ? nullptr : &known->second;
  }

  Model model;
  const std::string path = ro::render::SkillUnitModelPath(unit_id);
  if (!path.empty()) {
    // Read : le chemin vient de NOTRE table, donc de l'UTF-8, et non d'un
    // fichier du jeu déjà écrit dans la page de codes de l'archive.
    auto bytes = vfs_->Read(path);
    if (!bytes) {
      ro::core::LogWarning(
          "unité au sol {} : modèle absent — {} — le piège ou la zone ne paraîtra pas", unit_id,
          path);
    } else if (auto parsed = ro::formats::RsmModel::Parse(*bytes); !parsed) {
      ro::core::LogWarning(
          "unité au sol {} : modèle illisible — {} — le piège ou la zone ne paraîtra pas", unit_id,
          path);
    } else {
      // Pour les types d'ombrage 1 et 2, le client éteint de MOITIÉ un sommet
      // dont la couleur de sommet de texture n'est pas nulle — et un modèle 1.1,
      // qui n'en porte pas, compte pour blanc. Aucun modèle de l'installation
      // n'est concerné : les .rsm sont tous en 1.4 ou 1.5, et aucun .rsm ni
      // .rsm2 ne porte de couleur non nulle. Le maillage ne garde donc pas
      // cette couleur.
      model.mesh = ro::render::BuildModelMesh(*parsed);
      model.rig = ro::render::BuildModelRig(*parsed, model.mesh);
      model.rsm_version = parsed->version();
      model.shade_type = parsed->shade_type();
      for (const std::string& texture : model.mesh.textures) {
        std::optional<ro::formats::Image> image = ro::render::LoadSceneTexture(*vfs_, texture);
        model.textures.push_back(image.has_value()
                                     ? std::make_shared<const ro::formats::Image>(std::move(*image))
                                     : nullptr);
      }
    }
  }
  const auto placed = models_.emplace(unit_id, std::move(model)).first;
  return placed->second.mesh.textures.empty() ? nullptr : &placed->second;
}

void GroundUnits::Follow(const ro::sim::World& world) {
  std::erase_if(built_, [&world](const auto& kept) {
    const auto unit = world.skill_units().find(kept.first);
    return unit == world.skill_units().end() || ShowsNoModel(unit->second);
  });
  for (const auto& [id, unit] : world.skill_units()) {
    if (ShowsNoModel(unit)) {
      continue;
    }
    const auto known = built_.find(id);
    if (known != built_.end() && known->second.look_seen_ms == unit.look_since_ms) {
      continue;
    }
    // UNE APPARENCE NOUVELLE — la pose, ou le piège qui se déclenche — rebâtit
    // le modèle, figé sur son image de pose. Sans modèle pour elle, l'ancien
    // reste en place.
    const Model* model = ModelFor(unit.unit_id);
    if (model == nullptr) {
      if (known != built_.end()) {
        known->second.look_seen_ms = unit.look_since_ms;
      }
      continue;
    }
    Built fresh{.model_unit_id = unit.unit_id,
                .look_seen_ms = unit.look_since_ms,
                .built_ms = unit.look_since_ms,
                .play = ro::render::GroundUnitAtRest(model->rig),
                .group = std::nullopt};
    // Le même modèle garde sa place dans les lots : rien ne se retéléverse.
    if (known != built_.end() && known->second.model_unit_id == unit.unit_id) {
      fresh.group = known->second.group;
    }
    built_.insert_or_assign(id, fresh);
  }
}

const std::vector<ro::render::ModelInstances>* GroundUnits::Changed(const MapStage& stage,
                                                                    const ro::sim::World& world) {
  Follow(world);

  // LES EMPRISES, À CHAQUE IMAGE et pour toute unité qui en porte une, qu'elle
  // ait un modèle ou non : un bloc d'Ice Wall n'en a pas, il n'est fait que de
  // son effet. Quelques unités, un point du sol chacune.
  picks_.clear();
  speakers_.clear();
  for (const auto& [id, unit] : world.skill_units()) {
    // Celles qui PARLENT, qu'elles portent une emprise ou non : la Talkie Box
    // qui se déclenche prend aussitôt l'apparence du piège usé, qui n'en a pas.
    if (!unit.said.empty()) {
      const std::array<float, 3> ground =
          GroundPositionAt(stage, static_cast<float>(unit.cell.x), static_cast<float>(unit.cell.y));
      speakers_.push_back(GroundUnitPick{
          .id = id, .unit_id = unit.unit_id, .ground = {ground[0], ground[1], ground[2]}});
    }
    if (!GroundUnitOffersPick(world, id)) {
      continue;
    }
    const std::array<float, 3> ground =
        GroundPositionAt(stage, static_cast<float>(unit.cell.x), static_cast<float>(unit.cell.y));
    picks_.push_back(GroundUnitPick{
        .id = id, .unit_id = unit.unit_id, .ground = {ground[0], ground[1], ground[2]}});
  }

  // L'identifiant ET le témoin : une unité remplacée par une autre au même
  // endroit change la place de chacune dans les lots.
  std::vector<std::uint64_t> witness;
  witness.reserve(built_.size() * 2);
  for (const auto& [id, built] : built_) {
    witness.push_back(id);
    witness.push_back(Witness(built.model_unit_id, world.skill_units().at(id).cell));
  }
  if (witness == witness_) {
    return nullptr;
  }
  witness_ = std::move(witness);

  // Un lot par MODÈLE : dix pièges du même type ne font qu'un maillage et dix
  // poses, comme une carte pose le même arbre des centaines de fois.
  std::map<std::uint32_t, std::vector<std::pair<std::uint32_t, glm::mat4>>> poses;
  for (const auto& [id, built] : built_) {
    const ro::sim::SkillUnit& unit = world.skill_units().at(id);
    const Model* model = ModelFor(built.model_unit_id);
    const std::array<float, 3> ground =
        GroundPositionAt(stage, static_cast<float>(unit.cell.x), static_cast<float>(unit.cell.y));
    const ro::formats::RswModel placement =
        PlacementAt(ground, stage.scene.gnd.width(), stage.scene.gnd.height());
    poses[built.model_unit_id].emplace_back(
        id,
        ro::render::ModelInstanceMatrix(placement, stage.scene.gnd.width(),
                                        stage.scene.gnd.height(), model->mesh, model->rsm_version));
  }
  placed_ = built_.size();

  instances_.clear();
  instances_.reserve(poses.size());
  for (auto& [model_unit_id, placed] : poses) {
    const Model* model = ModelFor(model_unit_id);
    ro::render::ModelInstances instances;
    instances.mesh = model->mesh;
    instances.textures = model->textures;
    instances.rsm_version = model->rsm_version;
    instances.shade_type = model->shade_type;
    instances.rig = model->rig;
    for (const auto& [id, transform] : placed) {
      Built& built = built_.at(id);
      // UN GROUPE PAR UNITÉ quand le modèle bouge : chacune joue quand ELLE
      // se déclenche. Un modèle immobile se dessine d'un bloc.
      if (model->rig.empty()) {
        built.group.reset();
        instances.transforms.push_back(transform);
        continue;
      }
      built.group = std::make_pair(instances_.size(), instances.animated.size());
      instances.animated.push_back(ro::render::ModelPlaybackGroup{.transforms = {transform}});
    }
    instances_.push_back(std::move(instances));
  }

  clock_ = ro::render::ModelClock();
  for (const auto& [id, built] : built_) {
    if (built.group.has_value()) {
      clock_.Hold(instances_, built.group->first, built.group->second, built.play.frame);
    }
  }
  return &instances_;
}

const ro::render::ModelClock& GroundUnits::Advance(const ro::sim::World& world,
                                                   std::uint32_t now_ms) {
  if (!origin_ms_.has_value()) {
    origin_ms_ = now_ms;
  }
  // UN SEUL PAS PAR APPEL, même quand plusieurs se sont écoulés : comme le
  // décor de la carte, voir `ModelClock`. Soustraction non signée : l'horloge
  // du système peut repasser par zéro.
  const std::uint64_t steps = (now_ms - *origin_ms_) / ro::render::kModelStepMs;
  const bool stepped = steps > steps_seen_;
  if (stepped) {
    steps_seen_ = steps;
  }
  for (auto& [id, built] : built_) {
    if (!built.group.has_value()) {
      continue;
    }
    if (stepped) {
      const auto unit = world.skill_units().find(id);
      const bool sprung = unit != world.skill_units().end() && unit->second.sprung_ms.has_value();
      ro::render::StepGroundUnit(ModelFor(built.model_unit_id)->rig, sprung, built.built_ms, now_ms,
                                 built.play);
    }
    clock_.Hold(instances_, built.group->first, built.group->second, built.play.frame);
  }
  return clock_;
}

}  // namespace moonclient
