#include "ro/render/item_drop.h"

#include <cstdint>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::render::ItemArcAt;
using ro::render::ItemArcIsOver;
using ro::render::ItemDropPillarEffectId;
using ro::render::ItemDropPillarOf;
using ro::render::ItemDropSettings;
using ro::render::ItemFallIsOver;
using ro::render::ItemFallUpUnits;
using ro::render::ItemSparklesAt;

namespace {

// Un tic du client, en millisecondes entières.
constexpr std::uint32_t kTickMs = 24;

// De combien l'objet naît au-dessus de sa case : le « sol moins 15.0 » que le
// client écrit à la naissance de l'objet. ⚠ ÉCRIT EN TOUTES LETTRES, comme la
// période du scintillement plus bas, pour que l'essai morde sur la valeur.
constexpr float kBirthUnits = 15.0F;

// L'instant, en millisecondes, où la parabole du client repasse sous le sol :
// 0.083 t² - 0.6 t - 15 = 0 donne t ≈ 17.53 tics.
constexpr std::uint32_t kLandsAtMs = 421;

// Le sommet de la parabole : t = 0.6 / (2 × 0.083) ≈ 3.61 tics.
constexpr std::uint32_t kApexMs = 87;

// De combien le butin part au-dessus des pieds de son lâcheur, en unités du
// monde. La valeur vient de l'APPELANT — la taille du sprite, qui n'est pas
// la même pour un Poring et pour un Baphomet.
constexpr float kBodyUpUnits = 6.4F;

// 🔴 LE SCINTILLEMENT, ÉCRIT EN TOUTES LETTRES ET NON REPRIS DES CONSTANTES.
// `(int)t % 92 >= 90` est ce que le client exécute à chaque tic de chute ; un
// essai qui relirait `kItemSparklePeriodTicks` dirait la même chose que le code
// quelle que soit sa valeur, et ne mordrait sur rien. Le sabotage l'a montré :
// la période changée de 90 à 89, l'essai passait toujours.
constexpr std::uint32_t kSparklePeriodTicks = 92;
constexpr std::uint32_t kSparkleFirstLitTick = 90;

// Le tic du client vaut 24 ms : une période fait donc 2208 ms, dont 48 allumées.
constexpr std::uint32_t kSparklePeriodMs = kSparklePeriodTicks * kTickMs;

}  // namespace

TEST_CASE("Chute : l'objet naît quinze unités en l'air", "[render][item_drop]") {
  // 🔴 C'EST CE QUI DISTINGUE UN BUTIN D'UN DÉCOR. Le client pose la hauteur à
  // « sol moins quinze » à la naissance de l'objet ; naître au sol ferait
  // apparaître l'objet sans qu'il tombe.
  CHECK(ItemFallUpUnits(0) == Approx(kBirthUnits));
  CHECK_FALSE(ItemFallIsOver(0));
}

TEST_CASE("Chute : l'objet S'ÉLÈVE d'abord, puis retombe", "[render][item_drop]") {
  // ⚠ LA VITESSE INITIALE EST DIRIGÉE VERS LE HAUT. Chez le client l'ordonnée
  // descend et la constante vaut -0.6 : lue à l'endroit, elle ferait plonger
  // l'objet dès la première image au lieu de le laisser monter d'une unité.
  CHECK(ItemFallUpUnits(kTickMs) > kBirthUnits);
  CHECK(ItemFallUpUnits(kApexMs) > ItemFallUpUnits(0));
  CHECK(ItemFallUpUnits(kApexMs) > ItemFallUpUnits(2 * kApexMs));
  // Le sommet est PLAT : de part et d'autre, la hauteur est moindre.
  CHECK(ItemFallUpUnits(kApexMs) >= ItemFallUpUnits(kApexMs - kTickMs));
  CHECK(ItemFallUpUnits(kApexMs) >= ItemFallUpUnits(kApexMs + kTickMs));
}

TEST_CASE("Chute : l'objet se pose en quelque quatre cents millisecondes", "[render][item_drop]") {
  CHECK_FALSE(ItemFallIsOver(kLandsAtMs - kTickMs));
  CHECK(ItemFallUpUnits(kLandsAtMs - kTickMs) > 0.0F);
  CHECK(ItemFallIsOver(kLandsAtMs));
  CHECK(ItemFallUpUnits(kLandsAtMs) == 0.0F);
}

