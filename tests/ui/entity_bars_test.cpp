// QUAND UNE BARRE D'ENTITÉ PARAÎT, ET DE QUELLE COULEUR.
//
// 🔴 CE QUI SE VÉRIFIE ICI EST CE QU'UN ÉCRAN NE DIT PAS. Une barre qui ne vient
// jamais ressemble trait pour trait à un serveur qui se tait : le joueur cherche
// alors la panne du côté du réseau, et le réglage fautif reste en place. Cinq
// conditions se croisent — l'espèce éteinte, le mort, la valeur inconnue, le
// seuil d'entame, le survol —, et un « et » de trop les rend toutes muettes.
//
// 🔴 ET LA BASCULE DE COULEUR EST CELLE DU CLIENT, PAS UN GOÛT : il passe au
// rouge sous VINGT-CINQ POUR CENT. Un seuil qu'on aurait « arrondi » à un tiers
// donnerait un écran qui ressemble sans être pareil.

#include "ro/ui/entity_bars.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/lone_draw_list.h"

using ro::ui::DefaultEntityBars;
using ro::ui::EntityBarCaption;
using ro::ui::EntityBarFillColor;
using ro::ui::EntityBarFontPixels;
using ro::ui::EntityBarHealthCaption;
using ro::ui::EntityBarKind;
using ro::ui::EntityBarLook;
using ro::ui::EntityBarsHeight;
using ro::ui::EntityBarText;
using ro::ui::EntityBarTextOf;
using ro::ui::EntityBarView;
using ro::ui::EntityBarWhen;
using ro::ui::EntityBarWhenOf;
using ro::ui::EntityGauge;
using ro::ui::EntityGaugeFill;
using ro::ui::kSmallestBakedFontPixels;
using ro::ui::kSmallestEntityBarFont;
using ro::ui::ShowsEntityBar;

namespace {

[[nodiscard]] EntityGauge Told(std::int32_t current, std::int32_t most) {
  return EntityGauge{.known = true, .current = current, .most = most};
}

// Une entité entamée, que rien ne désigne.
[[nodiscard]] EntityBarView Hurt() {
  EntityBarView what;
  what.kind = EntityBarKind::kMonster;
  what.health = Told(40, 100);
  return what;
}

}  // namespace

TEST_CASE("Barre d'entité : une vie inconnue ne dessine rien", "[ui][barres]") {
  // Le serveur ne dit la vie d'un monstre qu'une fois entamé, et sous un
  // réglage : ne rien savoir est le cas ORDINAIRE. Une barre vide annoncerait
  // une mort qui n'a pas eu lieu.
  EntityBarLook look;
  look.when = EntityBarWhen::kAlways;
  EntityBarView silent;
  CHECK_FALSE(ShowsEntityBar(look, silent));
  CHECK(EntityGaugeFill(silent.health) == Catch::Approx(0.0F));
}

TEST_CASE("Barre d'entité : « une fois entamé » est le client", "[ui][barres]") {
  EntityBarLook look;
  look.when = EntityBarWhen::kDamaged;

  EntityBarView intact;
  intact.health = Told(100, 100);
  CHECK_FALSE(ShowsEntityBar(look, intact));
  CHECK(ShowsEntityBar(look, Hurt()));

  // ⚠ LA VIE SEULE DÉCIDE DE L'ENTAME : une magie dépensée sur un porteur
  // intact ne fait pas paraître sa barre de vie.
  EntityBarView spent = intact;
  spent.magic = Told(10, 100);
  look.magic = true;
  CHECK_FALSE(ShowsEntityBar(look, spent));

  // Et « toujours » la montre, elle.
  look.when = EntityBarWhen::kAlways;
  CHECK(ShowsEntityBar(look, intact));
}

TEST_CASE("Barre d'entité : un mort n'en porte plus", "[ui][barres]") {
  EntityBarLook look;
  look.when = EntityBarWhen::kAlways;
  EntityBarView dead = Hurt();
  dead.dead = true;
  CHECK_FALSE(ShowsEntityBar(look, dead));
  // Sauf si on demande le contraire : un cadavre reste une cible sur laquelle
  // on lance une résurrection.
  look.hide_dead = false;
  CHECK(ShowsEntityBar(look, dead));
}

