// Les altérations, qui n'arrivent pas comme les autres états.
//
// 🔴 CE QUI SE PROUVE ICI EST LA DIFFÉRENCE ENTRE UNE VALEUR ET UN MASQUE. Le
// serveur le dit à l'endroit où il les lit, et l'erreur qu'il annonce est
// silencieuse : traiter `opt2` comme `opt1` donnerait un sommeil dès qu'un
// poison est actif — une icône fausse, jamais un plantage.

#include "ro/sim/status_effects.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::AilmentStatuses;
using ro::sim::StatusEffectId;

namespace {

// Les valeurs du serveur, `e_opt1` et `e_opt2`.
constexpr std::uint16_t kOpt1Sleep = 4;
constexpr std::uint16_t kOpt1Stone = 1;
constexpr std::uint16_t kOpt2Poison = 0x0001;
constexpr std::uint16_t kOpt2Blind = 0x0010;
constexpr std::uint16_t kOpt2Angelus = 0x0020;

[[nodiscard]] std::vector<StatusEffectId> Of(std::uint16_t body, std::uint16_t health) {
  std::vector<StatusEffectId> found;
  AilmentStatuses(body, health, found);
  return found;
}

}  // namespace

TEST_CASE("Altérations : l'état du corps est une VALEUR, un seul à la fois", "[sim][etats]") {
  CHECK(Of(kOpt1Sleep, 0) == std::vector<StatusEffectId>{ro::sim::kEfstSleep});
  CHECK(Of(kOpt1Stone, 0) == std::vector<StatusEffectId>{ro::sim::kEfstStoneCurse});

  // ⚠ CINQ N'EXISTE PAS : le serveur saute ce rang, et la pétrification EN
  // COURS vaut six. Un rang inconnu ne pose rien plutôt que de deviner.
  CHECK(Of(5, 0).empty());
  CHECK(Of(6, 0) == std::vector<StatusEffectId>{ro::sim::kEfstStoneCurseInProgress});
  CHECK(Of(0, 0).empty());
}

TEST_CASE("Altérations : l'état de santé est un MASQUE, autant que de bits", "[sim][etats]") {
  // 🔴 C'EST L'ERREUR QUE LE SERVEUR ANNONCE. Lu comme une valeur, `0x0011`
  // serait le dix-septième rang d'`opt1` — ou, pire, le poison seul.
  const std::vector<StatusEffectId> both = Of(0, kOpt2Poison | kOpt2Blind);
  REQUIRE(both.size() == 2);
  CHECK(both[0] == ro::sim::kEfstPoison);
  CHECK(both[1] == ro::sim::kEfstBlind);

  // ⚠ ANGELUS PORTE SON PROPRE EFST, annoncé par un vrai paquet d'état : le
  // serveur ne traduit pas ce bit-là, et nous non plus. Le traduire poserait
  // deux icônes pour un seul buff.
  CHECK(Of(0, kOpt2Angelus).empty());
}

TEST_CASE("Altérations : les deux familles se cumulent", "[sim][etats]") {
  // Un empoisonné endormi porte les deux, et le corps passe d'abord.
  const std::vector<StatusEffectId> both = Of(kOpt1Sleep, kOpt2Poison);
  REQUIRE(both.size() == 2);
  CHECK(both[0] == ro::sim::kEfstSleep);
  CHECK(both[1] == ro::sim::kEfstPoison);
}

TEST_CASE("Altérations : la liste se refait, elle ne s'accumule pas", "[sim][etats]") {
  std::vector<StatusEffectId> found;
  AilmentStatuses(kOpt1Sleep, kOpt2Poison, found);
  REQUIRE(found.size() == 2);
  // ⚠ UN BIT QUI RETOMBE N'ANNONCE PAS SA FIN : c'est la liste ENTIÈRE qui vaut
  // vérité à chaque relevé, et la réutiliser sans la vider garderait un poison
  // que le joueur vient de soigner.
  AilmentStatuses(0, 0, found);
  CHECK(found.empty());
}

TEST_CASE("Nuit : ce qui assombrit la carte", "[sim][etats]") {
  using ro::sim::DarkensTheMap;
  CHECK_FALSE(DarkensTheMap({}));
  CHECK(DarkensTheMap({ro::sim::kEfstNight}));
  // Le lien d'âme et les esprits du Soul Reaper lèvent le même drapeau.
  CHECK(DarkensTheMap({ro::sim::kEfstSoulLink}));
  CHECK(DarkensTheMap({ro::sim::kEfstSoulFairy}));
  CHECK(DarkensTheMap({ro::sim::kEfstSoulDivision}));
  // Leurs voisins, non.
  CHECK_FALSE(DarkensTheMap({static_cast<StatusEffectId>(ro::sim::kEfstSoulFairy - 1),
                             static_cast<StatusEffectId>(ro::sim::kEfstSoulDivision + 1)}));
  CHECK_FALSE(DarkensTheMap({ro::sim::kEfstPoison}));
}
