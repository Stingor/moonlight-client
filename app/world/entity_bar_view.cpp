// Ce qui se peint au-dessus des entités. Voir l'en-tête pour la frontière avec
// `ui`, et pour ce que le protocole permet de savoir de qui.

#include "world/entity_bar_view.h"

#include <cstddef>
#include <optional>

#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// La jauge d'un couple de nombres venus de l'acteur. ⚠ LA SENTINELLE DU SERVEUR
// VAUT -1, et un seul des deux suffit à taire la jauge : une fraction dont un
// bout est faux est pire qu'une absence.
[[nodiscard]] ro::ui::EntityGauge Told(std::int32_t current, std::int32_t most) {
  ro::ui::EntityGauge gauge;
  if (current < 0 || most <= 0) {
    return gauge;
  }
  gauge.known = true;
  gauge.current = current;
  gauge.most = most;
  return gauge;
}

// La jauge d'un couple FACULTATIF, celui du dossier de cible. Les deux champs
// arrivent sous le même bit du masque : l'un sans l'autre ne se produit pas, et
// la garde le dit quand même.
[[nodiscard]] ro::ui::EntityGauge ToldOptional(const std::optional<std::uint32_t>& current,
                                               const std::optional<std::uint32_t>& most) {
  if (!current.has_value() || !most.has_value() || *most == 0) {
    return {};
  }
  return Told(static_cast<std::int32_t>(*current), static_cast<std::int32_t>(*most));
}

}  // namespace

ro::ui::EntityBarKind EntityBarKindOf(const ro::sim::Actor& actor, const ro::sim::World& world,
                                      const ro::sim::PartyRoster& party) {
  if (actor.id == world.player_id()) {
    return ro::ui::EntityBarKind::kOwn;
  }
  switch (actor.kind) {
    case ro::sim::ActorKind::kMonster:
      // ⚠ LE RANG VIENT DU PAQUET D'APPARITION, donc il vaut pour TOUT monstre
      // en vue — et non seulement pour celui qu'on a désigné. Mini-boss et MVP
      // partagent la même barre : ce réglage-là ne distingue que « on le
      // regarde longtemps » de « c'est un mouton ».
      return actor.boss != ro::sim::BossRank::kNone ? ro::ui::EntityBarKind::kBoss
                                                    : ro::ui::EntityBarKind::kMonster;
    case ro::sim::ActorKind::kPlayer:
      // UN COÉQUIPIER DU CHAMP DE BATAILLE se règle comme le groupe dès que son
      // camp est dit, avant même que sa vie arrive : sa jauge garde ainsi la
      // même règle d'un bout à l'autre de la partie.
      if (world.battlefield().IsAlly(actor.id)) {
        return ro::ui::EntityBarKind::kParty;
      }
      // ⚠ L'IDENTIFIANT D'UN JOUEUR EN VUE EST SON NUMÉRO DE COMPTE, celui que
      // la liste du groupe porte.
      return party.Find(actor.id) != nullptr ? ro::ui::EntityBarKind::kParty
                                             : ro::ui::EntityBarKind::kPlayer;
    default:
      return ro::ui::EntityBarKind::kOther;
  }
}

bool HidesEntityBarsOf(const ro::sim::Actor& actor, const ro::sim::World& world,
                       const ro::ui::EntityBarsLook& look) {
  if (!look.hide_vendors || actor.id == world.player_id() || !actor.signboard.has_value()) {
    return false;
  }
  // ⚠ UN SALON N'EST PAS UNE ÉCHOPPE : son hôte peut se lever et combattre
  // l'instant d'après, et rien ne le distingue alors d'un joueur ordinaire.
  return actor.signboard->kind == ro::sim::SignboardKind::kVendingShop ||
         actor.signboard->kind == ro::sim::SignboardKind::kBuyingStore;
}

