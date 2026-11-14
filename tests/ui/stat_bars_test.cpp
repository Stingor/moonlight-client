// Ce qu'une jauge libre ÉCRIT, et où elle bascule.
//
// 🔴 CHAQUE UNITÉ A SA RÈGLE, ET AUCUNE NE SE VOIT QUAND ELLE EST FAUSSE. Un
// poids multiplié par dix reste un nombre plausible ; une bourse écrite « 4210 /
// 2147483647 » est exacte et illisible ; un seuil de surcharge codé en dur
// s'accorde avec la configuration qu'on a sous la main et ment sur les autres.

#include "ro/ui/stat_bars.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

using ro::ui::OrbLevelY;
using ro::ui::OrbLiquidOf;
using ro::ui::OrbRimStroke;
using ro::ui::OrbShadowStroke;
using ro::ui::OrbSurface;
using ro::ui::OrbSurfaceOf;
using ro::ui::StatBar;
using ro::ui::StatBarCaption;
using ro::ui::StatBarFill;
using ro::ui::StatBarOverweight;
using ro::ui::StatBarPresent;
using ro::ui::StatBarShape;
using ro::ui::StatBarShapeOf;
using ro::ui::StatBarText;
using ro::ui::StatBarValue;
using ro::ui::StatBarWarnings;
using ro::ui::ZenyReferenceOf;

namespace {

[[nodiscard]] StatBarValue Told(std::int64_t current, std::int64_t most) {
  StatBarValue value;
  value.known = true;
  value.current = current;
  value.most = most;
  return value;
}

}  // namespace

TEST_CASE("Jauge libre : les trois modes de texte", "[ui][jauges]") {
  const StatBarValue health = Told(300, 1200);
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kNothing).empty());
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kPercent) == "25 %");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kValues) == "300 / 1200");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kBoth) == "300 / 1200 (25 %)");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kBothStacked) ==
        "300 / 1200\n(25 %)");
}

TEST_CASE("Jauge libre : le nom court précède le texte, et la bourse s'en passe", "[ui][jauges]") {
  // Le nom court en tête — « HP 25 % » — se règle et peut s'éteindre. ⚠ SANS
  // LE NOM, RIEN NE CHANGE : le reste du texte est exact au caractère près.
  const StatBarValue health = Told(300, 1200);
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kPercent, true) == "HP 25 %");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kValues, true) == "HP 300 / 1200");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kBoth, true) ==
        "HP 300 / 1200 (25 %)");
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kPercent, false) == "25 %");
  CHECK(StatBarCaption(StatBar::kMagic, health, StatBarText::kPercent, true) == "SP 25 %");
  CHECK(StatBarCaption(StatBar::kWeight, Told(4400, 8000), StatBarText::kValues, true) ==
        std::string(ro::ui::StatBarShortName(StatBar::kWeight)) + " 440 / 800");

  // 🔴 LA BOURSE N'A PAS DE NOM COURT : son « z » dit déjà ce qu'elle compte, et
  // « Zeny 1 234 z » le dirait deux fois.
  const StatBarValue purse = Told(1234, ro::ui::kDefaultZenyReference);
  CHECK(StatBarCaption(StatBar::kZeny, purse, StatBarText::kValues, true) ==
        StatBarCaption(StatBar::kZeny, purse, StatBarText::kValues, false));
  CHECK(std::string(ro::ui::StatBarShortName(StatBar::kZeny)).empty());
  // Et le silence reste le silence, nom ou pas.
  CHECK(StatBarCaption(StatBar::kHealth, StatBarValue{}, StatBarText::kBoth, true).empty());
  CHECK(StatBarCaption(StatBar::kHealth, health, StatBarText::kNothing, true).empty());
}

