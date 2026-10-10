// Les réglages des étiquettes d'entité : leurs défauts, et le filtre qui dit
// laquelle paraît. Voir l'en-tête pour le découpage par espèce.

#include "ro/render/entity_names.h"

#include "ro/core/i18n.h"
#include "ro/render/name_plate.h"
#include "ro/render/screen_text.h"

namespace ro::render {

NamePlateWhen NamePlateWhenOf(int stored) {
  if (stored < 0 || stored >= static_cast<int>(NamePlateWhen::kCount)) {
    return NamePlateWhen::kHovered;
  }
  return static_cast<NamePlateWhen>(stored);
}

std::array<EntityNameLook, kNamePlateKindCount> DefaultEntityNames() {
  std::array<EntityNameLook, kNamePlateKindCount> names;
  for (EntityNameLook& look : names) {
    look.color = PackedColor(kPlayerNameColor);
    look.shadow_color = PackedColor(kNameShadowColor);
  }

  // MOI : ÉTEINTE. Le client n'écrit pas mon nom sous mes pieds — je sais où je
  // suis, et l'étiquette ne ferait que couvrir ce sur quoi je marche.
  EntityNameLook& own = names[static_cast<std::size_t>(NamePlateKind::kOwn)];
  own.shown = false;
  own.color = PackedColor(kOwnNameColor);

  // LES PNJ : EN PERMANENCE. ⚠ CONTRE LE CLIENT, ET DÉLIBÉRÉMENT : un PNJ ne
  // bouge pas, il n'y en a jamais trente à l'écran, et c'est ce qu'on cherche
  // des yeux en arrivant sur une carte de ville.
  EntityNameLook& npc = names[static_cast<std::size_t>(NamePlateKind::kNonPlayer)];
  npc.when = NamePlateWhen::kAlways;

  // LES MONSTRES ET LES BOSS : l'écart de niveau, qui est le leur. Voir
  // `EntityNameLook::level_gap` — ailleurs, il ne ferait rien.
  EntityNameLook& monster = names[static_cast<std::size_t>(NamePlateKind::kMonster)];
  monster.color = PackedColor(kMonsterNameColor);
  monster.level_gap = true;
  EntityNameLook& boss = names[static_cast<std::size_t>(NamePlateKind::kBoss)];
  boss.color = PackedColor(kMonsterNameColor);
  boss.level_gap = true;

  // LES OBJETS AU SOL : EN PERMANENCE, et c'est le seul défaut qui s'écarte
  // franchement du client. Un butin par terre est ce qu'on cherche des yeux
  // après un combat, et il disparaît tout seul au bout d'un moment.
  EntityNameLook& drop = names[static_cast<std::size_t>(NamePlateKind::kGroundItem)];
  drop.when = NamePlateWhen::kAlways;
  // ⚠ ET PAS D'ÉTIQUETTE DE CLAN SUR UNE POTION : ces trois places sont vides
  // sur un objet, sauf l'aside, où nous mettons la QUANTITÉ.
  drop.title = false;
  drop.banner = false;

  return names;
}

bool ShowsEntityName(const EntityNamesLook& look, const EntityNameView& what) {
  if (!look.shown) {
    return false;
  }
  const auto rank = static_cast<std::size_t>(what.kind);
  if (rank >= look.kinds.size()) {
    return false;
  }
  const EntityNameLook& how = look.kinds[rank];
  if (!how.shown) {
    return false;
  }
  switch (how.when) {
    case NamePlateWhen::kAlways:
      return true;
    case NamePlateWhen::kHovered:
      return what.hovered;
    case NamePlateWhen::kTarget:
      return what.targeted;
    case NamePlateWhen::kHoveredOrTarget:
      return what.hovered || what.targeted;
    case NamePlateWhen::kCount:
      break;
  }
  return false;
}

const char* EntityNameKindName(NamePlateKind kind) {
  // ⚠ UN AIGUILLAGE DE LITTÉRAUX, et non une table : `Tr` veut voir sa chaîne à
  // l'appel, et le pointeur qu'il rend meurt au changement de langue.
  switch (kind) {
    case NamePlateKind::kOwn:
      return core::Tr("Moi");
    case NamePlateKind::kPlayer:
      return core::Tr("Joueurs");
    case NamePlateKind::kNonPlayer:
      return core::Tr("PNJ");
    case NamePlateKind::kMonster:
      return core::Tr("Monstres");
    case NamePlateKind::kBoss:
      return core::Tr("Boss");
    case NamePlateKind::kGroundItem:
      return core::Tr("Items au sol");
  }
  return "";
}

}  // namespace ro::render