TEST_CASE("Barre d'entité : le survol et la cible", "[ui][barres]") {
  EntityBarLook look;
  EntityBarView what = Hurt();

  look.when = EntityBarWhen::kHovered;
  CHECK_FALSE(ShowsEntityBar(look, what));
  what.hovered = true;
  CHECK(ShowsEntityBar(look, what));

  what.hovered = false;
  look.when = EntityBarWhen::kTarget;
  CHECK_FALSE(ShowsEntityBar(look, what));
  what.targeted = true;
  CHECK(ShowsEntityBar(look, what));

  // L'un OU l'autre.
  look.when = EntityBarWhen::kHoveredOrTarget;
  CHECK(ShowsEntityBar(look, what));
  what.targeted = false;
  what.hovered = true;
  CHECK(ShowsEntityBar(look, what));
}

TEST_CASE("Barre d'entité : l'espèce éteinte n'en montre aucune", "[ui][barres]") {
  EntityBarLook look;
  look.when = EntityBarWhen::kAlways;
  look.shown = false;
  CHECK_FALSE(ShowsEntityBar(look, Hurt()));

  // Et une espèce allumée dont les DEUX barres sont éteintes ne dessine pas un
  // cadre vide.
  look.shown = true;
  look.health = false;
  look.magic = false;
  CHECK_FALSE(ShowsEntityBar(look, Hurt()));
}

TEST_CASE("Barre d'entité : la magie seule suffit à la faire paraître", "[ui][barres]") {
  // Sur soi et sur la cible, la magie est connue ; la vie peut ne pas l'être.
  EntityBarLook look;
  look.when = EntityBarWhen::kAlways;
  look.magic = true;
  EntityBarView what;
  what.magic = Told(30, 60);
  CHECK(ShowsEntityBar(look, what));
  CHECK(EntityGaugeFill(what.magic) == Catch::Approx(0.5F));
}

TEST_CASE("Barre d'entité : la bascule tombe au quart, comme au client", "[ui][barres]") {
  EntityBarLook look;
  // Juste au-dessus du seuil : la couleur pleine ; juste en dessous : l'alerte.
  CHECK(EntityBarFillColor(look, 0.26F) == look.fill);
  CHECK(EntityBarFillColor(look, 0.25F) == look.fill);
  CHECK(EntityBarFillColor(look, 0.24F) == look.low_fill);
  CHECK(EntityBarFillColor(look, 0.0F) == look.low_fill);

  // Le seuil est un réglage, et la bascule le suit.
  look.low_share = 0.50F;
  CHECK(EntityBarFillColor(look, 0.40F) == look.low_fill);
}

TEST_CASE("Barre d'entité : le dégradé ne mord qu'en dessous du seuil", "[ui][barres]") {
  EntityBarLook look;
  look.gradient = true;
  // Au-dessus, la couleur pleine ne bouge pas : un vert qui jaunit à la première
  // égratignure se lirait comme une alerte qui n'en est pas une.
  CHECK(EntityBarFillColor(look, 1.0F) == look.fill);
  CHECK(EntityBarFillColor(look, 0.60F) == look.fill);
  // À vide, l'alerte pleine.
  CHECK(EntityBarFillColor(look, 0.0F) == look.low_fill);
  // Entre les deux, ni l'une ni l'autre.
  const ImU32 mixed = EntityBarFillColor(look, 0.125F);
  CHECK(mixed != look.fill);
  CHECK(mixed != look.low_fill);
}

TEST_CASE("Barre d'entité : ce que le texte écrit", "[ui][barres]") {
  const EntityGauge gauge = Told(40, 100);
  CHECK(EntityBarCaption(gauge, EntityBarText::kNothing).empty());
  CHECK(EntityBarCaption(gauge, EntityBarText::kPercent) == "40%");
  CHECK(EntityBarCaption(gauge, EntityBarText::kValues) == "40 / 100");
  CHECK(EntityBarCaption(gauge, EntityBarText::kBoth) == "40 / 100  (40%)");

  // ⚠ RIEN N'EST ÉCRIT D'UNE VALEUR INCONNUE : un « 0 % » inventé annoncerait
  // une mort.
  CHECK(EntityBarCaption(EntityGauge{}, EntityBarText::kBoth).empty());

  // Des PV reçus en pourcentage ne s'écrivent jamais « 37 / 100 ».
  EntityGauge percent = Told(37, 100);
  percent.in_percent = true;
  CHECK(EntityBarCaption(percent, EntityBarText::kValues) == "37%");
  CHECK(EntityBarCaption(percent, EntityBarText::kBoth) == "37%");
  CHECK(EntityBarCaption(percent, EntityBarText::kNothing).empty());
}

