#include "world/skill_area_view.h"

#include "ro/core/log.h"
#include "ro/render/ground_quad.h"
#include "ro/sim/world.h"

namespace moonclient {

const ro::formats::Image* SkillAreas::TextureFor(std::string_view path) {
  if (path.empty()) {
    return nullptr;
  }
  if (const auto found = textures_.find(path); found != textures_.end()) {
    return found->second.has_value() ? &*found->second : nullptr;
  }
  auto& slot = textures_[std::string(path)];
  // 🔴 LE CARREAU PLEIN SE BÂTIT, IL NE SE LIT PAS. C'est un aplat, et aucun
  // fichier du jeu n'est un carré blanc opaque ; aller le chercher rendrait un
  // motif éteint au lieu d'un motif plein, sans que rien ne s'en plaigne.
  //
  // ⚠ ET IL SE DESSINE, il n'est pas un aplat : voir `ComposeSkillAreaTile`. La
  // teinte du quad donne la couleur, l'image ne porte que la FORME.
  if (ro::render::SkillAreaIsPainted(path)) {
    slot = ro::render::ComposeSkillAreaTile();
    return &*slot;
  }
  auto bytes = vfs_->Read(std::string(path));
  if (!bytes) {
    ro::core::LogWarning("zone des sorts : {} absent — ce motif ne se peindra pas", path);
    return nullptr;
  }
  auto image = ro::formats::DecodeImage(std::string(path), *bytes);
  if (!image) {
    ro::core::LogWarning("zone des sorts : {} illisible — ce motif ne se peindra pas", path);
    return nullptr;
  }
  slot = std::move(*image);
  return &*slot;
}

void SkillAreas::PushArea(const ro::render::SkillAreaLook& look, const ro::sim::SkillTree& skills,
                          const MapStage& stage, std::uint16_t skill_id, std::uint16_t level,
                          ro::sim::CellPosition centre) {
  const ro::sim::SkillNode* node = skills.Find(skill_id);
  if (node == nullptr) {
    return;
  }
  const ro::sim::SkillScaleCells cells = ro::sim::SkillScaleAt(*node, level);
  if (!cells.Declared()) {
    return;
  }
  const float fill = ro::render::SkillAreaFill(look);
  for (const ro::render::EffectGroundCellPaint& paint :
       ro::render::ComposeSkillAreaCells(look, cells.x, cells.y)) {
    std::optional<ro::render::GroundQuad> quad = ro::render::ComposeGroundCellQuad(
        stage.scene.gnd, *stage.gat, static_cast<std::int32_t>(centre.x) + paint.cell_offset_x,
        static_cast<std::int32_t>(centre.y) + paint.cell_offset_y, paint.color, fill);
    if (!quad.has_value()) {
      continue;
    }
    quad->texture_uv = paint.texture_uv;
    quads_.push_back(*quad);
  }
}

std::span<const ro::render::DrawnGroundQuads> SkillAreas::Areas(
    const ro::render::SkillAreaLook& look, const ro::sim::World& world,
    const ro::sim::SkillTree* skills, const MapStage& stage, std::uint32_t now_ms,
    const ro::sim::KnownSkill* armed, std::optional<ro::sim::CellPosition> hovered_cell) {
  quads_.clear();
  lots_.clear();
  if (skills == nullptr || !stage.gat.has_value()) {
    return lots_;
  }
  if (look.cast_shown) {
    for (const auto& [id, actor] : world.actors()) {
      if (!actor.skill_scale.has_value()) {
        continue;
      }
      const ro::sim::SkillScale& scale = *actor.skill_scale;
      // ÉCHUE : le serveur ne dit pas qu'une incantation aboutit, il en a donné
      // la durée. Passé le terme, la zone s'en va d'elle-même.
      //
      // 🔴 MAIS SA DURÉE PEUT ÊTRE NULLE, et la zone ne paraissait alors JAMAIS.
      // `clif_skill_scale` reprend le temps d'incantation calculé pour ce
      // lanceur : réduit à zéro — Quick Cast, un équipement, un état —, il rend
      // un paquet qui annonce une zone sans lui donner d'instant où vivre.
      // L'INCANTATION EN COURS tranche alors, parce que c'est elle que la zone
      // accompagne.
      const std::uint32_t vit_ms =
          scale.duration_ms != 0
              ? scale.duration_ms
              : (actor.cast.has_value() && actor.cast->started_ms == scale.started_ms
                     ? actor.cast->duration_ms
                     : 0);
      if (now_ms < scale.started_ms || now_ms - scale.started_ms >= vit_ms) {
        continue;
      }
      // ⚠ SUR LA CASE VISÉE, et non sur le lanceur. Les deux coïncident pour un
      // sort lancé sur soi, et se séparent dès qu'il vise ailleurs — c'est
      // précisément ce que Moonlight corrige à l'émission.
      PushArea(look, *skills, stage, scale.skill_id, scale.level, scale.centre);
    }
  }
  // LE SORT QU'ON TIENT EN MAIN, sous le curseur. ⚠ SEULEMENT CEUX QUI VISENT
  // UNE CASE : une compétence qui désigne quelqu'un n'attend pas un clic au
  // sol, et suivre le curseur avec sa zone montrerait une aire là où le clic
  // n'ira pas.
  if (look.armed_shown && armed != nullptr && hovered_cell.has_value() &&
      ro::sim::SkillNeedsGround(*armed)) {
    PushArea(look, *skills, stage, armed->id, armed->level, *hovered_cell);
  }
  if (quads_.empty()) {
    return lots_;
  }
  // UN SEUL LOT : toutes les zones partagent le motif, donc l'image. Les
  // séparer multiplierait les appels de dessin sans rien changer à ce qui se
  // voit.
  texture_path_ = std::string(ro::render::SkillAreaTexturePath(look.pattern));
  lots_.push_back(ro::render::DrawnGroundQuads{quads_, &texture_path_, TextureFor(texture_path_)});
  return lots_;
}

}  // namespace moonclient