TEST_CASE("Jauge libre : le poids se dit en unités, pas en dixièmes", "[ui][jauges]") {
  // 🔴 LE SERVEUR STOCKE DES DIXIÈMES. Écrire la valeur brute multiplierait le
  // poids par dix sous les yeux du joueur — et « 4400 / 8000 » a l'air d'un
  // poids parfaitement ordinaire.
  const StatBarValue weight = Told(4400, 8000);
  CHECK(StatBarCaption(StatBar::kWeight, weight, StatBarText::kValues) == "440 / 800");
  // ⚠ LA PART, ELLE, NE SE DIVISE PAS : c'est un rapport, et les dixièmes se
  // simplifient. La rediviser donnerait 5 % au lieu de 55 %.
  CHECK(StatBarCaption(StatBar::kWeight, weight, StatBarText::kPercent) == "55 %");
}

TEST_CASE("Jauge libre : la bourse se groupe et ignore le mode", "[ui][jauges]") {
  // 🔴 UNE BOURSE N'A PAS DE FRACTION QUI VEUILLE DIRE QUELQUE CHOSE. Ce qu'on
  // veut savoir, c'est COMBIEN on a — pas où l'on en est du plafond d'un entier
  // signé.
  const StatBarValue purse = Told(1234567, ro::ui::kDefaultZenyReference);
  const std::string grouped = StatBarCaption(StatBar::kZeny, purse, StatBarText::kValues);
  CHECK(grouped.find('/') == std::string::npos);
  CHECK(grouped.find("2147483647") == std::string::npos);
  // ⚠ ET LE « z » EST LÀ, celui du pied du sac : un montant sans son unité se lit
  // comme un compte d'objets.
  CHECK(grouped.back() == 'z');
  // Le même texte quel que soit le mode — sauf « rien », qui reste rien.
  CHECK(StatBarCaption(StatBar::kZeny, purse, StatBarText::kPercent) == grouped);
  CHECK(StatBarCaption(StatBar::kZeny, purse, StatBarText::kBoth) == grouped);
  CHECK(StatBarCaption(StatBar::kZeny, purse, StatBarText::kNothing).empty());

  // 🔴 ET LA JAUGE SE REMPLIT CONTRE LE REPÈRE, NON CONTRE LE PLAFOND. Contre
  // `kZenyCeiling`, un million de zenys vaut un vingtième de pour cent : la barre
  // serait TOUJOURS vide, ce que la version précédente de cet essai constatait
  // sans y voir un défaut.
  CHECK(StatBarFill(purse) == 1.0F);
  CHECK(StatBarFill(Told(500000, ro::ui::kDefaultZenyReference)) == 0.5F);
  CHECK(StatBarFill(Told(1234567, ro::ui::kZenyCeiling)) < 0.01F);
}

TEST_CASE("Jauge libre : tant que le serveur n'a rien dit, elle se tait", "[ui][jauges]") {
  // ⚠ RIEN PLUTÔT QU'UN ZÉRO : « 0 / 0 » annoncerait une bourse vide ou une
  // mort — les deux faux, et les deux crédibles.
  StatBarValue silent;
  CHECK(StatBarCaption(StatBar::kHealth, silent, StatBarText::kBoth).empty());
  CHECK(StatBarCaption(StatBar::kZeny, silent, StatBarText::kValues).empty());
  CHECK(StatBarFill(silent) == 0.0F);
}

TEST_CASE("Jauge libre : le remplissage est borné", "[ui][jauges]") {
  CHECK(StatBarFill(Told(600, 1200)) == 0.5F);
  CHECK(StatBarFill(Told(0, 1200)) == 0.0F);
  CHECK(StatBarFill(Told(5000, 1200)) == 1.0F);
  CHECK(StatBarFill(Told(300, 0)) == 0.0F);
}

TEST_CASE("Jauge libre : la surcharge vient du SERVEUR, pas d'un nombre écrit ici",
          "[ui][jauges]") {
  StatBarWarnings warnings;
  const StatBarValue half = Told(4000, 8000);  // 50 %

  // 🔴 SANS SEUIL ANNONCÉ, ON NE COLORE RIEN. Un seuil codé en dur s'accorderait
  // avec la configuration qu'on a sous la main et mentirait sur toutes les
  // autres — et une jauge qui rougit à tort fait ranger des objets pour rien.
  CHECK_FALSE(StatBarOverweight(half, warnings));

  warnings.overweight_percent = 50;
  CHECK(StatBarOverweight(half, warnings));
  CHECK_FALSE(StatBarOverweight(Told(3900, 8000), warnings));

  // Un serveur plus permissif décale le seuil, et la jauge le suit.
  warnings.overweight_percent = 70;
  CHECK_FALSE(StatBarOverweight(half, warnings));
  CHECK(StatBarOverweight(Told(5600, 8000), warnings));

  // Et ce qu'on ne connaît pas ne rougit pas.
  CHECK_FALSE(StatBarOverweight(StatBarValue{}, warnings));
}