TEST_CASE("Barre d'entité : un adversaire garde sa jauge, sans chiffres", "[ui][barres]") {
  EntityBarView enemy;
  enemy.kind = EntityBarKind::kPlayer;
  enemy.is_player = true;
  enemy.health = Told(37, 100);
  enemy.health.in_percent = true;
  // La même règle que la fenêtre de cible : aucun mode n'écrit de chiffre.
  for (const EntityBarText mode : {EntityBarText::kPercent, EntityBarText::kValues,
                                   EntityBarText::kBoth, EntityBarText::kNothing}) {
    CHECK(EntityBarHealthCaption(enemy, mode).empty());
  }
  // La jauge, elle, reste remplie.
  CHECK(EntityGaugeFill(enemy.health) == Catch::Approx(0.37F));

  EntityBarView ally = enemy;
  ally.kind = EntityBarKind::kParty;
  ally.ally = true;
  CHECK(EntityBarHealthCaption(ally, EntityBarText::kValues) == "37%");

  CHECK(EntityBarHealthCaption(Hurt(), EntityBarText::kValues) == "40 / 100");
}

TEST_CASE("Barre d'entité : la hauteur ne compte que ce qui se dessine", "[ui][barres]") {
  EntityBarLook look;
  look.height_pixels = 6.0F;
  look.magic_height_pixels = 3.0F;
  look.gap_pixels = 2.0F;

  EntityBarView what = Hurt();
  CHECK(EntityBarsHeight(look, what) == Catch::Approx(6.0F));

  // La magie allumée mais inconnue ne prend pas de place : c'est ce qui évite
  // que l'enseigne d'un vendeur ne flotte au-dessus de rien.
  look.magic = true;
  CHECK(EntityBarsHeight(look, what) == Catch::Approx(6.0F));

  what.magic = Told(10, 20);
  CHECK(EntityBarsHeight(look, what) == Catch::Approx(6.0F + 2.0F + 3.0F));
}

TEST_CASE("Barre d'entité : un rang venu du fichier reste borné", "[ui][barres]") {
  // Le joueur ouvre son fichier de réglages dans un éditeur de texte, et un
  // `static_cast` hors bornes sur une énumération est un comportement indéfini.
  CHECK(EntityBarWhenOf(-1) == EntityBarWhen::kAlways);
  CHECK(EntityBarWhenOf(999) == EntityBarWhen::kAlways);
  CHECK(EntityBarWhenOf(static_cast<int>(EntityBarWhen::kCount)) == EntityBarWhen::kAlways);
  CHECK(EntityBarWhenOf(2) == EntityBarWhen::kHovered);
  CHECK(EntityBarTextOf(-3) == EntityBarText::kNothing);
  CHECK(EntityBarTextOf(3) == EntityBarText::kBoth);
}

namespace {

using ro::test::LoneDrawList;

// Les trois couches se distinguent par leur canal dominant : le cadre est rouge,
// le creux bleu, le remplissage vert. Un sommet ne peut donc pas être confondu.
constexpr ImU32 kFrameRed = IM_COL32(230, 10, 10, 255);
constexpr ImU32 kHollowBlue = IM_COL32(10, 10, 230, 255);
constexpr ImU32 kFillGreen = IM_COL32(10, 230, 10, 255);

[[nodiscard]] bool Channelled(const ImDrawVert& corner, int shift) {
  const int mine = static_cast<int>((corner.col >> shift) & 0xFFU);
  const int red = static_cast<int>((corner.col >> IM_COL32_R_SHIFT) & 0xFFU);
  const int green = static_cast<int>((corner.col >> IM_COL32_G_SHIFT) & 0xFFU);
  const int blue = static_cast<int>((corner.col >> IM_COL32_B_SHIFT) & 0xFFU);
  return mine > 120 && mine >= red && mine >= green && mine >= blue;
}

[[nodiscard]] float DistanceFrom(ImVec2 middle, ImVec2 point) {
  const float dx = point.x - middle.x;
  const float dy = point.y - middle.y;
  return std::sqrt(dx * dx + dy * dy);
}

// ⚠ CE QU'UN COSINUS COÛTE : les sommets d'un arc se calculent en flottants
// simples. Un vingtième de pixel, comme pour les orbes.
constexpr float kRoundingSlack = 0.05F;

[[nodiscard]] EntityBarLook Framed(float rounding) {
  EntityBarLook look;
  look.border = true;
  look.rounding = rounding;
  look.border_color = kFrameRed;
  look.background = kHollowBlue;
  return look;
}

}  // namespace

