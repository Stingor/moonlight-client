// L'EFFET D'INCANTATION : ce qui tient la barre, et ce qui gicle une fois.
//
// Le client pose une seule instance, qui vit la durée annoncée. Les anneaux
// d'incantation recopient cette vie dans leur nœud et tiennent donc toute la
// barre ; la gerbe du coup d'épée, elle, sème au premier tic des nœuds à vie
// posée et ne fait plus rien. Magnum Break, qui incante avec cette gerbe, la
// montrait en boucle jusqu'à la fin de l'incantation.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skill_cast_table.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_lua_effect.h"

#include "effects/effect_node_view.h"
#include "effects/effect_rules.h"

namespace {

constexpr std::uint16_t kMagnumBreak = 7;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint32_t kWaterElement = 1;
constexpr std::int16_t kSwordGush = 16;
constexpr std::uint16_t kBrandishSpear = 57;
constexpr std::int16_t kBrandishSpearLance = 144;

// La règle telle que le semis du cercle la consulte : avec la vie des nœuds.
[[nodiscard]] bool Relaunches(std::int16_t effect) {
  const auto effect_id = static_cast<std::uint32_t>(effect);
  return moonclient::CastAuraRelaunches(
      effect_id, moonclient::EffectNodesInPlay::ProceduralLifeTicks(effect_id));
}

}  // namespace

TEST_CASE("Incantation : la gerbe de Magnum Break ne se relance pas", "[app][effets]") {
  const std::int16_t effect = ro::render::CastEffectForElement(
      ro::render::SkillImplFor(kMagnumBreak).CastEffectId(), kWaterElement);
  REQUIRE(effect == kSwordGush);
  CHECK_FALSE(Relaunches(effect));
}

TEST_CASE("Incantation : un effet fait d'un .str seul ne se relance pas", "[app][effets]") {
  // La lance de la table de Brandish Spear, un `.str` sans nœud : sans vie de
  // nœud, la relance repartirait à chaque tic. Que le client ne la fasse pas
  // naître à l'incantation est une autre règle — voir
  // `brandish_spear_lance_test.cpp`.
  const std::int16_t effect = ro::render::CastEffectForElement(
      ro::render::SkillImplFor(kBrandishSpear).CastEffectId(), kWaterElement);
  REQUIRE(effect == kBrandishSpearLance);
  CHECK(moonclient::EffectNodesInPlay::ProceduralLifeTicks(static_cast<std::uint32_t>(effect)) ==
        0);
  CHECK_FALSE(Relaunches(effect));
}

TEST_CASE("Incantation : les auras élémentaires tiennent toute la barre", "[app][effets]") {
  // Cold Bolt et ses voisins, que le joueur a jugés conformes : l'aura doit
  // durer autant que l'incantation.
  const std::int16_t effect = ro::render::CastEffectForElement(
      ro::render::SkillImplFor(kColdBolt).CastEffectId(), kWaterElement);
  CHECK(Relaunches(effect));
  CHECK(Relaunches(ro::render::kCastEffectElemental));
  for (const std::int16_t aura : ro::render::CastElementalEffects()) {
    INFO("aura " << aura);
    CHECK(Relaunches(aura));
  }
}

TEST_CASE("Incantation : la gerbe et les fichiers seuls giclent une fois, dans toute la table",
          "[app][effets]") {
  // ⚠ LE CONTRÔLE PORTE SUR TOUTE LA TABLE : un effet qui rejoindrait la
  // famille de l'éclat d'écran, ou qui perdrait ses nœuds, changerait de
  // conduite sans qu'un cas nommé le voie.
  for (const ro::render::SkillCastEntry& entry : ro::render::SkillCastTable()) {
    if (entry.effect_id <= 0) {
      continue;
    }
    INFO("compétence " << entry.skill_id << ", effet " << entry.effect_id);
    const bool without_nodes = moonclient::EffectNodesInPlay::ProceduralLifeTicks(
                                   static_cast<std::uint32_t>(entry.effect_id)) == 0;
    CHECK(Relaunches(entry.effect_id) == (entry.effect_id != kSwordGush && !without_nodes));
  }
}
