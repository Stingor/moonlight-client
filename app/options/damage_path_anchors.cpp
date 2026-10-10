// LES ANCRES DES TRAJECTOIRES DE DÉGÂTS, et ce qu'elles changent. Voir
// `damage_path_anchors.h`.

#include "options/damage_path_anchors.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/render/number_look.h"

#include "options/percent.h"
#include "world/number_paths.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::render::kFirstHandleShare;
using ro::render::kSecondHandleShare;
using ro::ui::PathAnchor;
using ro::ui::PathAnchorKind;

[[nodiscard]] ImVec2 Plus(ImVec2 left, ImVec2 right) noexcept {
  return ImVec2(left.x + right.x, left.y + right.y);
}

[[nodiscard]] ImVec2 Scaled(ImVec2 point, float share) noexcept {
  return ImVec2(point.x * share, point.y * share);
}

// La boîte d'une ancre : `reach` de part et d'autre de `around`.
[[nodiscard]] ImVec2 Lowest(ImVec2 around, float reach) noexcept {
  return ImVec2(around.x - reach, around.y - reach);
}
[[nodiscard]] ImVec2 Highest(ImVec2 around, float reach) noexcept {
  return ImVec2(around.x + reach, around.y + reach);
}

}  // namespace

std::vector<PathAnchor> PathAnchorsOf(const NumberPath& path, const AllureShift& allure,
                                      const NumberPath& client) {
  using ro::render::kLargestArrivalPixels;
  using ro::render::kLargestBendPixels;
  using ro::render::kLargestNumberLift;
  using ro::render::kLargestStartShiftPixels;
  const ImVec2 start(path.start_x_pixels, path.lift_pixels);
  const ImVec2 reach(path.arrival_x_pixels, path.arrival_y_pixels);
  const ImVec2 arrival = Plus(start, reach);
  // Les points du chemin droit que les poignées tirent : le tiers, les deux
  // tiers.
  const ImVec2 first = Plus(start, Scaled(reach, kFirstHandleShare));
  const ImVec2 second = Plus(start, Scaled(reach, kSecondHandleShare));
  const ImVec2 bend_one(path.bend_one_x_pixels, path.bend_one_y_pixels);
  const ImVec2 bend_two(path.bend_two_x_pixels, path.bend_two_y_pixels);
  // OÙ LE CLIENT LES MET : son départ et son arrivée, et ses poignées écartées
  // du chemin droit d'ICI — elles en restent solidaires.
  const ImVec2 client_start(client.start_x_pixels, client.lift_pixels);
  const ImVec2 client_arrival =
      Plus(client_start, ImVec2(client.arrival_x_pixels, client.arrival_y_pixels));

  std::vector<PathAnchor> anchors;
  anchors.push_back(
      PathAnchor{.id = static_cast<int>(GameOption::kDamageBendTwo),
                 .kind = PathAnchorKind::kBend,
                 .at = Plus(second, bend_two),
                 .lowest = Lowest(second, kLargestBendPixels),
                 .highest = Highest(second, kLargestBendPixels),
                 .rest = Plus(second, ImVec2(client.bend_two_x_pixels, client.bend_two_y_pixels)),
                 .label = Tr("Seconde poignée de la courbe"),
                 .value = bend_two});
  anchors.push_back(
      PathAnchor{.id = static_cast<int>(GameOption::kDamageBendOne),
                 .kind = PathAnchorKind::kBend,
                 .at = Plus(first, bend_one),
                 .lowest = Lowest(first, kLargestBendPixels),
                 .highest = Highest(first, kLargestBendPixels),
                 .rest = Plus(first, ImVec2(client.bend_one_x_pixels, client.bend_one_y_pixels)),
                 .label = Tr("Première poignée de la courbe"),
                 .value = bend_one});
  if (path.scatter) {
    // Les rayons tels que le jeu les emploie : jamais sous le plus petit.
    const float radius_x = std::max(path.scatter_x_pixels, kSmallestScatterPixels);
    const float radius_y = std::max(path.scatter_y_pixels, kSmallestScatterPixels);
    anchors.push_back(PathAnchor{.id = static_cast<int>(GameOption::kDamageScatterX),
                                 .kind = PathAnchorKind::kRadius,
                                 .at = ImVec2(arrival.x + radius_x, arrival.y),
                                 .lowest = ImVec2(arrival.x + kSmallestScatterPixels, arrival.y),
                                 .highest = ImVec2(arrival.x + kLargestScatterPixels, arrival.y),
                                 .rest = ImVec2(arrival.x + client.scatter_x_pixels, arrival.y),
                                 .label = Tr("Rayon horizontal"),
                                 .value = ImVec2(radius_x, 0.0F),
                                 .single = true});
    anchors.push_back(PathAnchor{.id = static_cast<int>(GameOption::kDamageScatterY),
                                 .kind = PathAnchorKind::kRadius,
                                 .at = ImVec2(arrival.x, arrival.y + radius_y),
                                 .lowest = ImVec2(arrival.x, arrival.y + kSmallestScatterPixels),
                                 .highest = ImVec2(arrival.x, arrival.y + kLargestScatterPixels),
                                 .rest = ImVec2(arrival.x, arrival.y + client.scatter_y_pixels),
                                 .label = Tr("Rayon vertical"),
                                 .value = ImVec2(radius_y, 0.0F),
                                 .single = true});
  }
  anchors.push_back(PathAnchor{.id = static_cast<int>(GameOption::kDamageArrivalX),
                               .kind = PathAnchorKind::kArrival,
                               .at = arrival,
                               .lowest = Lowest(start, kLargestArrivalPixels),
                               .highest = Highest(start, kLargestArrivalPixels),
                               .rest = client_arrival,
                               .label = Tr("Le point d'arrivée"),
                               .value = reach});
  anchors.push_back(PathAnchor{.id = static_cast<int>(GameOption::kDamageStartX),
                               .kind = PathAnchorKind::kStart,
                               .at = start,
                               .lowest = ImVec2(-kLargestStartShiftPixels, -kLargestNumberLift),
                               .highest = ImVec2(kLargestStartShiftPixels, kLargestNumberLift),
                               .rest = client_start,
                               .label = Tr("Le point de départ"),
                               .value = start});
  // SUR LE CHEMIN QUE LES NOMBRES SUIVENT VRAIMENT : chaque ancre se décale de
  // ce que l'allure a parcouru là où elle agit, sa boîte avec elle.
  for (PathAnchor& anchor : anchors) {
    const ImVec2 shift = AllureShiftOf(static_cast<GameOption>(anchor.id), allure);
    anchor.at = Plus(anchor.at, shift);
    anchor.lowest = Plus(anchor.lowest, shift);
    anchor.highest = Plus(anchor.highest, shift);
    anchor.rest = Plus(anchor.rest, shift);
  }
  // LES POIGNÉES SE RELIENT au bout qu'elles courbent : la première au départ,
  // la seconde à l'arrivée — à leur place décalée.
  const ImVec2 start_at = anchors.back().at;
  const ImVec2 arrival_at = anchors[anchors.size() - 2].at;
  anchors[0].tied = arrival_at;
  anchors[1].tied = start_at;
  return anchors;
}