namespace {

// LE POINT DU CREUX LE PLUS PROCHE DU COIN de la barre. C'est lui qui dit
// combien la barre est arrondie : sur un rectangle droit il vaut zéro, et il
// s'éloigne à mesure que le rayon grandit.
[[nodiscard]] float HollowCornerGap(const LoneDrawList& sheet, ImVec2 corner_of_bar) {
  float nearest = FLT_MAX;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    if (Channelled(corner, IM_COL32_B_SHIFT)) {
      nearest = std::min(nearest, DistanceFrom(corner_of_bar, corner.pos));
    }
  }
  return nearest;
}

}  // namespace

TEST_CASE("Barre d'entité : le cadre ne rogne pas l'arrondi du contenu", "[ui][barres]") {
  // 🔴 LE DÉFAUT QUE LE JOUEUR A VU, ET LA MESURE QUI LE DIT. Le cadre se
  // peignait en rectangle PLEIN sous le contenu, qui rentrait donc d'un pixel de
  // chaque côté — et un pixel de hauteur en moins, c'est un pixel d'arrondi en
  // moins, ImGui bornant tout rayon à `hauteur / 2 - 1`. Sur la barre du client,
  // cinq pixels de haut, le contenu tombait à un rectangle DROIT.
  constexpr float kRounding = 8.0F;
  const ImVec2 top_left(0.0F, 0.0F);
  const ImVec2 bottom_right(120.0F, 24.0F);

  LoneDrawList framed;
  ro::ui::PaintEntityBar(framed.into, top_left, bottom_right, 0.0F, kFillGreen, Framed(kRounding));

  EntityBarLook bare = Framed(kRounding);
  bare.border = false;
  LoneDrawList plain;
  ro::ui::PaintEntityBar(plain.into, top_left, bottom_right, 0.0F, kFillGreen, bare);

  const float with_frame = HollowCornerGap(framed, top_left);
  const float without = HollowCornerGap(plain, top_left);

  // ⚠ L'ESSAI DOIT AVOIR VU UN ARRONDI : sur un rectangle droit, les deux
  // vaudraient zéro et l'égalité ne prouverait rien.
  CHECK(without > 1.0F);
  CHECK(with_frame == Catch::Approx(without).margin(kRoundingSlack));
}

TEST_CASE("Barre d'entité : une part minuscule ne déborde pas des coins", "[ui][barres]") {
  // 🔴 LE DÉFAUT QUE LE JOUEUR A VU : un remplissage de quelques pixels n'avait
  // plus d'arrondi — ImGui borne le rayon d'un rectangle à sa propre largeur — et
  // son carré sortait des coins du cadre.
  constexpr float kRounding = 8.0F;
  constexpr float kSliver = 0.01F;
  const ImVec2 top_left(0.0F, 0.0F);
  const ImVec2 bottom_right(120.0F, 24.0F);

  LoneDrawList sheet;
  ro::ui::PaintEntityBar(sheet.into, top_left, bottom_right, kSliver, kFillGreen,
                         Framed(kRounding));

  // L'intérieur : le cadre moins sa bordure, et les centres de ses deux coins
  // de gauche — ceux du cadre, puisque la courbe est parallèle.
  const float inner_radius = kRounding - ro::ui::kEntityBarBorderPixels;
  const ImVec2 upper(top_left.x + kRounding, top_left.y + kRounding);
  const ImVec2 lower(top_left.x + kRounding, bottom_right.y - kRounding);
  int seen = 0;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    if (!Channelled(corner, IM_COL32_G_SHIFT)) {
      continue;
    }
    ++seen;
    // Toute la part pleine tient dans la zone des coins : elle y suit la courbe.
    CHECK(corner.pos.x <= upper.x);
    if (corner.pos.y < upper.y) {
      CHECK(DistanceFrom(upper, corner.pos) <= inner_radius + kRoundingSlack);
    } else if (corner.pos.y > lower.y) {
      CHECK(DistanceFrom(lower, corner.pos) <= inner_radius + kRoundingSlack);
    } else {
      CHECK(corner.pos.x >= top_left.x + ro::ui::kEntityBarBorderPixels - kRoundingSlack);
    }
  }
  CHECK(seen > 0);
}

