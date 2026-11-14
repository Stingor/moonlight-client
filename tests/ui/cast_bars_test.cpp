// LA BARRE D'INCANTATION : QUAND ELLE PARAÎT, CE QU'ELLE ÉCRIT, ET CELLE DU CLIENT
// AU PIXEL PRÈS.
//
// 🔴 LA BARRE DU CLIENT SE VÉRIFIE ICI parce qu'elle ne se règle pas : soixante
// sur six, un pourcentage ENTIER, puis `largeur × pour cent / 100 − 2` tronqué.
// Un arrondi « plus juste » donnerait une barre qui ressemble sans être pareille.

#include "ro/ui/cast_bars.h"

#include <cstddef>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::ui::CastBarCaption;
using ro::ui::CastBarName;
using ro::ui::CastBarNameOf;
using ro::ui::CastBarProgress;
using ro::ui::CastBarsLook;
using ro::ui::CastBarView;
using ro::ui::CastSecondsText;
using ro::ui::ClientCastBarRects;
using ro::ui::ClientCastBarRectsOf;
using ro::ui::EntityBarKind;
using ro::ui::ShowsCastBar;

namespace {

[[nodiscard]] CastBarView Casting(EntityBarKind kind, std::uint32_t elapsed_ms,
                                  std::uint32_t total_ms) {
  CastBarView what;
  what.head = ImVec2(200.0F, 100.0F);
  what.kind = kind;
  what.elapsed_ms = elapsed_ms;
  what.total_ms = total_ms;
  what.skill_name = "Storm Gust";
  return what;
}

}  // namespace

