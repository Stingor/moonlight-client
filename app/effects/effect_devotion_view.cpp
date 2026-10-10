// LES TRAITS DE DEVOTION : un par protégé, tendus depuis le croisé.
//
// Le serveur annonce la liste ENTIÈRE des protégés d'un acteur — voir
// `ro::sim::DevotionList`. À chaque annonce, le client défait d'abord les
// traits que l'acteur porte, puis en tend un par case pleine : l'effet 232, un
// tube à dix faces qui pousse du croisé vers son protégé en quinze tics, puis
// suit les deux corps, huit unités au-dessus de chacun. La figure et son
// mouvement vivent dans `ro/render/effect_tethered_beam.h`.
//
// ⚠ `/effect` COUPÉ, L'ANNONCE EST PERDUE : le client ne défait rien, ne tend
// rien, et rallumer ne la rejoue pas.
//
// ⚠ UN PROTÉGÉ HORS DE VUE N'A PAS DE TRAIT, et ne le retrouve pas en
// revenant : le trait s'éteint dès qu'il ne trouve plus sa cible, et seule une
// annonce nouvelle le retend — le serveur en envoie une quand le protégé
// rentre en vue. La PORTÉE de la compétence, elle, ne coupe rien : le client ne
// la lit pas.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_spawn_request.h"
#include "ro/sim/devotion.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

namespace moonclient {
namespace {

// COMBIEN D'INSTANCES DU TRAIT UNE ANNONCE DÉFAIT, au plus, avant de retendre :
// autant que la liste a de cases, les plus anciennes d'abord.
constexpr std::size_t kLinksUndonePerList = ro::sim::kDevotionSlots;

}  // namespace

void EffectsInPlay::NoticeDevotionLinks(const ro::sim::World& world, const MapStage& stage,
                                        ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  // UN ACTEUR PARTI S'OUBLIE : ses traits sont partis avec lui, et celui qui
  // reprendrait son identifiant n'en a tendu aucun.
  std::erase_if(per_map_.devotion_lists_seen,
                [&world](const auto& seen) { return !world.actors().contains(seen.first); });
  const auto ground_of = [&stage, &planner, now_ms](const ro::sim::Actor& actor) {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    return glm::vec3(ground[0], ground[1], ground[2]);
  };
  for (const auto& [devoter_id, devoter] : world.actors()) {
    const std::uint32_t told = devoter.devotion.told_serial;
    const auto seen = per_map_.devotion_lists_seen.find(devoter_id);
    // 🔴 UN ACTEUR QU'ON DÉCOUVRE AVEC UNE LISTE LA TEND : le serveur l'annonce
    // dans le même souffle que l'entrée en vue du croisé.
    const std::uint32_t already = seen != per_map_.devotion_lists_seen.end() ? seen->second : 0U;
    if (told == already) {
      continue;
    }
    per_map_.devotion_lists_seen.insert_or_assign(devoter_id, told);
    if (!EffectsShown()) {
      continue;
    }
    nodes_->StopOldestNodes(kDevotionLinkEffect, devoter_id, kLinksUndonePerList);
    const glm::vec3 devoter_ground = ground_of(devoter);
    for (const std::uint32_t devotee_id : devoter.devotion.devotee_ids) {
      if (devotee_id == 0) {
        continue;
      }
      // ⚠ L'IDENTIFIANT DU PROTÉGÉ VOYAGE EN FLOTTANT jusqu'au trait, et c'est
      // sous cette forme que le trait le cherche.
      const std::uint32_t target_id =
          ro::render::DesignatedActorAfterStorage(devotee_id, world.player_id());
      const ro::sim::Actor* devotee = world.Find(target_id);
      // UNE CIBLE INTROUVABLE TEND QUAND MÊME L'INSTANCE, éteinte dès sa
      // naissance : c'est la cible, pas l'annonce, qui manque.
      const std::optional<glm::vec3> target =
          devotee != nullptr ? std::optional<glm::vec3>(ground_of(*devotee)) : std::nullopt;
      static_cast<void>(nodes_->PlayProcedural(kDevotionLinkEffect, devoter_ground, target, 0.0F,
                                               devoter_id, kRawEffectParam, 0, 0, 0.0F, target_id));
    }
  }
}

}  // namespace moonclient