TEST_CASE("Barre d'entité : à quatre-vingt-dix-neuf pour cent, rien ne dépasse", "[ui][barres]") {
  // 🔴 À 99 % d'une barre de cent vingt pixels, le front est déjà dans le coin
  // arrondi du creux : un front carré y dépasserait la courbe.
  constexpr float kRounding = 8.0F;
  constexpr float kNearlyFull = 0.99F;
  const ImVec2 top_left(0.0F, 0.0F);
  const ImVec2 bottom_right(120.0F, 24.0F);

  LoneDrawList sheet;
  ro::ui::PaintEntityBar(sheet.into, top_left, bottom_right, kNearlyFull, kFillGreen,
                         Framed(kRounding));

  // Le centre du disque du coin HAUT-DROIT, et ce que l'encre a le droit d'en
  // atteindre : le rayon de l'intérieur, bordure déduite. ⚠ LE BANC PEINT SANS
  // FONDU, donc sans le demi-pixel qu'ImGui étalerait au-delà du contour.
  const ImVec2 middle(bottom_right.x - kRounding, top_left.y + kRounding);
  const float allowed = kRounding - ro::ui::kEntityBarBorderPixels + kRoundingSlack;

  int seen = 0;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    if (!Channelled(corner, IM_COL32_G_SHIFT)) {
      continue;
    }
    if (corner.pos.x <= middle.x || corner.pos.y >= middle.y) {
      continue;
    }
    ++seen;
    CHECK(DistanceFrom(middle, corner.pos) <= allowed);
  }
  // ⚠ ET L'ESSAI DOIT AVOIR REGARDÉ QUELQUE CHOSE : sans sommet dans ce
  // quadrant, il serait vert sans rien prouver.
  CHECK(seen > 0);
}

TEST_CASE("Barre d'entité : le front d'une barre partielle reste franc", "[ui][barres]") {
  // Un bout arrondi au milieu du creux a l'air de s'effacer avant d'arriver :
  // c'est la règle des jauges libres, et elle vaut ici pour la même raison.
  constexpr float kRounding = 8.0F;
  constexpr float kHalf = 0.5F;
  const ImVec2 top_left(0.0F, 0.0F);
  const ImVec2 bottom_right(120.0F, 24.0F);

  LoneDrawList sheet;
  ro::ui::PaintEntityBar(sheet.into, top_left, bottom_right, kHalf, kFillGreen, Framed(kRounding));

  // ⚠ L'INTÉRIEUR, ET NON LE CADRE : la part pleine se tient sous la bordure.
  const float border = ro::ui::kEntityBarBorderPixels;
  const float hollow_top = top_left.y + border;
  const float hollow_bottom = bottom_right.y - border;
  const float front = top_left.x + border + (bottom_right.x - top_left.x - 2.0F * border) * kHalf;

  bool touches_top = false;
  bool touches_bottom = false;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    if (!Channelled(corner, IM_COL32_G_SHIFT)) {
      continue;
    }
    if (std::abs(corner.pos.x - front) > kRoundingSlack) {
      continue;
    }
    touches_top = touches_top || std::abs(corner.pos.y - hollow_top) <= kRoundingSlack;
    touches_bottom = touches_bottom || std::abs(corner.pos.y - hollow_bottom) <= kRoundingSlack;
  }
  // Arrondi à droite, aucun sommet n'aurait à la fois l'abscisse du front et
  // l'ordonnée d'un bord : c'est exactement ce que cette mesure attrape.
  CHECK(touches_top);
  CHECK(touches_bottom);
}

