#include "ro/render/option_effect_table.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"

// Les effets qu'un ÉTAT entretient. Ceux-là n'arrivent par aucun paquet : le
// client resème l'effet ENTIER à partir du seul bit d'option de l'acteur, et
// c'est pour cela qu'ils manquaient tant qu'on ne cherchait que du côté des
// compétences.
using ro::render::OptionEffect;
using ro::render::OptionEffectTable;

TEST_CASE("Effets d'état : rien d'incomplet n'y entre", "[render][optioneffect]") {
  const std::span<const OptionEffect> table = OptionEffectTable();
  REQUIRE(!table.empty());
  for (const OptionEffect& state : table) {
    // Un masque nul s'allumerait pour tout le monde, et jamais ne s'éteindrait.
    CHECK(state.option_mask != 0);
    CHECK(state.effect_id != 0);
    // Une cadence nulle resèmerait l'effet à chaque tic : c'est ce que le
    // client ferait d'une durée absente, et aucune des deux ne l'est.
    CHECK(state.respawn_ticks > 0);
  }
}

TEST_CASE("Effets d'état : Sight et Ruwach renaissent à la durée déclarée de leur effet",
          "[render][optioneffect]") {
  const std::span<const OptionEffect> table = OptionEffectTable();
  const auto sight =
      std::ranges::find_if(table, [](const OptionEffect& state) { return state.effect_id == 22; });
  REQUIRE(sight != table.end());
  // OPTION_SIGHT, que le client teste par « option & 1 ».
  CHECK(sight->option_mask == 0x1U);
  // 🔴 LE VRAI EFFET 22, TOUS LES DEUX CENT SOIXANTE-DIX TICS : c'est la durée
  // que la table du client déclare pour lui, et que le compteur de l'acteur
  // atteint avant de le faire renaître — et non un sprite toutes les cent vingt
  // millisecondes.
  CHECK(sight->respawn_ticks == 270);

  const auto ruwach =
      std::ranges::find_if(table, [](const OptionEffect& state) { return state.effect_id == 33; });
  REQUIRE(ruwach != table.end());
  // OPTION_RUWACH, que le client teste par « option & 0x2000 ».
  CHECK(ruwach->option_mask == 0x2000U);
  CHECK(ruwach->respawn_ticks == 200);
}

TEST_CASE("Effets d'état : Sight et Ruwach sonnent au front MONTANT, du même fichier",
          "[render][optioneffect]") {
  // 🔴 LE BRUIT NE VIENT PAS DU RESEMIS. À la réception d'un changement
  // d'option, le client le joue à l'instant où le bit s'allume, dans deux
  // branches jumelles : « Sight vient de changer, et il est posé », puis la
  // même chose pour Ruwach. Aucune table de sons d'effet ni de compétence ne le
  // porte : sans cette règle, MG_SIGHT reste muet.
  const std::span<const OptionEffect> table = OptionEffectTable();
  const auto sight =
      std::ranges::find_if(table, [](const OptionEffect& state) { return state.effect_id == 22; });
  const auto ruwach =
      std::ranges::find_if(table, [](const OptionEffect& state) { return state.effect_id == 33; });
  REQUIRE(sight != table.end());
  REQUIRE(ruwach != table.end());
  CHECK(sight->rise_sound == "effect\\EF_Sight.wav");
  // ⚠ RUWACH JOUE LE MÊME FICHIER, et ce n'est pas une copie distraite : les
  // deux branches nomment la même chaîne.
  CHECK(ruwach->rise_sound == sight->rise_sound);
}

TEST_CASE("Effets d'état : un compteur par acteur, que Sight et Ruwach se partagent",
          "[render][optioneffect]") {
  using ro::render::OptionEffectDue;
  constexpr std::uint32_t kSight = 0x1;
  constexpr std::uint32_t kRuwach = 0x2000;
  constexpr std::uint32_t kTickMs = ro::render::kEffectTickMs;
  // Un compteur au plus haut — rien de semé, ou un mot d'options passé par
  // zéro — fait naître l'effet aussitôt.
  REQUIRE(OptionEffectDue(kRuwach, std::nullopt) != nullptr);
  CHECK(OptionEffectDue(kRuwach, std::nullopt)->effect_id == 33);
  CHECK(OptionEffectDue(kRuwach, 199 * kTickMs) == nullptr);
  REQUIRE(OptionEffectDue(kRuwach, 200 * kTickMs) != nullptr);
  CHECK(OptionEffectDue(0, std::nullopt) == nullptr);
  // 🔴 SIGHT SE TESTE D'ABORD, puis Ruwach sur LE MÊME compteur : à deux cents
  // tics, Sight n'est pas mûr et c'est Ruwach qui naît et remet le compte à
  // zéro — Sight ne l'atteindra plus tant que Ruwach dure.
  REQUIRE(OptionEffectDue(kSight | kRuwach, 200 * kTickMs) != nullptr);
  CHECK(OptionEffectDue(kSight | kRuwach, 200 * kTickMs)->effect_id == 33);
  REQUIRE(OptionEffectDue(kSight | kRuwach, std::nullopt) != nullptr);
  CHECK(OptionEffectDue(kSight | kRuwach, std::nullopt)->effect_id == 22);
}

TEST_CASE("États : Hiding fume à l'entrée et à la sortie, l'effet 16 n'est qu'à l'entrée",
          "[render][option]") {
  // Le joueur, en côte à côte : l'effet 16 s'entendait aussi à la sortie, et la
  // fumée manquait des deux côtés.
  using ro::render::HideTransitionLookFor;
  constexpr std::uint16_t kThief = 6;
  CHECK(HideTransitionLookFor(true, kThief).smoke_puff);
  CHECK(HideTransitionLookFor(true, kThief).effect_id == 16);
  CHECK(HideTransitionLookFor(false, kThief).smoke_puff);
  CHECK(HideTransitionLookFor(false, kThief).effect_id == 0);
  // Le ninja et le bébé ninja n'ont que la fumée.
  CHECK(HideTransitionLookFor(true, ro::render::kJobNinja).effect_id == 0);
  CHECK(HideTransitionLookFor(true, ro::render::kJobBabyNinja).effect_id == 0);
  CHECK(HideTransitionLookFor(true, ro::render::kJobNinja).smoke_puff);
}
