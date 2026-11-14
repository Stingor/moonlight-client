#include "ro/sim/shortcut_bar.h"

#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

using ro::sim::kShortcutBarCount;
using ro::sim::kShortcutSlotCount;
using ro::sim::kShortcutSlotsPerBar;
using ro::sim::ShortcutBarOfIndex;
using ro::sim::ShortcutBars;
using ro::sim::ShortcutIndex;
using ro::sim::ShortcutRankOfIndex;
using ro::sim::ShortcutSlot;

TEST_CASE("Barres : une case vide ne porte rien", "[sim][barre]") {
  ShortcutBars bars;
  CHECK_FALSE(bars.At(0).filled());
  CHECK_FALSE(bars.At(0).holds_item());
  // Une case hors des barres ne plante pas et ne porte rien non plus : les
  // index viennent d'une touche, et une table de raccourcis peut dériver.
  CHECK_FALSE(bars.At(kShortcutSlotCount).filled());
  bars.Assign(kShortcutSlotCount, ShortcutSlot{.skill_id = 5});
  CHECK(bars.revision() == 0);
}

TEST_CASE("Barres : l'index dit la barre et le rang", "[sim][barre]") {
  // 🔴 L'INVARIANT DE LA FICHE 0038 : une case a un index, et les deux nombres
  // s'en déduisent. C'est ce qui permet au raccourci, au fichier et au glisser
  // de parler de la même chose sans conversion.
  CHECK(ShortcutIndex(0, 0) == 0);
  CHECK(ShortcutIndex(0, kShortcutSlotsPerBar - 1) == kShortcutSlotsPerBar - 1);
  CHECK(ShortcutIndex(1, 0) == kShortcutSlotsPerBar);
  CHECK(ShortcutIndex(kShortcutBarCount - 1, kShortcutSlotsPerBar - 1) == kShortcutSlotCount - 1);

  for (std::size_t bar = 0; bar < kShortcutBarCount; ++bar) {
    for (std::size_t rank = 0; rank < kShortcutSlotsPerBar; ++rank) {
      const std::size_t index = ShortcutIndex(bar, rank);
      CHECK(ShortcutBarOfIndex(index) == bar);
      CHECK(ShortcutRankOfIndex(index) == rank);
    }
  }
}

TEST_CASE("Barres : les douze cases d'une barre lui appartiennent", "[sim][barre]") {
  ShortcutBars bars;
  bars.Assign(ShortcutIndex(3, 5), ShortcutSlot{.skill_id = 42, .level = 2});

  const std::span<const ShortcutSlot> third = bars.BarSlots(3);
  REQUIRE(third.size() == kShortcutSlotsPerBar);
  CHECK(third[5].skill_id == 42);
  CHECK_FALSE(third[4].filled());
  // La barre d'à côté n'en sait rien.
  CHECK_FALSE(bars.BarSlots(2)[5].filled());

  // Une barre hors compte rend une vue vide : les numéros de barre viennent d'un
  // réglage, et un réglage peut dériver.
  CHECK(bars.BarSlots(kShortcutBarCount).empty());
}

TEST_CASE("Barres : une compétence ne tient qu'une case DE SA BARRE", "[sim][barre]") {
  // Le jeu retire celle qu'on dépose de son ancienne place : sans quoi un dépôt
  // la DUPLIQUE, et deux cases voisines pour la même compétence ne servent qu'à
  // se tromper de touche.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 5, .level = 3});
  bars.Assign(4, ShortcutSlot{.skill_id = 5, .level = 3});
  CHECK_FALSE(bars.At(0).filled());
  CHECK(bars.At(4).skill_id == 5);
}

