// CE QU'UNE MÉTHODE NOMMÉE FAIT DE L'IMAGE QU'ELLE SÈME, au-delà de la semer :
// la coucher au sol, la tenir au-dessus d'une tête, en compter les copies ; et
// les états nommés qui lisent leur valeur.
//
// Séparé de `effect_view.cpp`, dont `Drawn` ne fait qu'appeler ces gestes : ils
// ne concernent que les effets nommés et les charges d'un état.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/render/charge_count_effect.h"
#include "ro/render/effect_laid.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_table.h"
#include "ro/render/skill_named_table.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_named_sounds.h"
#include "effects/effect_view.h"

namespace moonclient {
namespace {

// Le trou d'un nom que la valeur de l'état remplit, sur deux chiffres au moins.
constexpr std::string_view kValueHole = "%02d";
// De quoi écrire n'importe quel entier sur trente-deux bits.
constexpr std::size_t kNumberBuffer = 16;

[[nodiscard]] std::string WithValue(std::string_view name, std::int32_t value) {
  std::string forme(name);
  if (const std::size_t trou = forme.find(kValueHole); trou != std::string::npos) {
    std::array<char, kNumberBuffer> chiffres{};
    std::snprintf(chiffres.data(), chiffres.size(), "%02d", value);
    forme.replace(trou, kValueHole.size(), chiffres.data());
  }
  return forme;
}

[[nodiscard]] ro::sim::CellPosition CellOf(const ro::sim::FractionalCell& cell) {
  return ro::sim::CellPosition{static_cast<std::uint16_t>(cell.x),
                               static_cast<std::uint16_t>(cell.y)};
}

// Vrai quand le moment de cet état lit sa valeur.
[[nodiscard]] bool ReadsValue(ro::sim::StatusEffectId state) {
  return std::ranges::any_of(ro::render::StateNamedEffects(state, ro::render::kStateNamedOnGained),
                             [](const ro::render::StateNamedEffectEntry& entry) {
                               return ro::render::StateNamedEntryReadsValue(entry);
                             });
}

}  // namespace

void EffectsInPlay::NoticeEffectRemovals(const ro::sim::World& world) {
  std::erase_if(per_map_.removals_seen,
                [&world](const auto& vu) { return !world.actors().contains(vu.first); });
  for (const auto& [id, actor] : world.actors()) {
    // UN ACTEUR QU'ON DÉCOUVRE n'a rien qui joue : ses retraits passés ne
    // coupent rien.
    const auto [vu, nouveau] = per_map_.removals_seen.try_emplace(id, actor.removed_effects_total);
    if (nouveau || vu->second == actor.removed_effects_total) {
      continue;
    }
    const std::size_t neufs = std::min<std::size_t>(actor.removed_effects_total - vu->second,
                                                    actor.removed_effects.size());
    vu->second = actor.removed_effects_total;
    for (std::size_t rang = actor.removed_effects.size() - neufs;
         rang < actor.removed_effects.size(); ++rang) {
      StopById(actor.removed_effects[rang], id);
    }
  }
}

glm::vec3 EffectsInPlay::ViewOffsetInWorld(glm::vec2 view_offset) const {
  if (!camera_.has_value() || view_offset == glm::vec2(0.0F)) {
    return glm::vec3(0.0F);
  }
  // L'ORDONNÉE DE LA VUE DESCEND, et l'axe haut de notre panneau monte : d'où
  // le signe.
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(*camera_);
  return basis.right * view_offset.x - basis.up * view_offset.y;
}

void EffectsInPlay::LayOnGround(const EffectMapMemory::Playing& playing, std::int32_t frame,
                                const MapStage& stage) {
  std::vector<ro::render::EffectQuad> quads;
  ro::render::ComposeEffect(playing.effect->prepared, frame, quads);
  ro::render::LaidEffectPose pose;
  pose.center = playing.ground_position;
  pose.turn_degrees = playing.turn_degrees;
  pose.camera_yaw_degrees = camera_.has_value() ? camera_->yaw_degrees : 0.0F;
  pose.scale = playing.scale;
  pose.flip_x = playing.flip_x;
  pose.flip_y = playing.flip_y;
  // LE RELIEF DE LA MARCHE, celui où les acteurs se tiennent.
  const auto relief = [&stage](float world_x, float world_z) {
    const std::array<float, 2> cellule =
        ro::render::FractionalCellAt(stage.scene.gnd, world_x, world_z);
    return GroundPositionAt(stage, cellule[0], cellule[1])[1];
  };
  const ro::render::LoadedEffect& charge = *playing.effect;
  for (const ro::render::EffectQuad& quad : quads) {
    if (quad.texture >= charge.textures.size() || charge.textures[quad.texture].empty()) {
      continue;
    }
    ro::render::LayEffectQuads(std::span(&quad, 1), pose, relief, laid_quads_);
    ro::render::DrawnWorldQuads lot;
    lot.texture_path = &charge.texture_paths[quad.texture];
    lot.texture = &charge.textures[quad.texture];
    lot.ground_position = playing.ground_position;
    laid_lots_.push_back(lot);
  }
}

void EffectsInPlay::BindLaidLots() {
  // ⚠ APRÈS que le tableau a fini de grandir : des vues prises au fil des
  // ajouts pendraient à la première réallocation.
  for (std::size_t rang = 0; rang < laid_lots_.size() && rang < laid_quads_.size(); ++rang) {
    laid_lots_[rang].quads = std::span(laid_quads_).subspan(rang, 1);
  }
}

void EffectsInPlay::PlayChargeCount(std::uint32_t actor_id, std::int32_t count,
                                    std::uint32_t now_ms) {
  // AU-DELÀ DU PLUS GRAND COMPTE, rien ne change — pas même un effacement.
  if (count > ro::render::kChargeCountMost) {
    return;
  }
  const auto is_charge = [actor_id](const Playing& shown) {
    return shown.from_effect_id == ro::render::kChargeCountEffectId && shown.actor_id == actor_id;
  };
  auto already = static_cast<std::int32_t>(std::ranges::count_if(per_map_.playing, is_charge));
  // UN COMPTE QUI DESCEND efface tout et rebâtit depuis zéro.
  if (already > count) {
    std::erase_if(per_map_.playing, is_charge);
    already = 0;
  }
  const ro::render::LoadedEffect* effect = catalog_->Load(
      std::string(ro::render::kEffectFolder) + std::string(ro::render::kChargeCountFile));
  if (effect == nullptr) {
    return;
  }
  for (std::int32_t rang = std::max(already, 0); rang < count; ++rang) {
    Playing charge{effect, actor_id, glm::vec3(0.0F), now_ms};
    charge.from_effect_id = ro::render::kChargeCountEffectId;
    charge.view_offset = ro::render::kChargeCountViewOffsets[static_cast<std::size_t>(rang)];
    charge.scale = ro::render::kChargeCountScale;
    charge.holds_last_frame = true;
    // Posée au point même : aucun relèvement d'ancre.
    charge.lift_pixels = 0.0F;
    per_map_.playing.push_back(charge);
  }
}

void EffectsInPlay::PlayStateNamedValues(const ro::sim::World& world, const MapStage& stage,
                                         ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  (void)stage;
  // Un état levé, ou un acteur parti, oublie ses valeurs : l'état qui revient
  // rejoue son moment.
  std::erase_if(per_map_.named_state_values, [&world](const auto& vu) {
    const ro::sim::Actor* actor = world.Find(vu.first.first);
    return actor == nullptr || !actor->status_effects.contains(vu.first.second);
  });
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::StatusEffectId state : actor.status_effects) {
      if (!ReadsValue(state)) {
        continue;
      }
      const auto current_value = actor.status_values.find(state);
      if (current_value == actor.status_values.end()) {
        continue;
      }
      const auto second = actor.status_second_values.find(state);
      const std::pair<std::int32_t, std::int32_t> state_values{
          current_value->second, second == actor.status_second_values.end() ? 0 : second->second};
      const auto key = std::pair{id, state};
      if (const auto vu = per_map_.named_state_values.find(key);
          vu != per_map_.named_state_values.end() && vu->second == state_values) {
        continue;
      }
      per_map_.named_state_values.insert_or_assign(key, state_values);
      const ro::sim::CellPosition placed =
          CellOf(ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms));
      // ⚠ LA SECONDE VALEUR DÉSIGNE UN ACTEUR, ou personne : les deux bras
      // s'excluent.
      const ro::sim::Actor* designe =
          state_values.second != 0 ? world.Find(static_cast<std::uint32_t>(state_values.second))
                                   : nullptr;
      for (const ro::render::StateNamedEffectEntry& entry :
           ro::render::StateNamedEffects(state, ro::render::kStateNamedOnGained)) {
        // Un effet par NUMÉRO ne lit pas la valeur : il est parti à la pose.
        if (entry.effect_id != 0) {
          continue;
        }
        if (entry.only_at_value >= 0 && state_values.first != entry.only_at_value) {
          continue;
        }
        if (entry.counts_charges) {
          if (state_values.second == 0) {
            PlayChargeCount(id, state_values.first, now_ms);
          }
          continue;
        }
        if (entry.on_designated_actor) {
          if (designe == nullptr) {
            continue;
          }
          const ro::sim::CellPosition sa_case =
              CellOf(ro::sim::InterpolatedCell(*designe, planner.PathFor(*designe), now_ms));
          PlayNamedEntry(entry.name, entry.verb, designe->id, false, sa_case, entry.anchor, false,
                         now_ms);
          // LE BRUIT PART AVEC CET EFFET-LÀ, sur celui que la valeur désigne.
          PlayNamedMethodSounds(NamedMethodFamily::kState, state, ro::render::kStateNamedOnGained,
                                designe->id, sa_case, sa_case, per_map_.just_sown_named_sounds);
          continue;
        }
        if (entry.name_takes_value) {
          // ⚠ LE NOM FORMÉ NE VIT QUE LE TEMPS DE CET APPEL : c'est celui de la
          // fiche qui se garde.
          const ro::formats::NamedEffect* record =
              NamedEffectFor(WithValue(entry.name, state_values.first));
          if (record != nullptr) {
            PlayNamedEntry(record->name, entry.verb, id, false, placed, entry.anchor, false,
                           now_ms);
          }
          continue;
        }
        PlayNamedEntry(entry.name, entry.verb, id, false, placed, entry.anchor, false, now_ms);
      }
    }
  }
}

}  // namespace moonclient
