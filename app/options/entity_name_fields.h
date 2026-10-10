#pragma once

#include "ro/render/entity_names.h"

#include "options/game_option_ids.h"

namespace moonclient {

// ── CE QUI EXISTE SUR CETTE ESPÈCE-LÀ, ET CE QUI N'Y EXISTE PAS ─────────────
//
// 🔴 DEUX FAÇONS DE NE PAS S'APPLIQUER, ET ELLES NE SE MONTRENT PAS PAREIL.
// « Pas maintenant » — le maître est éteint, l'ombre est éteinte — se GRISE :
// la rangée reste à sa place, et le joueur voit ce qu'il retrouvera en
// rallumant. « Jamais ici » ne se grise pas, il DISPARAÎT : une rangée qui ne
// pourra rien faire sur cet onglet, quoi qu'on règle, n'apprend rien et ment —
// un insigne de boss proposé sous l'onglet des objets au sol laisse croire
// qu'un butin pourrait en porter un.
//
// ⚠ ET CE N'EST PAS UN GOÛT DE MISE EN PAGE : ce que ce tableau retire, c'est
// ce que le PROTOCOLE ne transporte pas.
//
//   · l'INSIGNE vient du rang du paquet d'apparition, et seul un monstre de
//     rang boss en porte un ;
//   · l'ÉCART DE NIVEAU est gardé chez le client par un « ce n'est pas un
//     métier de joueur », et nous ne connaissons le niveau que de ce que le
//     paquet d'apparition porte — voir `NameColorOf`, qui pose la même garde ;
//   · un PNJ répond par `ZC_ACK_REQNAMEALL_NPC`, qui ne porte QUE son nom et
//     son titre. Ses trois autres chaînes n'existent pas dans le paquet, et le
//     client les passe VIDES à son poseur ;
//   · un OBJET AU SOL ne répond à rien du tout — `clif_name` n'a aucun cas pour
//     `BL_ITEM` — et c'est NOUS qui composons son nom. Il n'a donc ni titre ni
//     seconde ligne, et sa parenthèse est la quantité que nous y mettons.
//
// ⚠ UN MONSTRE GARDE SES TROIS CHAÎNES, ET CE N'EST PAS UNE INDULGENCE : sous
// `show_mob_info`, ce serveur-ci lui écrit « Lv. X | HP: Y% » en parenthèse, sa
// RACE en seconde ligne et son ÉLÉMENT entre crochets ; sans ce réglage, il
// répond par le paquet des PNJ et porte alors le titre d'un `setunittitle`. Les
// trois dépendent d'une configuration du serveur, donc aucune ne se retire.
[[nodiscard]] constexpr bool EntityNameFieldApplies(ro::render::NamePlateKind kind,
                                                    EntityNameField field) {
  using ro::render::NamePlateKind;
  switch (field) {
    case EntityNameField::kBossBadge:
      return kind == NamePlateKind::kBoss;
    case EntityNameField::kLevelGap:
      return kind == NamePlateKind::kMonster || kind == NamePlateKind::kBoss;
    case EntityNameField::kTitle:
      return kind != NamePlateKind::kGroundItem;
    case EntityNameField::kAside:
      return kind != NamePlateKind::kNonPlayer;
    case EntityNameField::kBanner:
      return kind != NamePlateKind::kNonPlayer && kind != NamePlateKind::kGroundItem;
    // Le reste vaut partout : une étiquette a toujours une couleur, une place,
    // et un moment où elle paraît.
    case EntityNameField::kShown:
    case EntityNameField::kWhen:
    case EntityNameField::kColor:
    case EntityNameField::kShadow:
    case EntityNameField::kShadowColor:
    case EntityNameField::kOffset:
    case EntityNameField::kOpacity:
      return true;
    case EntityNameField::kCount:
      break;
  }
  // 🔴 PAS DE `default` : un treizième réglage ajouté à l'énumération doit faire
  // échouer la compilation ICI, et non hériter en silence d'un « il s'applique
  // partout » que personne n'a décidé.
  return false;
}

}  // namespace moonclient