TEST_CASE("Barres : la même compétence à un AUTRE rang garde sa case", "[sim][barre]") {
  // Le dépôt du grimoire et sa touche tenue posent tous deux par `Assign`. Heal 1
  // sous une touche et Heal 10 sous la voisine sont deux gestes : poser le
  // second ne doit pas effacer le premier.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 28, .level = 1});
  bars.Assign(1, ShortcutSlot{.skill_id = 28, .level = 10});
  CHECK(bars.At(0).skill_id == 28);
  CHECK(bars.At(0).level == 1);
  CHECK(bars.At(1).skill_id == 28);
  CHECK(bars.At(1).level == 10);

  // Et la même entrée exacte, reposée ailleurs, quitte toujours sa place.
  bars.Assign(5, ShortcutSlot{.skill_id = 28, .level = 10});
  CHECK(bars.At(0).level == 1);
  CHECK_FALSE(bars.At(1).filled());
  CHECK(bars.At(5).level == 10);
}

TEST_CASE("Barres : régler le rang d'une case ne vide pas sa voisine", "[sim][barre]") {
  // La molette amène une case au rang de sa voisine : un réglage, pas un dépôt.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 28, .level = 1});
  bars.Assign(1, ShortcutSlot{.skill_id = 28, .level = 10});
  bars.SetLevel(0, 10);
  CHECK(bars.At(0).level == 10);
  CHECK(bars.At(1).skill_id == 28);
  CHECK(bars.At(1).level == 10);

  // Une case d'objet ou vide n'a pas de rang à régler, et rien ne change.
  bars.Assign(2, ShortcutSlot{.item_id = 501});
  const std::uint32_t before = bars.revision();
  bars.SetLevel(2, 4);
  bars.SetLevel(3, 4);
  bars.SetLevel(0, 10);
  CHECK(bars.At(2).level == 0);
  CHECK_FALSE(bars.At(3).filled());
  CHECK(bars.revision() == before);
}

TEST_CASE("Barres : la même compétence tient sur DEUX barres", "[sim][barre]") {
  // 🔴 CE QUE LA FICHE 0038 A CHANGÉ, ET POURQUOI. L'unicité portait sur toute
  // la barre unique ; appliquée aux huit, elle interdirait de poser Heal sur la
  // barre de soin ET sur celle de mêlée — la chose même pour quoi l'on fait huit
  // barres.
  ShortcutBars bars;
  bars.Assign(ShortcutIndex(0, 2), ShortcutSlot{.skill_id = 28, .level = 10});
  bars.Assign(ShortcutIndex(5, 7), ShortcutSlot{.skill_id = 28, .level = 10});
  CHECK(bars.At(ShortcutIndex(0, 2)).skill_id == 28);
  CHECK(bars.At(ShortcutIndex(5, 7)).skill_id == 28);
}

TEST_CASE("Barres : une case porte un objet, et l'un chasse l'autre", "[sim][barre]") {
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 5, .level = 3});
  REQUIRE(bars.At(0).filled());
  CHECK_FALSE(bars.At(0).holds_item());

  // Poser un objet sur la même case efface la compétence : une case qui
  // porterait les deux ferait deux gestes pour une touche, et chaque lecteur
  // trancherait à sa façon.
  bars.Assign(0, ShortcutSlot{.item_id = 501});
  CHECK(bars.At(0).holds_item());
  CHECK(bars.At(0).item_id == 501);
  CHECK(bars.At(0).skill_id == 0);
  CHECK(bars.At(0).level == 0);
}

TEST_CASE("Barres : poser un objet ne vide pas les cases des AUTRES", "[sim][barre]") {
  // 🔴 CE QUE CET ESSAI ATTRAPE, ET IL A FAILLI PASSER. Le dédoublonnage
  // comparait les seuls identifiants de COMPÉTENCE. Pour un objet celui-ci vaut
  // zéro — comme celui de toute case vide et de toute autre case d'objet — et
  // poser une potion vidait donc la barre entière, sans erreur ni trace.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.item_id = 501});
  bars.Assign(1, ShortcutSlot{.item_id = 502});
  bars.Assign(2, ShortcutSlot{.skill_id = 5, .level = 1});

  bars.Assign(3, ShortcutSlot{.item_id = 503});
  CHECK(bars.At(0).item_id == 501);
  CHECK(bars.At(1).item_id == 502);
  CHECK(bars.At(2).skill_id == 5);
  CHECK(bars.At(3).item_id == 503);
}

