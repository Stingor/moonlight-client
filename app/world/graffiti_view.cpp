#include "world/graffiti_view.h"

#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/formats/image.h"
#include "ro/render/graffiti.h"
#include "ro/render/ground_quad.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/hit_popup_rules.h"

namespace moonclient {

const ro::formats::Image* GraffitiOnGround::Alphabet() {
  if (!alphabet_read_) {
    alphabet_read_ = true;
    const std::string path(ro::render::kGraffitiAlphabetPath);
    if (auto bytes = vfs_->Read(path); bytes) {
      if (auto image = ro::formats::DecodeImage(path, *bytes); image) {
        // LE MAGENTA EST LA TRANSPARENCE DU JEU : le fond de la planche
        // disparaît, et ne reste que le trait des lettres.
        ro::formats::ApplyEffectTextureColorKey(*image);
        alphabet_ = std::move(*image);
      }
    }
    if (!alphabet_.has_value()) {
      ro::core::LogWarning("graffiti : {} illisible — aucun message ne s'écrira au sol", path);
    }
  }
  return alphabet_.has_value() ? &*alphabet_ : nullptr;
}

void GraffitiOnGround::Paint(const MapStage& stage, ro::sim::CellPosition centre,
                             std::span<const ro::render::EffectGroundCellPaint> cells,
                             const std::string* texture_path, const ro::formats::Image* texture) {
  const std::size_t first = quads_.size();
  for (const ro::render::EffectGroundCellPaint& paint : cells) {
    std::optional<ro::render::GroundQuad> quad = ro::render::ComposeGroundCellQuad(
        stage.scene.gnd, *stage.gat, static_cast<std::int32_t>(centre.x) + paint.cell_offset_x,
        static_cast<std::int32_t>(centre.y) + paint.cell_offset_y, paint.color);
    if (!quad.has_value()) {
      continue;
    }
    quad->texture_uv = paint.texture_uv;
    quads_.push_back(*quad);
  }
  pending_.push_back(Pending{first, texture_path, texture});
}

std::span<const ro::render::DrawnGroundQuads> GraffitiOnGround::Compose(const ro::sim::World& world,
                                                                        const MapStage& stage,
                                                                        EmblemPlates* emblems,
                                                                        std::uint32_t now_ms) {
  quads_.clear();
  pending_.clear();
  lots_.clear();
  // UNE UNITÉ PARTIE emporte son message.
  std::erase_if(scribbles_, [&world](const auto& entry) {
    const auto unit = world.skill_units().find(entry.first);
    return unit == world.skill_units().end() || unit->second.scribbled != entry.second.message ||
           unit->second.cell != entry.second.cell;
  });
  if (!stage.gat.has_value()) {
    return lots_;
  }
  for (const auto& [unit_id, unit] : world.skill_units()) {
    if (unit.scribbled.empty() || scribbles_.contains(unit_id)) {
      continue;
    }
    const ro::formats::Image* alphabet = Alphabet();
    if (alphabet == nullptr) {
      break;
    }
    // LE REGARD DU JOUEUR, PRIS À LA PARUTION : c'est lui qui oriente la toile.
    // Sans personnage à regarder, le message garde l'angle nul.
    float body_degrees = 0.0F;
    if (const ro::sim::Actor* player = world.player(); player != nullptr) {
      body_degrees = ClientBodyDegrees(*player, ro::sim::FacingAt(*player, now_ms));
    }
    Scribble scribble;
    scribble.message = unit.scribbled;
    scribble.cell = unit.cell;
    scribble.canvas = ro::render::ComposeGraffitiCanvas(*alphabet, unit.scribbled);
    scribble.texture_path = "graffiti:" + std::to_string(unit_id) + ":" +
                            std::to_string(body_degrees) + ":" + unit.scribbled;
    scribble.body_degrees = body_degrees;
    scribbles_.emplace(unit_id, std::move(scribble));
  }
  for (const auto& [unit_id, scribble] : scribbles_) {
    Paint(stage, scribble.cell, ro::render::ComposeGraffitiCells(scribble.body_degrees),
          &scribble.texture_path, &scribble.canvas);
  }
  // LES EMBLÈMES DES FLAG GRAFFITI. Un emblème encore en route ne peint rien :
  // il paraîtra à l'image où il arrive.
  for (const auto& [unit_id, unit] : world.skill_units()) {
    if (unit.unit_id != ro::sim::kFlagGraffitiUnit || unit.emblem_guild_id == 0 ||
        emblems == nullptr) {
      continue;
    }
    const ro::formats::Image* emblem = emblems->Demand(unit.emblem_guild_id, unit.emblem_version);
    if (emblem == nullptr) {
      continue;
    }
    const std::string& path = emblem_paths_
                                  .try_emplace({unit.emblem_guild_id, unit.emblem_version},
                                               "emblem:" + std::to_string(unit.emblem_guild_id) +
                                                   ":" + std::to_string(unit.emblem_version))
                                  .first->second;
    Paint(stage, unit.cell, ro::render::ComposeFlagGraffitiCells(unit.skill_level), &path, emblem);
  }
  // LES LOTS, une fois le tampon fini : leurs vues se prennent maintenant, sans
  // quoi une réallocation les laisserait pendre.
  for (std::size_t rank = 0; rank < pending_.size(); ++rank) {
    const std::size_t end = rank + 1 < pending_.size() ? pending_[rank + 1].first : quads_.size();
    lots_.push_back(ro::render::DrawnGroundQuads{
        std::span(quads_).subspan(pending_[rank].first, end - pending_[rank].first),
        pending_[rank].texture_path, pending_[rank].texture});
  }
  return lots_;
}

}  // namespace moonclient