// ── LES FORMES ──────────────────────────────────────────────────────────────

TEST_CASE("Orbe : le disque s'inscrit dans le cadre, sur le PLUS PETIT côté", "[ui][jauges]") {
  // 🔴 LE PRENDRE SUR LA LARGEUR FERAIT DÉBORDER UN ORBE d'un cadre plus haut
  // que large, et la moitié du remplissage sortirait de ce que le joueur a posé.
  // Le défaut ne se voit pas sur un cadre carré — c'est-à-dire pas au moment où
  // on écrit le code.
  const OrbSurface wide = OrbSurfaceOf(ImVec2(100.0F, 200.0F), ImVec2(300.0F, 280.0F));
  CHECK(wide.radius == 40.0F);
  CHECK(wide.middle.x == 200.0F);
  CHECK(wide.middle.y == 240.0F);

  const OrbSurface tall = OrbSurfaceOf(ImVec2(100.0F, 200.0F), ImVec2(160.0F, 400.0F));
  CHECK(tall.radius == 30.0F);

  // Un cadre plat n'a pas d'orbe, et n'en invente pas un de rayon négatif.
  CHECK(OrbSurfaceOf(ImVec2(10.0F, 10.0F), ImVec2(10.0F, 40.0F)).radius == 0.0F);
}

TEST_CASE("Orbe : le liquide passe SOUS ce qui le couvre", "[ui][jauges]") {
  // 🔴 CE QUE LE JOUEUR A VU : un liseré saturé tout autour de l'orbe, que ni
  // l'ombre ni le filet n'effaçaient. La cause n'est pas dans nos rayons mais
  // dans la façon dont ImGui lisse — il prend son pixel de fondu DES DEUX CÔTÉS
  // d'un trait, et seulement AU-DEHORS d'un remplissage. Un disque rempli au
  // rayon R est donc opaque jusqu'à R, quand un filet censé le border ne l'est
  // plus dès R − 0,5.
  const OrbSurface disc = OrbSurfaceOf(ImVec2(0.0F, 0.0F), ImVec2(80.0F, 80.0F));
  REQUIRE(disc.radius == 40.0F);
  const OrbSurface liquid = OrbLiquidOf(disc);

  // Le liquide rentre, et d'un pixel exactement — pas plus, sinon l'orbe
  // maigrit à vue d'œil.
  CHECK(liquid.radius == disc.radius - ro::ui::kOrbEdgeInset);
  CHECK(liquid.middle.x == disc.middle.x);
  CHECK(liquid.middle.y == disc.middle.y);

  // 🔴 LA PROPRIÉTÉ QUI COMPTE : le bord OPAQUE du liquide passe sous le bord
  // OPAQUE du filet.
  const ro::ui::OrbStroke rim = OrbRimStroke(disc);
  CHECK(liquid.radius <= rim.opaque_outer());
  // ⚠ ET UN LIQUIDE QUI IRAIT JUSQU'À `disc` ÉCHOUERAIT ICI : le filet ne le
  // couvre PAS jusqu'au bout.
  CHECK(disc.radius > rim.opaque_outer());
  // Un trait ne rend opaque que son épaisseur MOINS le fondu.
  CHECK(rim.opaque_outer() < rim.center_radius + rim.thickness * 0.5F);

  // La première bande d'ombre est posée par son bord opaque : il tombe pile sur
  // le bord du liquide, et non un demi-pixel en deçà.
  const ro::ui::OrbStroke band = OrbShadowStroke(liquid, 0);
  CHECK(band.opaque_outer() == liquid.radius);
  // ⚠ CE QU'ON AURAIT ÉCRIT SANS Y PENSER — centrer la bande sur son milieu —
  // laissait justement ce demi-pixel.
  const ro::ui::OrbStroke naive{.center_radius = liquid.radius - band.thickness * 0.5F,
                                .thickness = band.thickness};
  CHECK(naive.opaque_outer() < liquid.radius);

  // Les bandes s'enfoncent, elles ne se superposent pas.
  CHECK(OrbShadowStroke(liquid, 1).center_radius < band.center_radius);

  // Un orbe minuscule ne se retourne pas : le liquide s'arrête à zéro.
  CHECK(OrbLiquidOf(OrbSurfaceOf(ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F))).radius == 0.0F);
}

