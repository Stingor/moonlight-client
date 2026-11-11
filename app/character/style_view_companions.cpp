// LES COMPAGNONS DE L'ÉDITEUR DE STYLE : un onglet pour l'homoncule et un pour
// le familier sortis, mesurés sur leur propre sprite, et leurs teintures portées.
//
// 🔴 LA TEINTURE D'UN COMPAGNON SE RANGE SOUS SON GID dans le livre, et non sous
// celui du joueur : c'est le bloc du compagnon qui la porte. L'éditeur les
// réunit ici, pour que l'historique et la validation les traitent comme les
// autres calques.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"
#include "ro/sim/actor.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "character/style_recipe.h"
#include "character/style_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

using ro::ui::PaletteTabKind;

constexpr int kPercent = 100;
// À PARTIR DE CETTE PART DE GRIS, l'onglet conseille le réglage absolu : les
// Lunatic, Nightmare Terror, Aliot, Eclipse, Alicel et Wanderer de
// l'installation sont entre 30 et 65 % de gris (docs/couleurs-du-personnage/
// compagnons.md).
constexpr long long kMostlyGreyPercent = 30;

[[nodiscard]] PaletteTabKind TabOfFamiliar(ro::sim::Familiar familiar) {
  return familiar == ro::sim::Familiar::kHomunculus ? PaletteTabKind::kHomunculus
                                                    : PaletteTabKind::kPet;
}

// La part de gris d'une mesure : les pièces sans teinte, et le gris du reste.
[[nodiscard]] bool MostlyGrey(const ro::render::ActorWardrobe::BodyRamps& measure) {
  if (measure.pixels_total <= 0) {
    return false;
  }
  long long grey = measure.rest.pixels[ro::render::kGreyHueClass];
  for (const ro::render::PaletteRamp& ramp : measure.ramps) {
    if (!ramp.hue.has_value()) {
      grey += ramp.pixels;
    }
  }
  return grey * kPercent >= kMostlyGreyPercent * measure.pixels_total;
}

}  // namespace

std::vector<StyleView::SeenCompanion> StyleView::CompanionsOf(const ro::sim::World& world) {
  std::vector<SeenCompanion> found;
  const std::uint32_t player_id = world.player_id();
  if (player_id == 0) {
    return found;
  }
  for (const auto& [gid, actor] : world.actors()) {
    const bool companion = actor.familiar == ro::sim::Familiar::kHomunculus ||
                           actor.familiar == ro::sim::Familiar::kPet;
    if (!companion || actor.summon_master_id != player_id || actor.look.job == 0) {
      continue;
    }
    found.push_back(
        SeenCompanion{.gid = gid, .job = actor.look.job, .tab = TabOfFamiliar(actor.familiar)});
  }
  // L'homoncule d'abord, puis le familier ; deux du même genre (une erreur du
  // serveur) gardent le premier gid.
  std::ranges::sort(found, [](const SeenCompanion& left, const SeenCompanion& right) {
    return std::pair(left.tab, left.gid) < std::pair(right.tab, right.gid);
  });
  const auto [first, last] = std::ranges::unique(found, {}, &SeenCompanion::tab);
  found.erase(first, last);
  return found;
}

void StyleView::AppendCompanionTabs(std::span<const SeenCompanion> companions,
                                    const ro::sim::World& world) {
  for (const SeenCompanion& seen : companions) {
    const ro::sim::Actor* actor = world.Find(seen.gid);
    if (actor == nullptr) {
      continue;
    }
    LayerTab tab;
    tab.shown.kind = seen.tab;
    tab.shown.companion = true;
    tab.shown.name = actor->name;
    tab.part = ro::render::ActorPart::kBody;
    tab.at = LayerAt{.layer = ro::render::StyleLayer::kCompanion, .key = seen.job};
    // ⚠ L'APPARENCE NUE : le pantin montre le compagnon tel qu'il est dessiné,
    // et la teinture s'y pose comme sur la carte.
    ro::render::ActorLook look = LookOf(*actor);
    look.style = 0;
    const ro::render::ActorWardrobe::BodyRamps measure =
        wardrobe_->LayerRampsFor(look, ro::render::ActorPart::kBody);
    tab.companion_look = look;
    tab.sprite_base = measure.base;
    if (measure.ramps.empty()) {
      // Aucun compagnon de l'installation n'en manque ; s'il arrivait, il n'aurait
      // rien à régler.
      tab.shown.blocked = ro::ui::PaletteTabBlock::kNoPiece;
    } else {
      tab.base = measure.base;
      tab.ramps = measure.ramps;
      tab.profile = ro::render::kLayerProfileNumber;
      tab.shown.coverage_percent = static_cast<int>(
          (static_cast<long long>(measure.pixels_covered) * kPercent + measure.pixels_total / 2) /
          measure.pixels_total);
      FillRest(tab, measure.rest);
      tab.shown.mostly_grey = MostlyGrey(measure);
    }
    tab.retouchable = mixed_allowed_ && measure.pixels_total > 0;
    tabs_.push_back(std::move(tab));
  }
}

void StyleView::ObserveWorn(const ro::sim::World& world) {
  std::vector<ro::render::LayerDye> worn;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> revisions;
  const auto take = [&](std::uint32_t gid, bool companion) {
    revisions.emplace_back(gid, world.styles().RevisionOf(gid));
    const std::vector<ro::sim::StyleLayerRecipe>* told = world.styles().FindLayers(gid);
    if (told == nullptr) {
      return;
    }
    for (const ro::sim::StyleLayerRecipe& recipe : *told) {
      // 🔴 CHACUN SES CALQUES : le bloc d'un compagnon ne dit que le sien, celui
      // du joueur jamais celui d'un compagnon.
      if ((recipe.layer == ro::sim::StyleLayerKind::kCompanion) != companion) {
        continue;
      }
      if (const std::optional<ro::render::LayerDye> dye = FromWire(recipe)) {
        worn.push_back(*dye);
      }
    }
  };
  take(world.player_id(), /*companion=*/false);
  for (const SeenCompanion& seen : CompanionsOf(world)) {
    take(seen.gid, /*companion=*/true);
  }
  // ⚠ UN NUMÉRO QUI AVANCE À CHAQUE CHANGEMENT de l'un des blocs, ou de qui est
  // sorti : `StyleLayers` ne relit rien tant qu'il ne change pas.
  if (revisions != worn_revisions_) {
    worn_revisions_ = std::move(revisions);
    ++worn_serial_;
  }
  layers_.Observe(worn, worn_serial_);
}

}  // namespace moonclient