TEST_CASE("Barre d'entité : les défauts disent ce que le serveur permet", "[ui][barres]") {
  const auto bars = DefaultEntityBars();

  // La mienne est éteinte : le client d'origine ne la montre pas non plus.
  CHECK_FALSE(bars[static_cast<std::size_t>(EntityBarKind::kOwn)].shown);
  // Un compagnon de groupe se voit intact : c'est à cela que sa barre sert.
  CHECK(bars[static_cast<std::size_t>(EntityBarKind::kParty)].when == EntityBarWhen::kAlways);
  // Un joueur quelconque n'a de vie connue que s'il est désigné : promettre
  // « toujours » serait promettre un affichage que le protocole ne tient pas.
  CHECK(bars[static_cast<std::size_t>(EntityBarKind::kPlayer)].when ==
        EntityBarWhen::kHoveredOrTarget);
  // Un monstre suit le client : une fois entamé.
  CHECK(bars[static_cast<std::size_t>(EntityBarKind::kMonster)].when == EntityBarWhen::kDamaged);
  // Un PNJ ne porte pas de barre, un compagnon en porte une même intact.
  CHECK_FALSE(bars[static_cast<std::size_t>(EntityBarKind::kOther)].shown);
  CHECK(bars[static_cast<std::size_t>(EntityBarKind::kCompanion)].shown);
  CHECK(bars[static_cast<std::size_t>(EntityBarKind::kCompanion)].when == EntityBarWhen::kAlways);
  // Un boss se lit de loin, et écrit sa part.
  const EntityBarLook& boss = bars[static_cast<std::size_t>(EntityBarKind::kBoss)];
  CHECK(boss.width_pixels > bars[static_cast<std::size_t>(EntityBarKind::kMonster)].width_pixels);
  CHECK(boss.text == EntityBarText::kPercent);
}

TEST_CASE("Barre d'entité : la taille du texte tombe sur le texel", "[ui][barres]") {
  // 🔴 IMGUI CUIT SES GLYPHES À `IM_ROUND(taille)` PUIS LES ÉTIRE DE
  // `taille / cuite` (`ImFont::RenderText`, 1.92). Une taille fractionnaire fait
  // donc rééchantillonner chaque lettre : sur la rampe de zoom, l'étirement
  // monte à 10 %, et il SAUTE quinze fois — de 1,08 à 0,92 d'un coup, à chaque
  // demi-pixel franchi. C'est ce que le joueur voyait : un texte qui s'épaissit,
  // maigrit et ressaute pendant qu'il tourne la molette.
  //
  // ⚠ CE QUE CET ESSAI TIENT : la taille demandée est ENTIÈRE, donc l'étirement
  // vaut un et les lettres se posent texel pour texel.
  //
  // ⚠ LE ZOOM N'ENTRE PLUS DANS CE CALCUL : la taille réglée est celle qu'on
  // grave, comme chez le client, dont la jauge est en pixels d'écran. La règle
  // du texel, elle, reste — un réglage fractionnaire la briserait tout autant.
  EntityBarLook look;

  for (float reglee = 5.0F; reglee <= 24.0F; reglee += 0.013F) {
    look.font_pixels = reglee;
    const float pixels = EntityBarFontPixels(look);
    CHECK(pixels == std::floor(pixels));
    CHECK(pixels >= kSmallestBakedFontPixels);
  }

  // ⚠ ET UNE TAILLE RÉGLÉE ENTRE DEUX PIXELS TOMBE SUR LE PLUS PROCHE : le
  // curseur affiche déjà « %.0f px », donc il ANNONÇAIT un entier pendant que le
  // dessin en demandait un autre.
  look.font_pixels = 11.4F;
  CHECK(EntityBarFontPixels(look) == Catch::Approx(11.0F));
  look.font_pixels = 11.6F;
  CHECK(EntityBarFontPixels(look) == Catch::Approx(12.0F));

  // Le plancher est technique : zéro ferait diviser par zéro l'échelle d'ImGui.
  look.font_pixels = kSmallestEntityBarFont;
  CHECK(EntityBarFontPixels(look) >= kSmallestBakedFontPixels);
}

TEST_CASE("Mode réglage : une barre d'entité touche le vide et le plein", "[ui][barres]") {
  for (std::size_t rank = 0; rank < 8; ++rank) {
    for (const bool magic : {false, true}) {
      float lowest = 1.0F;
      float highest = 0.0F;
      for (int tick = 0; tick < 3'000; ++tick) {
        const EntityGauge gauge =
            ro::ui::TuningEntityGauge(static_cast<double>(tick) * 0.02, rank, magic);
        REQUIRE(gauge.known);
        const float fill = EntityGaugeFill(gauge);
        lowest = std::min(lowest, fill);
        highest = std::max(highest, fill);
      }
      CHECK(lowest < 0.01F);
      CHECK(highest > 0.99F);
    }
  }
  // Deux entités voisines ne battent pas ensemble.
  CHECK(ro::ui::TuningEntityGauge(1.0, 0, false).current !=
        ro::ui::TuningEntityGauge(1.0, 1, false).current);
}