namespace {

// UNE LISTE DE DESSIN SANS CONTEXTE IMGUI. ⚠ MÊME HARNAIS QUE LE BALAYAGE
// HORAIRE (`tests/ui/status_cell_test.cpp`) : ce qu'on veut mesurer, ce sont les
// SOMMETS produits, et ils ne demandent ni fenêtre ni GPU.
struct LoneDrawList {
  ImDrawListSharedData shared;
  ImDrawList into;

  LoneDrawList() : into(&shared) {
    shared.ClipRectFullscreen = ImVec4(0.0F, 0.0F, 4096.0F, 4096.0F);
    into._ResetForNewFrame();
    into.PushClipRect(ImVec2(0.0F, 0.0F), ImVec2(4096.0F, 4096.0F), false);
    into.PushTextureID(ImTextureRef());
  }
};

// Un sommet porte-t-il l'ENCRE ? Le fond de l'essai est gris neutre, l'encre est
// franchement rouge : un canal rouge qui domine les deux autres ne peut venir
// que d'elle, quelle que soit la nuance que le dégradé lui a donnée.
[[nodiscard]] bool Inked(const ImDrawVert& corner) {
  const auto red = static_cast<int>((corner.col >> IM_COL32_R_SHIFT) & 0xFFU);
  const auto green = static_cast<int>((corner.col >> IM_COL32_G_SHIFT) & 0xFFU);
  const auto blue = static_cast<int>((corner.col >> IM_COL32_B_SHIFT) & 0xFFU);
  const auto alpha = static_cast<int>((corner.col >> IM_COL32_A_SHIFT) & 0xFFU);
  return alpha > 0 && red > green + 40 && red > blue + 40;
}

// ⚠ CE QU'UN COSINUS COÛTE. Les sommets d'un disque se calculent en flottants
// simples : le plus éloigné tombe à 39,000011 et non à 39. La mesure se donne
// donc un vingtième de pixel — deux ordres de grandeur sous le pixel qu'elle
// sert à vérifier, donc sans rien laisser passer.
constexpr float kRoundingSlack = 0.05F;

[[nodiscard]] float DistanceFrom(ImVec2 middle, ImVec2 point) {
  const float dx = point.x - middle.x;
  const float dy = point.y - middle.y;
  return std::sqrt(dx * dx + dy * dy);
}

// Le sommet ENCRÉ le plus éloigné du centre.
[[nodiscard]] float FarthestInk(const LoneDrawList& sheet, ImVec2 middle) {
  float farthest = 0.0F;
  for (const ImDrawVert& corner : sheet.into.VtxBuffer) {
    if (Inked(corner)) {
      farthest = std::max(farthest, DistanceFrom(middle, corner.pos));
    }
  }
  return farthest;
}

}  // namespace