ImVec2 AllureShiftOf(GameOption anchor, const AllureShift& allure) noexcept {
  switch (anchor) {
    case GameOption::kDamageBendOne:
      return allure.one;
    case GameOption::kDamageBendTwo:
      return allure.two;
    case GameOption::kDamageArrivalX:
    case GameOption::kDamageScatterX:
    case GameOption::kDamageScatterY:
      return allure.end;
    default:
      return ImVec2(0.0F, 0.0F);
  }
}

ro::ui::PathTrace PathTraceOf(const NumberPath& path) noexcept {
  return ro::ui::PathTrace{
      .start = ImVec2(path.start_x_pixels, path.lift_pixels),
      .arrival = ImVec2(path.arrival_x_pixels, path.arrival_y_pixels),
      .bend_one = ImVec2(path.bend_one_x_pixels, path.bend_one_y_pixels),
      .bend_two = ImVec2(path.bend_two_x_pixels, path.bend_two_y_pixels),
      .scatter = path.scatter,
      .scatter_radii = ImVec2(std::max(path.scatter_x_pixels, kSmallestScatterPixels),
                              std::max(path.scatter_y_pixels, kSmallestScatterPixels))};
}

bool MoveAnchor(NumberPath& path, GameOption anchor, ImVec2 point) noexcept {
  // ⚠ CHAQUE ANCRE SE LIT DEPUIS CE QUI LA PORTE : l'arrivée depuis le départ,
  // une poignée depuis le point du chemin droit qu'elle tire, un rayon depuis
  // l'arrivée — comme les champs qu'elle change.
  switch (anchor) {
    case GameOption::kDamageStartX: {
      // 🔴 L'ARRIVÉE RESTE OÙ ELLE EST : relative au départ, elle le suivait, et
      // deux points superposés ne se séparaient plus.
      using ro::render::kLargestArrivalPixels;
      const ImVec2 arrival(path.start_x_pixels + path.arrival_x_pixels,
                           path.lift_pixels + path.arrival_y_pixels);
      path.start_x_pixels = point.x;
      path.lift_pixels = point.y;
      path.arrival_x_pixels =
          std::clamp(arrival.x - point.x, -kLargestArrivalPixels, kLargestArrivalPixels);
      path.arrival_y_pixels =
          std::clamp(arrival.y - point.y, -kLargestArrivalPixels, kLargestArrivalPixels);
      return true;
    }
    case GameOption::kDamageArrivalX:
      path.arrival_x_pixels = point.x - path.start_x_pixels;
      path.arrival_y_pixels = point.y - path.lift_pixels;
      return true;
    case GameOption::kDamageBendOne:
      path.bend_one_x_pixels =
          point.x - (path.start_x_pixels + path.arrival_x_pixels * kFirstHandleShare);
      path.bend_one_y_pixels =
          point.y - (path.lift_pixels + path.arrival_y_pixels * kFirstHandleShare);
      return true;
    case GameOption::kDamageBendTwo:
      path.bend_two_x_pixels =
          point.x - (path.start_x_pixels + path.arrival_x_pixels * kSecondHandleShare);
      path.bend_two_y_pixels =
          point.y - (path.lift_pixels + path.arrival_y_pixels * kSecondHandleShare);
      return true;
    case GameOption::kDamageScatterX:
      path.scatter_x_pixels = point.x - (path.start_x_pixels + path.arrival_x_pixels);
      return true;
    case GameOption::kDamageScatterY:
      path.scatter_y_pixels = point.y - (path.lift_pixels + path.arrival_y_pixels);
      return true;
    default:
      return false;
  }
}

