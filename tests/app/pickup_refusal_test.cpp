#include "items/pickup_refusal.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_words.h"

namespace {

using moonclient::PickupRefusalOf;
using ro::vfs::ItemPickupRefusal;

}  // namespace

TEST_CASE("Refus : chaque code que ce serveur émet trouve SON message") {
  // ⚠ LES NUMÉROS SE RÉÉCRIVENT EN TOUTES LETTRES, sans relire l'énumération
  // qu'ils vérifient : un essai qui la relit suivrait la faute au lieu de
  // l'attraper. Ils viennent de `e_additem_result`, moonlight src/map/pc.hpp.
  CHECK(PickupRefusalOf(1) == ItemPickupRefusal::kCannotGet);
  CHECK(PickupRefusalOf(2) == ItemPickupRefusal::kTooHeavy);
  CHECK(PickupRefusalOf(4) == ItemPickupRefusal::kOverWeightLimit);
  CHECK(PickupRefusalOf(5) == ItemPickupRefusal::kOneKindLimit);
  CHECK(PickupRefusalOf(6) == ItemPickupRefusal::kCannotGet);
  CHECK(PickupRefusalOf(7) == ItemPickupRefusal::kSameItemLimit);
}

TEST_CASE("Refus : deux codes partagent le message qui ne dit pas pourquoi") {
  // Le client écrit la même phrase pour le refus sans motif et pour le refus
  // temporaire. Les séparer inventerait une distinction qu'il ne fait pas.
  CHECK(PickupRefusalOf(1) == PickupRefusalOf(6));
}

TEST_CASE("Refus : un succès n'est pas un refus") { CHECK_FALSE(PickupRefusalOf(0).has_value()); }

TEST_CASE("Refus : un code sans message reste SILENCIEUX") {
  // 🔴 CE SERVEUR N'ÉMET NI 3 NI 9 — vérifié dans son code, pas supposé : la
  // valeur 3 n'est retournée nulle part, et aucun appel ne passe 9. Le client
  // sait pourtant écrire une phrase pour le 9. Leur en donner une ici serait du
  // code que rien n'atteint, et qui aurait l'air éprouvé.
  CHECK_FALSE(PickupRefusalOf(3).has_value());
  CHECK_FALSE(PickupRefusalOf(9).has_value());
  // Et tout le reste de l'octet se tait aussi, plutôt que d'inventer un motif.
  CHECK_FALSE(PickupRefusalOf(8).has_value());
  CHECK_FALSE(PickupRefusalOf(200).has_value());
  CHECK_FALSE(PickupRefusalOf(255).has_value());
}

TEST_CASE("Refus : aucun code connu ne rend un message vide quand la table le porte") {
  // Le défaut que ce chemin répare est le silence : chaque sens se range sous sa
  // clé, et la table de l'installation le dit.
  ro::vfs::ItemWords words;
  words.pickup.cannot_get = "Impossible.";
  words.pickup.too_heavy = "Trop lourd.";
  words.pickup.over_weight_limit = "Poids dépassé.";
  words.pickup.one_kind_limit = "Trop d'un seul objet.";
  words.pickup.same_item_limit = "Plafond atteint.";
  for (const std::uint8_t result : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{4},
                                    std::uint8_t{5}, std::uint8_t{6}, std::uint8_t{7}}) {
    const std::optional<ItemPickupRefusal> why = PickupRefusalOf(result);
    REQUIRE(why.has_value());
    CHECK_FALSE(ro::vfs::PickupRefusalText(words, *why).empty());
  }
}

TEST_CASE("Refus : sans la clé de la table, aucun texte anglais en dur") {
  // La table de messages est la seule source : une clé absente donne un texte
  // vide, que l'appelant n'écrit pas.
  const ro::vfs::ItemWords words;
  for (const std::uint8_t result : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{4},
                                    std::uint8_t{5}, std::uint8_t{6}, std::uint8_t{7}}) {
    const std::optional<ItemPickupRefusal> why = PickupRefusalOf(result);
    REQUIRE(why.has_value());
    CHECK(ro::vfs::PickupRefusalText(words, *why).empty());
  }
}