TEST_CASE("Orbe : aucune teinte vive ne sort du disque du liquide", "[ui][jauges]") {
  // 🔴 LA MESURE, ET NON LE RAISONNEMENT. Comparer des rayons ne prouve rien si
  // le dessin passe le disque PLEIN à l'une de ses couches — ce qui donnerait en
  // jeu un liseré saturé tout autour de l'orbe, que ni l'ombre ni le filet
  // n'effaceraient.
  const OrbSurface disc = OrbSurfaceOf(ImVec2(0.0F, 0.0F), ImVec2(80.0F, 80.0F));
  REQUIRE(disc.radius == 40.0F);
  const float liquid = OrbLiquidOf(disc).radius;

  // Plein, avec tout son relief : c'est le cas où le bord se voit le plus.
  LoneDrawList sheet;
  ro::ui::PaintStatusOrb(sheet.into, disc,
                         ro::ui::OrbPaint{.fill = 1.0F,
                                          .ink = IM_COL32(220, 40, 40, 255),
                                          .background = IM_COL32(20, 20, 20, 255),
                                          .border_color = IM_COL32(0, 0, 0, 160),
                                          .glossy = true,
                                          .border = false});

  // ⚠ LE FONDU D'UN REMPLISSAGE SORT D'UN DEMI-PIXEL du chemin, et ses sommets
  // sont à alpha nul : `Inked` les écarte. Ce qu'on borne ici, c'est la matière
  // OPAQUE.
  const float farthest = FarthestInk(sheet, disc.middle);
  CHECK(farthest > 0.0F);
  CHECK(farthest <= liquid + kRoundingSlack);
  // ⚠ ET SURTOUT : elle ne va PAS jusqu'au bord du disque.
  CHECK(farthest < disc.radius);

  // Le filet, lui, est opaque au-delà de ce bord-là : il le recouvre.
  CHECK(OrbRimStroke(disc).opaque_outer() >= farthest);
}

TEST_CASE("Orbe : un orbe à moitié plein ne déborde pas non plus", "[ui][jauges]") {
  // ⚠ LA CORDE EST UNE DROITE, et elle touche le bord du disque par ses deux
  // bouts : c'est l'autre endroit où de la couleur vive pouvait sortir.
  const OrbSurface disc = OrbSurfaceOf(ImVec2(10.0F, 10.0F), ImVec2(110.0F, 110.0F));
  const float liquid = OrbLiquidOf(disc).radius;
  for (const float part : {0.15F, 0.5F, 0.85F}) {
    LoneDrawList sheet;
    ro::ui::PaintStatusOrb(sheet.into, disc,
                           ro::ui::OrbPaint{.fill = part,
                                            .ink = IM_COL32(220, 40, 40, 255),
                                            .background = IM_COL32(20, 20, 20, 255),
                                            .border_color = IM_COL32(0, 0, 0, 160),
                                            .glossy = true,
                                            .border = true});
    INFO(part);
    const float farthest = FarthestInk(sheet, disc.middle);
    CHECK(farthest > 0.0F);
    CHECK(farthest <= liquid + kRoundingSlack);
  }
}

TEST_CASE("Orbe : le niveau monte quand la part monte", "[ui][jauges]") {
  // 🔴 LE SEUL SENS QUI SE LISE : un orbe se vide par le bas comme un verre.
  // Inversé, il resterait parfaitement crédible — plein quand on est mort.
  const OrbSurface disc = OrbSurfaceOf(ImVec2(0.0F, 0.0F), ImVec2(100.0F, 100.0F));
  REQUIRE(disc.radius == 50.0F);

  // Vide : le niveau est au BAS du disque.
  CHECK(OrbLevelY(disc, 0.0F) == 100.0F);
  // Plein : au sommet.
  CHECK(OrbLevelY(disc, 1.0F) == 0.0F);
  // À moitié : au centre.
  CHECK(OrbLevelY(disc, 0.5F) == 50.0F);
  // ⚠ ET IL MONTE, sans exception.
  CHECK(OrbLevelY(disc, 0.25F) > OrbLevelY(disc, 0.75F));

  // Hors bornes, il ne sort pas du disque.
  CHECK(OrbLevelY(disc, -1.0F) == 100.0F);
  CHECK(OrbLevelY(disc, 5.0F) == 0.0F);
}