TEST_CASE("Chute : posé, l'objet ne s'enfonce pas", "[render][item_drop]") {
  // ⚠ LA PARABOLE CONTINUE DE DESCENDRE APRÈS LE SOL. Sans la borne, un objet
  // posé depuis dix secondes serait à deux cents unités sous la carte — le
  // client, lui, éteint sa chute dès qu'elle passe le terrain.
  CHECK(ItemFallUpUnits(2 * kLandsAtMs) == 0.0F);
  CHECK(ItemFallUpUnits(60000) == 0.0F);
}

TEST_CASE("Scintillement : deux tics allumés sur quatre-vingt-douze",
          "[render][item_drop][sparkle]") {
  std::uint32_t lit = 0;
  for (std::uint32_t tick = 0; tick < kSparklePeriodTicks; ++tick) {
    if (ItemSparklesAt(tick * kTickMs)) {
      ++lit;
    }
  }
  CHECK(lit == 2);
  // L'éclat est aux DEUX DERNIERS tics de la période, et nulle part ailleurs.
  CHECK_FALSE(ItemSparklesAt(0));
  CHECK_FALSE(ItemSparklesAt((kSparkleFirstLitTick - 1) * kTickMs));
  CHECK(ItemSparklesAt(kSparkleFirstLitTick * kTickMs));
  CHECK(ItemSparklesAt((kSparkleFirstLitTick + 1) * kTickMs));
  CHECK_FALSE(ItemSparklesAt(kSparklePeriodTicks * kTickMs));
}

TEST_CASE("Scintillement : la période NE DIT RIEN de la durée de vie",
          "[render][item_drop][sparkle]") {
  // 🔴 CE N'EST PAS UN COMPTE À REBOURS. Le client ne connaît pas l'échéance de
  // l'objet — `flooritem_lifetime` est un réglage du serveur, que rien
  // n'annonce sur le fil —, et son éclat bat au même rythme à la première
  // seconde qu'à la dernière. Une lecture qui y verrait une fin de vie
  // ferait ramasser en urgence un butin qui a encore une minute devant lui.
  for (std::uint32_t turn = 0; turn < 30; ++turn) {
    const std::uint32_t base_ms = turn * kSparklePeriodMs;
    CHECK_FALSE(ItemSparklesAt(base_ms));
    CHECK(ItemSparklesAt(base_ms + kSparkleFirstLitTick * kTickMs));
  }
}

TEST_CASE("Jaillissement : l'objet part du CORPS et finit sur sa case",
          "[render][item_drop][arc]") {
  const ItemDropSettings settings;
  CHECK(ItemArcAt(settings, 0, kBodyUpUnits).advance == Approx(0.0F));
  // 🔴 IL PART À MI-HAUTEUR DU PORTEUR, ET NON DE SES PIEDS. Parti de la
  // position de l'entité — le sol —, le butin aurait l'air de sortir de
  // terre. Le client d'origine fait pire : il le lâche au-dessus de
  // sa CASE, sans rien savoir de qui l'a lâché.
  CHECK(ItemArcAt(settings, 0, kBodyUpUnits).up_units == Approx(kBodyUpUnits));
  CHECK_FALSE(ItemArcAt(settings, 0, kBodyUpUnits).over);

  // À l'atterrissage, l'avance vaut un : l'objet est exactement sur la case que
  // le serveur lui a donnée, et c'est elle qu'on ramasse. ⚠ ET LA HAUTEUR DU
  // CORPS A DISPARU — sans quoi le butin resterait en l'air.
  CHECK(ItemArcAt(settings, settings.duration_ms, kBodyUpUnits).advance == Approx(1.0F));
  CHECK(ItemArcAt(settings, settings.duration_ms, kBodyUpUnits).up_units == Approx(0.0F));
}

TEST_CASE("Jaillissement : la hauteur du corps s'efface au fil de l'avance",
          "[render][item_drop][arc]") {
  // Elle vaut exactement la part de chemin QUI RESTE, et elle ne dépend pas de
  // la crête : les deux s'additionnent.
  ItemDropSettings flat;
  flat.height_units = 0.0F;
  constexpr float kTall = 10.0F;
  CHECK(ItemArcAt(flat, 0, kTall).up_units == Approx(kTall));
  const auto half_ms = flat.duration_ms / 2;
  // À mi-temps l'avance vaut trois quarts : il reste un quart de la hauteur.
  CHECK(ItemArcAt(flat, half_ms, kTall).up_units == Approx(2.5F).margin(0.05F));
  CHECK(ItemArcAt(flat, flat.duration_ms, kTall).up_units == Approx(0.0F));

  // À zéro, l'objet part du sol.
  CHECK(ItemArcAt(flat, 0, 0.0F).up_units == Approx(0.0F));
}

