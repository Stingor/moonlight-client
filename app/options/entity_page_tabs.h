#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include "ro/core/i18n.h"
#include "ro/render/entity_names.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/entity_sample.h"

namespace moonclient {

// ── LES ONGLETS DE LA PAGE « ENTITÉS » ──────────────────────────────────────
//
// Les barres et les noms se règlent ensemble, parce qu'ils se posent au même
// endroit — sous les pieds, l'un poussant l'autre — et qu'on les regarde sur la
// même entité.
//
// 🔴 LEURS ESPÈCES NE SONT PAS LES MÊMES, ET CE N'EST PAS UN OUBLI : chacune
// suit ce que le PROTOCOLE dit. Le groupe a sa barre — sa vie arrive par un
// paquet à lui — mais pas de nom à part : sa carte de nom est celle de tout
// joueur. Un objet au sol a son nom, et pas de vie. D'où l'union : un onglet
// par espèce de l'une ou de l'autre, et ce qu'il ne porte pas se dit en note.
//
// ⚠ L'ORDRE EST CELUI DES ONGLETS À L'ÉCRAN, et il n'est pas persisté : le
// fichier de réglages range par espèce de barre et de nom, pas par onglet.
//
// ⚠ LE DERNIER N'EST NI UNE BARRE NI UN NOM : les enseignes des salons et des
// échoppes se posent elles aussi au-dessus des têtes, mais n'ont pas d'espèce.
// Voir `SignboardOptions`.
enum class EntityPageTab : std::size_t {
  kOwn = 0,
  kParty,
  kPlayer,
  kMonster,
  kBoss,
  kOther,
  kGroundItem,
  kSignboards,
  kCount,
};
inline constexpr std::size_t kEntityPageTabCount = static_cast<std::size_t>(EntityPageTab::kCount);

// L'onglet d'un rang lu de la fenêtre, borné.
[[nodiscard]] inline EntityPageTab EntityPageTabOf(int rank) {
  return static_cast<EntityPageTab>(std::clamp(rank, 0, static_cast<int>(kEntityPageTabCount) - 1));
}

// CE QUE L'ONGLET RÈGLE : l'espèce de barre, l'espèce de nom. ⚠ ET CE QUE SON
// CADRE MONTRE, qui peut aller plus loin : le groupe n'a pas de réglage de nom,
// mais son nom s'écrit comme celui des joueurs — voir `EntityPageDrawn`.
[[nodiscard]] inline ro::ui::EntitySampleShown EntityPageSettles(EntityPageTab tab) {
  using ro::render::NamePlateKind;
  using ro::ui::EntityBarKind;
  switch (tab) {
    case EntityPageTab::kOwn:
      return {.bars = EntityBarKind::kOwn, .name = NamePlateKind::kOwn};
    case EntityPageTab::kParty:
      return {.bars = EntityBarKind::kParty, .name = std::nullopt};
    case EntityPageTab::kPlayer:
      return {.bars = EntityBarKind::kPlayer, .name = NamePlateKind::kPlayer};
    case EntityPageTab::kMonster:
      return {.bars = EntityBarKind::kMonster, .name = NamePlateKind::kMonster};
    case EntityPageTab::kBoss:
      return {.bars = EntityBarKind::kBoss, .name = NamePlateKind::kBoss};
    case EntityPageTab::kOther:
      return {.bars = EntityBarKind::kOther, .name = NamePlateKind::kNonPlayer};
    case EntityPageTab::kSignboards:
      return {.bars = std::nullopt, .name = std::nullopt};
    case EntityPageTab::kGroundItem:
    case EntityPageTab::kCount:
      break;
  }
  return {.bars = std::nullopt, .name = NamePlateKind::kGroundItem};
}

[[nodiscard]] inline ro::ui::EntitySampleShown EntityPageDrawn(EntityPageTab tab) {
  ro::ui::EntitySampleShown drawn = EntityPageSettles(tab);
  if (tab == EntityPageTab::kParty) {
    drawn.name = ro::render::NamePlateKind::kPlayer;
  }
  return drawn;
}

// LE NOM DE L'ONGLET. ⚠ DÉJÀ TRADUIT.
[[nodiscard]] inline const char* EntityPageTabName(EntityPageTab tab) {
  if (tab == EntityPageTab::kSignboards) {
    return ro::core::Tr("Enseignes");
  }
  if (tab == EntityPageTab::kGroundItem) {
    return ro::render::EntityNameKindName(ro::render::NamePlateKind::kGroundItem);
  }
  // « PNJ » ET NON « AUTRES » : c'est ce que l'onglet porte d'abord, et le nom
  // qu'il a côté étiquettes. L'infobulle de ses barres dit le reste —
  // familiers, homoncules, mercenaires.
  if (tab == EntityPageTab::kOther) {
    return ro::render::EntityNameKindName(ro::render::NamePlateKind::kNonPlayer);
  }
  return ro::ui::EntityBarKindName(*EntityPageSettles(tab).bars);
}

}  // namespace moonclient