TEST_CASE("Orbe : le niveau se rapporte au DISQUE, pas au cadre", "[ui][jauges]") {
  // ⚠ SUR UN CADRE PLUS HAUT QUE LARGE, les deux diffèrent — et c'est là que le
  // raccourci « moitié du cadre » laisse du remplissage hors du disque.
  const OrbSurface disc = OrbSurfaceOf(ImVec2(0.0F, 0.0F), ImVec2(100.0F, 300.0F));
  REQUIRE(disc.radius == 50.0F);
  CHECK(disc.middle.y == 150.0F);
  // Plein : le sommet du DISQUE (100), et non celui du cadre (0).
  CHECK(OrbLevelY(disc, 1.0F) == 100.0F);
  CHECK(OrbLevelY(disc, 0.0F) == 200.0F);
}

TEST_CASE("Bourse : le repère se borne AVANT de devenir un entier", "[ui][jauges]") {
  // 🔴 SANS CETTE BORNE, le curseur poussé à fond afficherait
  // « -2147483648 z ». Borner à `kZenyCeiling`, le plafond du serveur, ne
  // suffit pas : `2 147 483 647` n'est pas représentable en simple précision,
  // et le flottant le plus proche vaut UN DE TROP. `static_cast<int>` d'un
  // flottant hors des bornes d'un entier est un comportement indéfini, pas un
  // grand nombre.
  CHECK(static_cast<std::int64_t>(static_cast<float>(ro::ui::kLargestZenyReference)) ==
        ro::ui::kLargestZenyReference);
  CHECK(ro::ui::kLargestZenyReference < ro::ui::kZenyCeiling);

  // La borne haute rend exactement elle-même, aller-retour compris.
  CHECK(ZenyReferenceOf(static_cast<float>(ro::ui::kLargestZenyReference)) ==
        ro::ui::kLargestZenyReference);
  // ⚠ ET CE QUI LA DÉPASSE N'EST PAS INDÉFINI : un fichier de réglages écrit à
  // la main passe par le même chemin qu'un curseur.
  CHECK(ZenyReferenceOf(1.0e12F) == ro::ui::kLargestZenyReference);
  CHECK(ZenyReferenceOf(-1.0e12F) == ro::ui::kSmallestZenyReference);
  CHECK(ZenyReferenceOf(0.0F) == ro::ui::kSmallestZenyReference);
  CHECK(ZenyReferenceOf(static_cast<float>(ro::ui::kDefaultZenyReference)) ==
        ro::ui::kDefaultZenyReference);
}

TEST_CASE("Style : le geste de recopie emporte la MANIÈRE, jamais l'IDENTITÉ", "[ui][jauges]") {
  // 🔴 CE QUI DISTINGUE LES SIX NE SE RECOPIE PAS. Recopier la forme mettrait six
  // orbes d'un clic — ce qui détruit exactement la disposition que les formes
  // servent à bâtir ; recopier la couleur obligerait à lire un chiffre pour
  // savoir quelle jauge on regarde. Une affectation entière ferait les deux, et
  // elle est plausible : c'est une ligne au lieu de onze.
  std::array<ro::ui::StatBarLook, ro::ui::kStatBarCount> bars = ro::ui::DefaultStatBars();

  auto& model = bars[0];
  model.shape = StatBarShape::kOrb;
  model.fill = IM_COL32(1, 2, 3, 255);
  model.reversed = true;
  model.shown = false;
  model.rect.x = 640.0F;
  model.text = StatBarText::kBoth;
  model.font_pixels = 21.0F;
  model.rounding = 9.0F;
  model.border = false;
  model.glossy = false;
  model.label = false;
  model.vertical = true;
  model.ring_thickness = 0.4F;
  model.background = IM_COL32(9, 9, 9, 200);
  model.text_color = IM_COL32(8, 8, 8, 255);
  model.border_color = IM_COL32(7, 7, 7, 255);

  const ro::ui::StatBarLook before = bars[3];
  ro::ui::CopyStatBarStyle(bars, 0);
  const ro::ui::StatBarLook& other = bars[3];

  // La manière est partie.
  CHECK(other.text == StatBarText::kBoth);
  CHECK(other.font_pixels == 21.0F);
  CHECK(other.rounding == 9.0F);
  CHECK(other.border == false);
  CHECK(other.glossy == false);
  CHECK(other.label == false);
  CHECK(other.vertical == true);
  CHECK(other.ring_thickness == 0.4F);
  CHECK(other.background == IM_COL32(9, 9, 9, 200));
  CHECK(other.text_color == IM_COL32(8, 8, 8, 255));
  CHECK(other.border_color == IM_COL32(7, 7, 7, 255));

  // ⚠ ET L'IDENTITÉ EST RESTÉE, en entier.
  CHECK(other.shape == before.shape);
  CHECK(other.fill == before.fill);
  CHECK(other.reversed == before.reversed);
  CHECK(other.shown == before.shown);
  CHECK(other.rect.x == before.rect.x);
  CHECK(other.rect.y == before.rect.y);

  // Le modèle ne se recopie pas sur lui-même, et un rang hors bornes ne touche
  // à rien.
  CHECK(bars[0].fill == IM_COL32(1, 2, 3, 255));
  const auto untouched = bars;
  ro::ui::CopyStatBarStyle(bars, ro::ui::kStatBarCount);
  CHECK(bars[2].text == untouched[2].text);
}