bool CopyPathPart(HitPopupLook& look, GameOption what, int target) {
  const DamagePath shown = DamagePathOf(look.path_shown);
  // ⚠ UNE COPIE DE LA SOURCE, PRISE AVANT TOUT : un type qui suit le coup
  // ordinaire changerait sous la main quand c'est lui qu'on écrase.
  const NumberPath source = PathOf(look, shown);
  const auto copy_into = [&](NumberPath& into) {
    switch (what) {
      case GameOption::kDamagePathCopy:
        into = source;
        return true;
      case GameOption::kDamageStartX:
        into.start_x_pixels = source.start_x_pixels;
        into.lift_pixels = source.lift_pixels;
        return true;
      case GameOption::kDamageArrivalX:
        into.arrival_x_pixels = source.arrival_x_pixels;
        into.arrival_y_pixels = source.arrival_y_pixels;
        return true;
      case GameOption::kDamageBendOne:
        into.bend_one_x_pixels = source.bend_one_x_pixels;
        into.bend_one_y_pixels = source.bend_one_y_pixels;
        return true;
      case GameOption::kDamageBendTwo:
        into.bend_two_x_pixels = source.bend_two_x_pixels;
        into.bend_two_y_pixels = source.bend_two_y_pixels;
        return true;
      case GameOption::kDamageScatterX:
        into.scatter = source.scatter;
        into.scatter_x_pixels = source.scatter_x_pixels;
        return true;
      case GameOption::kDamageScatterY:
        into.scatter = source.scatter;
        into.scatter_y_pixels = source.scatter_y_pixels;
        return true;
      default:
        return false;
    }
  };
  const auto every = static_cast<int>(kDamagePathCount);
  if (target < 0 || target > every) {
    return false;
  }
  bool copied = false;
  for (int rank = 0; rank < every; ++rank) {
    const auto path = static_cast<DamagePath>(rank);
    if (path == shown || (target != every && rank != target)) {
      continue;
    }
    copied = copy_into(PathOf(look, path)) || copied;
  }
  return copied;
}