TEST_CASE("Jaillissement : la crête est atteinte à mi-course", "[render][item_drop][arc]") {
  const ItemDropSettings settings;
  const auto half_ms = settings.duration_ms / 2;
  // ⚠ LA CRÊTE S'ADDITIONNE À CE QU'IL RESTE DE LA HAUTEUR DU CORPS : à
  // mi-temps l'avance vaut trois quarts, donc un quart de celle-ci demeure.
  const float remaining = kBodyUpUnits / 4.0F;
  CHECK(ItemArcAt(settings, half_ms, kBodyUpUnits).up_units ==
        Approx(settings.height_units + remaining).margin(0.2F));
  // L'horizontale DÉCÉLÈRE : à mi-temps, l'objet a déjà fait les trois quarts
  // du chemin. Une avance linéaire donnerait la moitié.
  CHECK(ItemArcAt(settings, half_ms, kBodyUpUnits).advance == Approx(0.75F).margin(0.01F));
}

TEST_CASE("Jaillissement : deux rebonds, de plus en plus courts", "[render][item_drop][arc]") {
  const ItemDropSettings settings;
  const auto flight_ms = settings.duration_ms;
  const auto first_ms = static_cast<std::uint32_t>(static_cast<float>(flight_ms) * 0.32F);
  const auto second_ms = static_cast<std::uint32_t>(static_cast<float>(flight_ms) * 0.18F);

  const float first_peak = ItemArcAt(settings, flight_ms + first_ms / 2, kBodyUpUnits).up_units;
  const float second_peak =
      ItemArcAt(settings, flight_ms + first_ms + second_ms / 2, kBodyUpUnits).up_units;
  CHECK(first_peak > 0.0F);
  CHECK(second_peak > 0.0F);
  CHECK(first_peak > second_peak);
  // Les rebonds se font SUR PLACE : l'objet ne s'écarte plus de sa case.
  CHECK(ItemArcAt(settings, flight_ms + first_ms / 2, kBodyUpUnits).advance == Approx(1.0F));

  CHECK_FALSE(ItemArcAt(settings, flight_ms + first_ms + second_ms - 1, kBodyUpUnits).over);
  CHECK(ItemArcAt(settings, flight_ms + first_ms + second_ms, kBodyUpUnits).over);
  CHECK(ItemArcAt(settings, flight_ms + first_ms + second_ms, kBodyUpUnits).up_units ==
        Approx(0.0F));
}

TEST_CASE("Jaillissement : une durée réglée trop courte tient quand même",
          "[render][item_drop][arc]") {
  // Le curseur ne descend pas sous le plancher, mais un fichier de réglages
  // écrit à la main, lui, peut poser n'importe quoi : une durée nulle ferait
  // diviser par zéro.
  ItemDropSettings settings;
  settings.duration_ms = 0;
  CHECK(ItemArcAt(settings, 0, kBodyUpUnits).advance == Approx(0.0F));
  CHECK(ItemArcAt(settings, 10, kBodyUpUnits).up_units > 0.0F);
  CHECK(ItemArcAt(settings, 10000, kBodyUpUnits).over);
}

TEST_CASE("Pilier : le mode du serveur donne l'identifiant du client",
          "[render][item_drop][pillar]") {
  // 🔴 LES CINQ NOMBRES SONT CEUX DU CLIENT À LA NAISSANCE DE L'OBJET, écrits
  // ici en toutes lettres : un essai qui relirait les constantes de la table
  // passerait quelle que soit leur valeur.
  CHECK(ItemDropPillarEffectId(1) == 1869);
  CHECK(ItemDropPillarEffectId(2) == 1189);
  CHECK(ItemDropPillarEffectId(3) == 1190);
  CHECK(ItemDropPillarEffectId(4) == 1870);
  CHECK(ItemDropPillarEffectId(5) == 1871);
  // Hors de un à cinq, le client ne plante rien : sa garde
  // `(uint16)(mode - 1) <= 4` referme le chemin, et le cas par défaut du switch
  // est mort.
  CHECK(ItemDropPillarEffectId(0) == 0);
  CHECK(ItemDropPillarEffectId(6) == 0);
  CHECK(ItemDropPillarEffectId(65535) == 0);
}