TEST_CASE("Style : chaque jauge a le sien, et les six naissent pareilles", "[ui][jauges]") {
  // 🔴 C'EST TOUT L'OBJET DU DÉPLACEMENT. Une apparence commune interdisait la
  // disposition pour laquelle les formes existent : deux orbes MUETS encadrant
  // des barres qui écrivent leurs valeurs ne se règlent pas avec les mêmes
  // nombres.
  std::array<ro::ui::StatBarLook, ro::ui::kStatBarCount> bars = ro::ui::DefaultStatBars();
  bars[0].text = StatBarText::kNothing;
  CHECK(bars[1].text == StatBarText::kPercent);

  // ⚠ ET AU DÉPART ELLES SE RESSEMBLENT : le granulaire ne doit pas se payer
  // d'un écran bariolé à la première ouverture.
  for (const ro::ui::StatBarLook& bar : ro::ui::DefaultStatBars()) {
    CHECK(bar.text == StatBarText::kPercent);
    CHECK(bar.font_pixels == 13.0F);
    CHECK(bar.rounding == 4.0F);
    CHECK(bar.border);
    CHECK(bar.glossy);
    CHECK(bar.label);
    CHECK_FALSE(bar.vertical);
    CHECK_FALSE(bar.reversed);
    CHECK(bar.shape == StatBarShape::kBar);
  }
}

TEST_CASE("Forme : une valeur hors bornes retombe sur la barre", "[ui][jauges]") {
  // ⚠ LE FICHIER DE RÉGLAGES N'EST PAS SOUS NOTRE CONTRÔLE, et un `static_cast`
  // sur une énumération hors bornes n'est pas une valeur bizarre, c'est un
  // comportement indéfini.
  CHECK(StatBarShapeOf(0) == StatBarShape::kBar);
  CHECK(StatBarShapeOf(1) == StatBarShape::kOrb);
  CHECK(StatBarShapeOf(2) == StatBarShape::kRing);
  CHECK(StatBarShapeOf(-1) == StatBarShape::kBar);
  CHECK(StatBarShapeOf(99) == StatBarShape::kBar);
}