TEST_CASE("Barre d'incantation : chaque espèce se montre ou se masque", "[ui][incantation]") {
  CastBarsLook look;
  CHECK(ShowsCastBar(look, Casting(EntityBarKind::kMonster, 0, 1'000)));
  look.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].shown = false;
  CHECK_FALSE(ShowsCastBar(look, Casting(EntityBarKind::kMonster, 0, 1'000)));
  // ⚠ LES AUTRES ESPÈCES N'Y SONT POUR RIEN.
  CHECK(ShowsCastBar(look, Casting(EntityBarKind::kBoss, 0, 1'000)));
  CHECK(ShowsCastBar(look, Casting(EntityBarKind::kPlayer, 0, 1'000)));
  // 🔴 SANS REMPLACEMENT AUSSI : masquer la sienne masque alors celle du client,
  // sinon décocher le remplacement ferait revenir ce qu'on voulait cacher.
  look.kinds[static_cast<std::size_t>(EntityBarKind::kOwn)].shown = false;
  look.replaced = false;
  CHECK_FALSE(ShowsCastBar(look, Casting(EntityBarKind::kOwn, 0, 1'000)));
  CHECK_FALSE(ShowsCastBar(look, Casting(EntityBarKind::kMonster, 0, 1'000)));
  // Une barre sans durée n'est pas une barre.
  CHECK_FALSE(ShowsCastBar(look, Casting(EntityBarKind::kPlayer, 0, 0)));
}

TEST_CASE("Barre d'incantation : les monstres et les boss sont orange d'origine",
          "[ui][incantation]") {
  const CastBarsLook look;
  CHECK(look.kinds[static_cast<std::size_t>(EntityBarKind::kMonster)].fill ==
        ro::ui::kMonsterCastBarFill);
  CHECK(look.kinds[static_cast<std::size_t>(EntityBarKind::kBoss)].fill ==
        ro::ui::kMonsterCastBarFill);
  CHECK(look.kinds[static_cast<std::size_t>(EntityBarKind::kPlayer)].fill == ro::ui::kCastBarFill);
}

TEST_CASE("Barre d'incantation : celle du client fait 60 sur 6, au-dessus de la tête",
          "[ui][incantation]") {
  const ClientCastBarRects rects = ClientCastBarRectsOf(Casting(EntityBarKind::kPlayer, 0, 1'000));
  CHECK(rects.frame_min.x == Catch::Approx(170.0F));
  CHECK(rects.frame_min.y == Catch::Approx(100.0F));
  CHECK(rects.frame_max.x - rects.frame_min.x == Catch::Approx(60.0F));
  CHECK(rects.frame_max.y - rects.frame_min.y == Catch::Approx(6.0F));
  // Le creux, un pixel en retrait de chaque côté.
  CHECK(rects.trough_min.x == Catch::Approx(171.0F));
  CHECK(rects.trough_max.x == Catch::Approx(229.0F));
  CHECK(rects.trough_max.y == Catch::Approx(105.0F));
  // Rien d'écoulé : pas de part pleine.
  CHECK(rects.fill_max.x <= rects.fill_min.x);
}

TEST_CASE("Barre d'incantation : la part pleine du client, arrondis compris", "[ui][incantation]") {
  const auto filled = [](std::uint32_t elapsed_ms) {
    const ClientCastBarRects rects =
        ClientCastBarRectsOf(Casting(EntityBarKind::kPlayer, elapsed_ms, 1'000));
    return rects.fill_max.x - rects.fill_min.x;
  };
  // 4 % : 60 × 4 / 100 − 2 = 0,4, tronqué à zéro.
  CHECK(filled(40) <= 0.0F);
  CHECK(filled(49) <= 0.0F);
  // 5 % : un pixel.
  CHECK(filled(50) == Catch::Approx(1.0F));
  // 50 % : 28.
  CHECK(filled(500) == Catch::Approx(28.0F));
  // 99 % : 57,4 tronqué à 57.
  CHECK(filled(999) == Catch::Approx(57.0F));
  // Au terme, elle rejoint le bord du creux.
  CHECK(filled(1'000) == Catch::Approx(58.0F));
}

TEST_CASE("Barre d'incantation : l'étiquette selon le réglage du nom", "[ui][incantation]") {
  ro::ui::CastBarLook look;
  CastBarView what = Casting(EntityBarKind::kMonster, 1'600, 3'000);
  CHECK(CastBarCaption(look, what) == "Storm Gust  " + CastSecondsText(1'400));
  // Une bulle l'annonce déjà : seul le temps s'écrit.
  what.balloon = true;
  CHECK(CastBarCaption(look, what) == CastSecondsText(1'400));
  look.name = CastBarName::kAlways;
  CHECK(CastBarCaption(look, what) == "Storm Gust  " + CastSecondsText(1'400));
  look.name = CastBarName::kNever;
  look.time_left = false;
  CHECK(CastBarCaption(look, what).empty());
  // Un sort qu'on ne sait pas nommer laisse le temps seul.
  look.name = CastBarName::kAlways;
  look.time_left = true;
  what.skill_name = {};
  CHECK(CastBarCaption(look, what) == CastSecondsText(1'400));
}

TEST_CASE("Barre d'incantation : le temps restant au dixième le plus proche", "[ui][incantation]") {
  CHECK(CastSecondsText(1'400) == "1,4 s");
  CHECK(CastSecondsText(1'449) == "1,4 s");
  CHECK(CastSecondsText(1'450) == "1,5 s");
  CHECK(CastSecondsText(0) == "0,0 s");
}

TEST_CASE("Barre d'incantation : la part écoulée et le rang lu du fichier", "[ui][incantation]") {
  CHECK(CastBarProgress(Casting(EntityBarKind::kPlayer, 750, 3'000)) == Catch::Approx(0.25F));
  CHECK(CastBarProgress(Casting(EntityBarKind::kPlayer, 9'000, 3'000)) == Catch::Approx(1.0F));
  CHECK(CastBarProgress(Casting(EntityBarKind::kPlayer, 0, 0)) == Catch::Approx(0.0F));
  CHECK(CastBarNameOf(2) == CastBarName::kAlways);
  CHECK(CastBarNameOf(-1) == CastBarName::kIfFree);
  CHECK(CastBarNameOf(7) == CastBarName::kIfFree);
}