namespace {

// LA PLACE QUE LES BARRES DE CET ACTEUR PRENNENT DU CÔTÉ DEMANDÉ.
//
// ⚠ LES DEUX BARRES SONT COMPTÉES COMME SI ELLES ÉTAIENT LÀ : voir l'en-tête,
// qui dit pourquoi cette réponse ignore ce que le serveur a dit.
[[nodiscard]] float BarsOnSide(const ro::sim::Actor& actor, const ro::sim::World& world,
                               const ro::sim::PartyRoster& party,
                               const ro::ui::EntityBarsLook& look, float anchor_scale,
                               bool under_feet) {
  if (!look.shown || HidesEntityBarsOf(actor, world, look)) {
    return 0.0F;
  }
  const auto rank = static_cast<std::size_t>(EntityBarKindOf(actor, world, party));
  if (rank >= look.kinds.size()) {
    return 0.0F;
  }
  const ro::ui::EntityBarLook& how = look.kinds[rank];
  if (!how.shown || how.under_feet != under_feet) {
    return 0.0F;
  }
  ro::ui::EntityBarView both;
  both.health.known = how.health;
  both.magic.known = how.magic;
  both.scale = anchor_scale;
  // ⚠ SEUL L'ÉCART S'ÉCHELONNE : la hauteur de la pile est en pixels, comme le
  // dessin. C'est la méthode du client — voir `ScreenAnchorScale`.
  return how.offset_pixels * anchor_scale + ro::ui::EntityBarsHeight(how, both);
}

}  // namespace

float EntityBarsAbove(const ro::sim::Actor& actor, const ro::sim::World& world,
                      const ro::sim::PartyRoster& party, const ro::ui::EntityBarsLook& look,
                      float anchor_scale) {
  return BarsOnSide(actor, world, party, look, anchor_scale, false);
}

std::vector<ro::ui::EntityBarView> EntityBarsOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party, std::uint32_t hovered_id,
    float viewport_width, float viewport_height) {
  std::vector<ro::ui::EntityBarView> bars;
  const ro::sim::TargetDetail& told = world.target_detail();
  const ro::sim::PlayerStatus& mine = world.player_status();
  // ⚠ LE RECUL SE PREND UNE FOIS, la taille du CORPS par acteur : c'est la
  // méthode du client, dont l'écart vaut douze pixels d'un écran de 480 de
  // haut, fois l'échelle de dessin du sprite — perspective comprise. Voir
  // `ScreenAnchorScale`.
  const float apparent = ro::render::ApparentScale(camera);

  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr) {
      continue;
    }
    // Un objet au sol n'a pas de vie, et une unité de compétence n'est pas un
    // acteur. Les écarter ici évite de projeter pour rien.
    if (actor->kind == ro::sim::ActorKind::kItem || actor->kind == ro::sim::ActorKind::kSkillUnit) {
      continue;
    }

    ro::ui::EntityBarView what;
    what.actor_id = actor->id;
    what.scale = ro::render::ScreenAnchorScale(viewport_height, apparent, drawn[index].scale);
    what.kind = EntityBarKindOf(*actor, world, party);
    what.dead = actor->posture == ro::sim::Posture::kDead;
    what.hovered = hovered_id != 0 && actor->id == hovered_id;
    what.targeted = world.selected_id() != 0 && actor->id == world.selected_id();

    what.health = Told(actor->health, actor->max_health);
    // 🔴 LE DOSSIER DE CIBLE PASSE DEVANT, quand il en dit plus. Il est la seule
    // source de la vie d'un joueur hors de mon groupe, et il ne s'écrit pas sur
    // l'acteur — les deux ne se contredisent donc jamais, l'un comble l'autre.
    if (what.targeted && told.id == actor->id && told.reachable) {
      if (const ro::ui::EntityGauge from_record = ToldOptional(told.health, told.max_health);
          from_record.known) {
        what.health = from_record;
      }
      what.magic = ToldOptional(told.magic, told.max_magic);
    }
    if (what.kind == ro::ui::EntityBarKind::kOwn) {
      // MA MAGIE VIENT DE MON ÉTAT, et de nulle part ailleurs : c'est la même
      // source que ma jauge libre, et les dédoubler les ferait diverger.
      what.magic = Told(static_cast<std::int32_t>(mine.sp), static_cast<std::int32_t>(mine.max_sp));
    }

    // Les PIEDS et le HAUT, tous deux : c'est le réglage qui choisit lequel
    // sert, et cette couche n'a pas à connaître ce choix.
    const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
        camera, aspect_ratio, drawn[index].ground_position, viewport_width, viewport_height);
    if (!feet.has_value()) {
      continue;
    }
    what.feet = ImVec2(feet->x, feet->y);
    what.head_y = feet->y;
    if (const std::optional<ro::render::ScreenRect> bounds = ro::render::ActorScreenBounds(
            camera, aspect_ratio, drawn[index], viewport_width, viewport_height);
        bounds.has_value()) {
      what.head_y = bounds->min.y;
    }
    bars.push_back(what);
  }
  return bars;
}

}  // namespace moonclient