TEST_CASE("Pilier : chaque mode charge SA couleur", "[render][item_drop][pillar]") {
  // Les cinq couleurs du client, dans l'ordre des modes. Elles s'accordent
  // aux noms que le serveur emploie SOUS SON PACKETVER — `e_item_drop_effect`
  // est découpée par des `#if PACKETVER < 20200304`, et la lire sans ses gardes
  // donne cinq noms décalés d'un cran. Le décalage ne se verrait pas : un pilier
  // paraîtrait quand même, d'une autre couleur. C'est pourquoi cet essai
  // s'appuie sur les NOMBRES du client et non sur les noms du serveur.
  CHECK(ItemDropPillarOf(ItemDropPillarEffectId(1)).top.find("blue") != std::string_view::npos);
  CHECK(ItemDropPillarOf(ItemDropPillarEffectId(2)).top.find("yellow") != std::string_view::npos);
  CHECK(ItemDropPillarOf(ItemDropPillarEffectId(3)).top.find("purple") != std::string_view::npos);
  CHECK(ItemDropPillarOf(ItemDropPillarEffectId(4)).top.find("green") != std::string_view::npos);
  CHECK(ItemDropPillarOf(ItemDropPillarEffectId(5)).top.find("red") != std::string_view::npos);
}

TEST_CASE("Pilier : deux fichiers et un son, et le rose par défaut",
          "[render][item_drop][pillar]") {
  const ro::render::ItemDropPillar blue = ItemDropPillarOf(1869);
  // ⚠ LA FLAQUE AU PIED EST UN SECOND FICHIER, et non une variante du premier :
  // n'en jouer qu'un laisserait la colonne flotter sur un sol nu.
  CHECK(blue.top == "new_dropitem\\dropitem_blue\\dropitem_blue\\dropitem_blue.str");
  CHECK(blue.bottom ==
        "new_dropitem\\dropitem_blue\\dropitem_blue_bottom\\dropitem_blue_bottom.str");
  CHECK(blue.sound == "effect\\drop_blue.wav");

  // Le défaut de la classe du client, qu'aucun mode n'atteint par un butin.
  CHECK(ItemDropPillarOf(0).top.find("pink") != std::string_view::npos);
  CHECK(ItemDropPillarOf(1186).sound == "effect\\drop_pink.wav");
}

TEST_CASE("Jaillissement : la fin se lit SANS le départ", "[render][item_drop][arc]") {
  // 🔴 C'EST CE QUI ROMPT UN CYCLE. Lire la fin dans `ItemArcAt` obligeait à
  // connaître d'abord d'où le butin était parti ; or le départ s'EFFACE à la fin
  // du vol, et le relire le recréait à l'image suivante, pour l'effacer encore.
  // Chaque butin posé cherchait ainsi son lâcheur soixante fois par seconde.
  const ItemDropSettings settings;
  const auto flight_ms = settings.duration_ms;
  const auto first_ms = static_cast<std::uint32_t>(static_cast<float>(flight_ms) * 0.32F);
  const auto second_ms = static_cast<std::uint32_t>(static_cast<float>(flight_ms) * 0.18F);
  const std::uint32_t whole_ms = flight_ms + first_ms + second_ms;

  CHECK_FALSE(ItemArcIsOver(settings, 0));
  CHECK_FALSE(ItemArcIsOver(settings, flight_ms));
  CHECK_FALSE(ItemArcIsOver(settings, whole_ms - 1));
  CHECK(ItemArcIsOver(settings, whole_ms));
  CHECK(ItemArcIsOver(settings, whole_ms * 10));

  // ⚠ ET LES DEUX DISENT LA MÊME CHOSE, quelle que soit la hauteur de départ :
  // une fin qui divergerait de l'arc laisserait un butin figé en l'air.
  for (std::uint32_t at_ms = 0; at_ms <= whole_ms + 40; at_ms += 7) {
    CHECK(ItemArcIsOver(settings, at_ms) == ItemArcAt(settings, at_ms, kBodyUpUnits).over);
    CHECK(ItemArcIsOver(settings, at_ms) == ItemArcAt(settings, at_ms, 0.0F).over);
  }
}
