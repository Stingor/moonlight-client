// L'espèce d'une étiquette, et la couleur de son nom. Voir l'en-tête : ces deux
// règles vivent à part pour pouvoir se vérifier hors du jeu.

#include "world/name_plate_view.h"

#include "ro/render/name_plate.h"
#include "ro/sim/world.h"

namespace moonclient {

bool HasNamePlate(const ro::sim::Actor& actor) {
  return !(actor.kind == ro::sim::ActorKind::kNonPlayer && actor.look.job == ro::sim::kPortalJob);
}

bool NameCutAtHash(const ro::sim::Actor& actor) {
  return actor.kind != ro::sim::ActorKind::kPlayer;
}

ro::render::NamePlateKind NamePlateKindOf(const ro::sim::Actor& actor,
                                          const ro::sim::World& world) {
  // ⚠ MOI D'ABORD : sans quoi ma propre étiquette hériterait des réglages faits
  // pour les autres joueurs. Même ordre que `EntityBarKindOf`.
  if (actor.id == world.player_id()) {
    return ro::render::NamePlateKind::kOwn;
  }
  switch (actor.kind) {
    case ro::sim::ActorKind::kMonster:
      return actor.boss != ro::sim::BossRank::kNone ? ro::render::NamePlateKind::kBoss
                                                    : ro::render::NamePlateKind::kMonster;
    case ro::sim::ActorKind::kNonPlayer:
      return ro::render::NamePlateKind::kNonPlayer;
    case ro::sim::ActorKind::kItem:
      return ro::render::NamePlateKind::kGroundItem;
    case ro::sim::ActorKind::kPlayer:
    case ro::sim::ActorKind::kSkillUnit:
    case ro::sim::ActorKind::kUnknown:
      break;
  }
  // ⚠ UNE UNITÉ DE COMPÉTENCE N'A PAS DE NOM et ne parvient pas jusqu'ici ; ce
  // qui reste se règle comme un joueur, faute d'une espèce qui lui soit propre.
  return ro::render::NamePlateKind::kPlayer;
}

std::array<std::uint8_t, 4> NameColorOf(const ro::sim::Actor& actor,
                                        const ro::render::EntityNameLook& how,
                                        const ro::sim::World& world) {
  const std::array<std::uint8_t, 4> chosen = ro::render::UnpackedColor(how.color);
  // 🔴 SUR UN MONSTRE, ET SUR RIEN D'AUTRE. Le panneau ne propose plus ce
  // réglage ailleurs, mais le FICHIER de réglages, lui, s'ouvre dans un éditeur
  // de texte : sans cette garde, un drapeau posé à la main sur les joueurs
  // peindrait en rouge tout ce qui nous dépasse de dix niveaux. C'est aussi la
  // garde du client, qui vérifie « ce n'est pas un métier de joueur » avant
  // d'écraser sa couleur.
  if (!how.level_gap || actor.kind != ro::sim::ActorKind::kMonster) {
    return chosen;
  }
  // L'ÉCART DE NIVEAU PASSE DEVANT LA COULEUR D'ESPÈCE, et c'est l'ordre du
  // client : il peint d'abord par type, puis écrase. ⚠ ET LE NIVEAU DE L'AUTRE
  // N'EST CONNU QUE SI LE PAQUET D'APPARITION LE PORTE — zéro veut dire « on ne
  // sait pas », et la couleur d'espèce reste.
  const std::int64_t mine = world.player_status().base_level;
  return ro::render::NameColorForLevelGap(chosen, static_cast<int>(mine), actor.level);
}

}  // namespace moonclient
