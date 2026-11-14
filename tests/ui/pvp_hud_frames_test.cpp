#include "ro/ui/pvp_hud_frames.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::ui::DefaultPvpHudRect;
using ro::ui::HudRect;
using ro::ui::kPvpRankReferenceWidth;
using ro::ui::PvpHudLook;
using ro::ui::PvpHudPart;
using ro::ui::PvpHudPlacementOf;
using ro::ui::PvpHudRectUnset;
using ro::ui::PvpHudScaleOf;
using ro::ui::Rank;

namespace {

constexpr glm::vec2 kScreen{1280.0F, 720.0F};

}  // namespace

TEST_CASE("HUD PVP : sans cadre posé, les deux pièces sont là où le client les met", "[ui][pvp]") {
  // 🔴 LE DÉFAUT EST CELUI DU CLIENT D'ORIGINE, et il se CALCULE : un défaut
  // écrit dans le fichier de réglages serait celui d'UNE taille d'écran, et
  // suivrait le joueur qui change de résolution — deux pièces coincées hors de
  // l'image, sans rien qui le dise.
  const PvpHudLook fresh;
  REQUIRE(PvpHudRectUnset(fresh.parts[Rank(PvpHudPart::kRank)]));

  const ro::render::PvpHudPlacement rank_placement =
      PvpHudPlacementOf(PvpHudPart::kRank, fresh, kScreen);
  CHECK(rank_placement.corner_px.x == Catch::Approx(kScreen.x));
  CHECK(rank_placement.corner_px.y == Catch::Approx(kScreen.y));
  CHECK(rank_placement.scale == Catch::Approx(1.0F));

  // Le chronomètre part du MILIEU du bord haut, et non d'un coin : son nombre de
  // glyphes change avec la durée, et il doit rester centré sur lui-même.
  const ro::render::PvpHudPlacement chrono = PvpHudPlacementOf(PvpHudPart::kTimer, fresh, kScreen);
  CHECK(chrono.corner_px.x == Catch::Approx(kScreen.x / 2.0F));
  CHECK(chrono.scale == Catch::Approx(1.0F));
}

TEST_CASE("HUD PVP : le cadre par défaut enferme le dessin par défaut", "[ui][pvp]") {
  // ⚠ SINON LE PREMIER DÉVERROUILLAGE MENTIRAIT : on attraperait une poignée
  // posée ailleurs que le dessin qu'elle commande, et on la déplacerait en
  // croyant déplacer autre chose.
  const PvpHudLook fresh;
  for (const PvpHudPart part : {PvpHudPart::kRank, PvpHudPart::kTimer}) {
    PvpHudLook pose;
    pose.parts[Rank(part)] = DefaultPvpHudRect(part, kScreen);

    const ro::render::PvpHudPlacement before = PvpHudPlacementOf(part, fresh, kScreen);
    const ro::render::PvpHudPlacement after = PvpHudPlacementOf(part, pose, kScreen);
    CHECK(after.corner_px.x == Catch::Approx(before.corner_px.x));
    CHECK(after.corner_px.y == Catch::Approx(before.corner_px.y));
    CHECK(after.scale == Catch::Approx(1.0F));
  }
}

TEST_CASE("HUD PVP : retailler le cadre retaille les chiffres", "[ui][pvp]") {
  // 🔴 LA TAILLE EST LE CADRE, et il n'y a pas de curseur d'échelle à côté : deux
  // façons de dire la même chose se contrediraient dès qu'on emploie les deux.
  HudRect rank_rect = DefaultPvpHudRect(PvpHudPart::kRank, kScreen);
  CHECK(PvpHudScaleOf(PvpHudPart::kRank, rank_rect) == Catch::Approx(1.0F));

  rank_rect.width = kPvpRankReferenceWidth * 2.0F;
  CHECK(PvpHudScaleOf(PvpHudPart::kRank, rank_rect) == Catch::Approx(2.0F));

  // ⚠ LA LARGEUR SEULE : tirer le cadre vers le bas pour lui faire de la marge ne
  // doit pas rapetisser les chiffres.
  rank_rect.height *= 3.0F;
  CHECK(PvpHudScaleOf(PvpHudPart::kRank, rank_rect) == Catch::Approx(2.0F));

  PvpHudLook pose;
  pose.parts[Rank(PvpHudPart::kRank)] = rank_rect;
  CHECK(PvpHudPlacementOf(PvpHudPart::kRank, pose, kScreen).scale == Catch::Approx(2.0F));
}

TEST_CASE("HUD PVP : verrouillé par défaut", "[ui][pvp]") {
  // ⚠ ON NE COMPOSE SON ÉCRAN QUE LORSQU'ON LE DÉCIDE : un HUD qu'on attrape par
  // accident en pleine mêlée est un HUD perdu.
  CHECK(PvpHudLook{}.locked);
}