TEST_CASE("Incantation : le texte composé passe devant le mode", "[ui][jauges][incantation]") {
  StatBarValue cast = Told(600, 2'000);
  cast.composed = "Storm Gust  1,4 s";
  CHECK(StatBarCaption(StatBar::kCast, cast, StatBarText::kPercent, true) == "Storm Gust  1,4 s");
  CHECK(StatBarCaption(StatBar::kCast, cast, StatBarText::kBoth) == "Storm Gust  1,4 s");
  // ⚠ LE MODE DÉCIDE ENCORE S'IL S'ÉCRIT : muette, la jauge reste muette.
  CHECK(StatBarCaption(StatBar::kCast, cast, StatBarText::kNothing).empty());
}

TEST_CASE("Incantation : la jauge n'est là que pendant le sort", "[ui][jauges][incantation]") {
  CHECK_FALSE(StatBarPresent(StatBar::kCast, StatBarValue{}));
  CHECK(StatBarPresent(StatBar::kCast, Told(600, 2'000)));
  // ⚠ LES AUTRES RESTENT, même inconnues : c'est leur texte qui se tait.
  CHECK(StatBarPresent(StatBar::kZeny, StatBarValue{}));
}

TEST_CASE("Expérience : la jauge s'efface au niveau maximum, sauf si on la garde", "[ui][jauges]") {
  using ro::ui::kBaseExpCeiling;
  using ro::ui::kJobExpCeiling;
  // Le seuil du niveau suivant est le plafond : niveau maximum, la jauge part,
  // quelle que soit l'expérience accumulée.
  CHECK_FALSE(StatBarPresent(StatBar::kBaseExp, Told(0, kBaseExpCeiling)));
  CHECK_FALSE(StatBarPresent(StatBar::kBaseExp, Told(kBaseExpCeiling, kBaseExpCeiling)));
  CHECK_FALSE(StatBarPresent(StatBar::kJobExp, Told(12'345, kJobExpCeiling)));
  // Gardée par le réglage, elle reste.
  CHECK(StatBarPresent(StatBar::kBaseExp, Told(0, kBaseExpCeiling), true));
  CHECK(StatBarPresent(StatBar::kJobExp, Told(12'345, kJobExpCeiling), true));
  // Un niveau ordinaire, même plein, n'est pas le niveau maximum : elle reste.
  CHECK(StatBarPresent(StatBar::kBaseExp, Told(5'000, 5'000)));
  // Chaque jauge a SON plafond : celui du métier n'efface pas la base.
  CHECK(StatBarPresent(StatBar::kBaseExp, Told(0, kJobExpCeiling)));
  // Inconnue, elle reste : c'est son texte qui se tait.
  CHECK(StatBarPresent(StatBar::kJobExp, StatBarValue{}));
}

TEST_CASE("Incantation : sa place d'origine, sous le poids", "[ui][jauges][incantation]") {
  const auto bars = ro::ui::DefaultStatBars();
  const ro::ui::StatBarLook& weight = bars[static_cast<std::size_t>(StatBar::kWeight)];
  const ro::ui::StatBarLook& cast = bars[static_cast<std::size_t>(StatBar::kCast)];
  CHECK(cast.rect.x == weight.rect.x);
  CHECK(cast.rect.y == weight.rect.y + 18.0F);
  CHECK(cast.rect.width == weight.rect.width);
  CHECK(cast.fill == IM_COL32(107, 173, 255, 255));
}

TEST_CASE("Texte : son décalage se compte en tailles de jauge, borné", "[ui][jauges]") {
  ro::ui::StatBarLook look;
  const ImVec2 size(200.0F, 20.0F);
  // Zéro : la disposition d'origine, texte au centre.
  ImVec2 shift = ro::ui::StatBarTextShift(look, size);
  CHECK(shift.x == 0.0F);
  CHECK(shift.y == 0.0F);
  // Une demi-largeur à droite, une hauteur vers le haut : le texte suit la taille.
  look.text_shift_x = 0.5F;
  look.text_shift_y = -1.0F;
  shift = ro::ui::StatBarTextShift(look, size);
  CHECK(shift.x == 100.0F);
  CHECK(shift.y == -20.0F);
  // Un fichier écrit à la main ne pousse pas le texte au-delà de deux tailles.
  look.text_shift_x = 9.0F;
  look.text_shift_y = -9.0F;
  shift = ro::ui::StatBarTextShift(look, size);
  CHECK(shift.x == ro::ui::kLargestTextShiftShare * size.x);
  CHECK(shift.y == -ro::ui::kLargestTextShiftShare * size.y);
}

TEST_CASE("Texte : son décalage reste à chaque jauge quand le style se recopie", "[ui][jauges]") {
  std::array<ro::ui::StatBarLook, ro::ui::kStatBarCount> bars = ro::ui::DefaultStatBars();
  bars[0].text_shift_x = 1.0F;
  bars[0].text_shift_y = -0.5F;
  ro::ui::CopyStatBarStyle(bars, 0);
  CHECK(bars[1].text_shift_x == 0.0F);
  CHECK(bars[1].text_shift_y == 0.0F);
}
