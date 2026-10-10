#include "world/armed_cursor.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/sim/skill_tree.h"

namespace moonclient {

ro::render::CursorShape ArmedCursor(SkillAim aim, const ro::sim::Actor* hovered,
                                    ro::render::CursorShape otherwise) {
  if (aim == SkillAim::kNothing) {
    return otherwise;
  }
  // 🔴 LE CIBLAGE AU SOL NE REGARDE PAS CE QU'IL SURVOLE. Le client sort de son
  // routeur de survol dès l'entrée dans ce mode-là, si bien qu'aucune branche
  // par catégorie ne s'exécute : l'anneau couvre tout, les PNJ et les objets au
  // sol compris.
  if (aim == SkillAim::kGround) {
    return ro::render::CursorShape::kGroundTarget;
  }
  // ⚠ LA VISÉE D'UN PIÈGE GARDE L'ÉPÉE SUR CE QUI SE FRAPPE : le client ne
  // consulte son mode de ciblage, sur un monstre ou un joueur, que pour les
  // visées d'une cible et d'un soutien. Partout ailleurs — le sol, une unité au
  // sol —, l'anneau lent.
  if (aim == SkillAim::kTrap) {
    switch (otherwise) {
      case ro::render::CursorShape::kPickUp:
      case ro::render::CursorShape::kTalk:
      case ro::render::CursorShape::kWarp:
      case ro::render::CursorShape::kPoint:
      case ro::render::CursorShape::kAttack:
        return otherwise;
      case ro::render::CursorShape::kArrow:
      case ro::render::CursorShape::kForbidden:
      case ro::render::CursorShape::kGroundTarget:
      case ro::render::CursorShape::kTargetLocked:
      case ro::render::CursorShape::kRotate:
        break;
    }
    return ro::render::CursorShape::kGroundTarget;
  }
  switch (otherwise) {
    // ⚠ CELLES-LÀ IGNORENT L'ARMEMENT, au client comme ici : leurs branches
    // posent leur forme sans jamais lire le mode de ciblage. Un clic sur un PNJ
    // lui parle même une compétence armée, et la forme doit le dire.
    case ro::render::CursorShape::kPickUp:
    case ro::render::CursorShape::kTalk:
    case ro::render::CursorShape::kWarp:
    case ro::render::CursorShape::kPoint:
      return otherwise;
    // Ce qui se frappe est ce que la compétence atteindra : l'anneau vif.
    case ro::render::CursorShape::kAttack:
      return ro::render::CursorShape::kTargetLocked;
    case ro::render::CursorShape::kArrow:
    case ro::render::CursorShape::kForbidden:
    case ro::render::CursorShape::kGroundTarget:
    case ro::render::CursorShape::kTargetLocked:
    // La rotation cède au ciblage.
    case ro::render::CursorShape::kRotate:
      break;
  }
  // ⚠ UN MORT GARDE LA FLÈCHE, et c'est la seule exception. Le client teste
  // l'état de l'acteur avant de poser l'anneau lent : un cadavre n'est pas une
  // cible, et l'anneau y promettrait un lancement. Le sol interdit, lui, PERD
  // son panneau — l'armement passe devant, comme au client où le mode de
  // ciblage est consulté avant la case.
  const bool dead = hovered != nullptr && hovered->posture == ro::sim::Posture::kDead;
  return dead ? ro::render::CursorShape::kArrow : ro::render::CursorShape::kGroundTarget;
}

std::uint16_t ArmedLevelAfterWheel(std::uint16_t armed_level, std::uint16_t learned_level,
                                   bool level_matters, float wheel_steps) {
  if (!level_matters || learned_level == 0) {
    return armed_level;
  }
  // ⚠ LE SIGNE, ET RIEN QUE LUI : le client mesure la molette au signe du cumul
  // de l'image, et monte d'un seul rang quel qu'en soit le nombre de crans.
  const int step = wheel_steps > 0.0F ? 1 : (wheel_steps < 0.0F ? -1 : 0);
  if (step == 0) {
    return armed_level;
  }
  // Le plafond AVANT le plancher, comme le client : un rang armé qui dépasserait
  // déjà l'appris redescend, et le plancher rattrape ce que la soustraction a
  // fait passer sous un.
  int wanted = static_cast<int>(armed_level) + step;
  if (wanted > static_cast<int>(learned_level)) {
    wanted = static_cast<int>(learned_level);
  }
  if (wanted < 1) {
    wanted = 1;
  }
  return static_cast<std::uint16_t>(wanted);
}

std::string ArmedBannerText(const ro::sim::SkillTree* skills, std::uint16_t skill_id,
                            std::uint16_t level, bool with_level) {
  const ro::sim::SkillNode* node = skills != nullptr ? skills->Find(skill_id) : nullptr;
  std::string text = node != nullptr && !node->display_name.empty()
                         ? node->display_name
                         : "#" + std::to_string(skill_id);
  if (with_level) {
    text += " ";
    text += ro::core::Tr("Nv");
    text += std::to_string(level);
  }
  return text;
}

}  // namespace moonclient