TEST_CASE("Barres : un objet ne tient qu'une case, comme une compétence", "[sim][barre]") {
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.item_id = 501});
  bars.Assign(4, ShortcutSlot{.item_id = 501});
  CHECK_FALSE(bars.At(0).filled());
  CHECK(bars.At(4).item_id == 501);
}

TEST_CASE("Barres : glisser une case sur une autre les ÉCHANGE", "[sim][barre]") {
  // 🔴 UN ÉCHANGE ET NON UN ÉCRASEMENT. Lâcher sur une case occupée rend son
  // contenu à la place qu'on quitte ; l'écraser ferait perdre une compétence
  // pour un geste d'un pixel trop à droite.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 5, .level = 3});
  bars.Assign(1, ShortcutSlot{.item_id = 501});

  bars.Swap(0, 1);
  CHECK(bars.At(0).item_id == 501);
  CHECK(bars.At(1).skill_id == 5);
  CHECK(bars.At(1).level == 3);
}

TEST_CASE("Barres : l'échange traverse les barres", "[sim][barre]") {
  ShortcutBars bars;
  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = 5, .level = 3});
  bars.Swap(ShortcutIndex(0, 0), ShortcutIndex(7, 11));
  CHECK_FALSE(bars.At(ShortcutIndex(0, 0)).filled());
  CHECK(bars.At(ShortcutIndex(7, 11)).skill_id == 5);
}

TEST_CASE("Barres : un échange qui ne déplace rien ne compte pas", "[sim][barre]") {
  // Deux cases vides, une case avec elle-même, un index hors des barres : le
  // fichier se réécrirait pour un glisser qui n'a rien fait.
  ShortcutBars bars;
  bars.Assign(0, ShortcutSlot{.skill_id = 5, .level = 3});
  const std::uint32_t before = bars.revision();
  bars.Swap(3, 4);
  bars.Swap(0, 0);
  bars.Swap(0, kShortcutSlotCount);
  CHECK(bars.revision() == before);
  CHECK(bars.At(0).skill_id == 5);
}

TEST_CASE("Barres : l'étendue utile s'arrête à la dernière case remplie", "[sim][barre]") {
  // ⚠ POUR L'ÉCRITURE DU FICHIER : douze entrées par barre, dont les dernières
  // vides, seraient illisibles à la main — et ce fichier est fait pour se
  // corriger à la main. Tronquer la queue ne déplace rien, l'index d'une case
  // restant ce qu'il est.
  ShortcutBars bars;
  CHECK(bars.used_span(0) == 0);

  bars.Assign(ShortcutIndex(0, 0), ShortcutSlot{.skill_id = 5});
  CHECK(bars.used_span(0) == 1);

  // 🔴 CHAQUE BARRE SE COMPTE SEULE. Une étendue commune ferait écrire les six
  // premières barres en entier pour une seule case posée sur la septième.
  bars.Assign(ShortcutIndex(6, 3), ShortcutSlot{.item_id = 501});
  CHECK(bars.used_span(0) == 1);
  CHECK(bars.used_span(6) == 4);
  CHECK(bars.used_span(5) == 0);

  bars.Clear(ShortcutIndex(6, 3));
  CHECK(bars.used_span(6) == 0);

  // Une barre hors compte rend zéro, comme une barre vide.
  CHECK(bars.used_span(kShortcutBarCount) == 0);
}

TEST_CASE("Barres : le compte de révisions suit chaque changement", "[sim][barre]") {
  // Un COMPTE et non un drapeau : qui enregistre compare au sien et sait s'il a
  // manqué un tour, là où un drapeau baissé par un premier lecteur priverait le
  // second.
  ShortcutBars bars;
  const std::uint32_t start = bars.revision();
  bars.Assign(0, ShortcutSlot{.item_id = 501});
  CHECK(bars.revision() > start);
  const std::uint32_t after = bars.revision();
  bars.Clear(0);
  CHECK(bars.revision() > after);
}