bool ChangeDamagePath(HitPopupLook& look, GameOption which,
                      const ro::ui::OptionsWindowAction& asked) {
  const DamagePath shown = DamagePathOf(look.path_shown);
  // UNE COPIE PAR LE MENU DU CADRE, avant tout : elle porte l'identifiant d'une
  // ancre, qui se lirait sinon comme un curseur poussé à zéro.
  if (asked.copy_to >= 0) {
    return CopyPathPart(look, which, asked.copy_to);
  }
  switch (which) {
    case GameOption::kDamagePathShown:
      // ⚠ `chosen` ET NON `value` : c'est une liste déroulante.
      look.path_shown = static_cast<int>(DamagePathOf(asked.chosen));
      return true;
    case GameOption::kDamageRehearsalStep:
      look.rehearsal_interval_ms = asked.value;
      return true;
    case GameOption::kDamagePreviewFloor:
      look.preview_floor_pixels = asked.value;
      return true;
    case GameOption::kDamagePreviewGrid:
      look.preview_grid_pixels = asked.value;
      return true;
    // ── LA VIE DU TYPE MONTRÉ ──────────────────────────────────────────────
    // ⚠ LA TAILLE SE MONTRE EN POUR CENT et se garde en fois la taille finale.
    case GameOption::kDamageBirthScale:
      PathLifeOf(look, shown).birth_scale = asked.value / kPercent;
      return true;
    case GameOption::kDamageBirthSettle:
      PathLifeOf(look, shown).birth_settle_ms = asked.value;
      return true;
    case GameOption::kDamageFadeStart:
      PathLifeOf(look, shown).fade_start_ms = asked.value;
      return true;
    case GameOption::kDamageLifeSpan:
      PathLifeOf(look, shown).life_ms = asked.value;
      return true;
    case GameOption::kDamageFacingDrift:
      PathOf(look, shown).facing_drift = asked.on;
      return true;
    case GameOption::kDamagePathClient:
      // Ce type seul, chemin et vie : les autres gardent ce qu'on leur a donné.
      PathOf(look, shown) = ClientPathOf(shown);
      PathLifeOf(look, shown) = ro::render::ClientLifeOf(StyleOfPath(shown));
      return true;
    default:
      break;
  }
  // LE RESTE MODIFIE LA TRAJECTOIRE DU TYPE MONTRÉ.
  switch (which) {
    case GameOption::kDamageStartX:
    case GameOption::kDamageArrivalX:
    case GameOption::kDamageBendOne:
    case GameOption::kDamageBendTwo:
    case GameOption::kDamageScatterX:
    case GameOption::kDamageScatterY:
      return asked.dragged && MoveAnchor(PathOf(look, shown), which, asked.point);
    default:
      break;
  }
  if (which == GameOption::kDamageScatter) {
    PathOf(look, shown).scatter = asked.on;
    return true;
  }
  return false;
}

}  // namespace moonclient
