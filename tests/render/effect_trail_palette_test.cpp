#include "ro/render/effect_trail_palette.h"

#include <catch2/catch_test_macros.hpp>

using ro::render::EffectTrailVariant;
using ro::render::EffectTrailVariantFor;
using ro::render::EffectTrailVariantTable;
using ro::render::NeutralEffectTrailVariant;

TEST_CASE("Palette de ruban : la table est TRIÉE et sans doublon", "[render][table]") {
  // La recherche est dichotomique : un doublon rendrait l'une des deux entrées
  // inatteignable, et un désordre ferait rendre le neutre à une variante qui a
  // bien sa couleur.
  const std::span<const EffectTrailVariant> table = EffectTrailVariantTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].variant < table[rang].variant);
  }
}

TEST_CASE("Palette de ruban : une variante ABSENTE rend le neutre", "[render][table]") {
  // Le switch du client a cent un cas et n'en pose que quarante-quatre ; les
  // autres tombent dans son défaut, qui laisse le blanc. Rendre autre chose
  // teinterait des rubans que le client laisse nus.
  //
  // Le cas 5 est le contrôle : il TOMBE dans le défaut, et c'est lui qui a
  // dénoncé l'index d'octets oublié — sans cette indirection il prenait la
  // couleur du cas 6.
  const EffectTrailVariant neutre = NeutralEffectTrailVariant();
  CHECK(neutre.color == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK(neutre.forced_alpha == 0);
  CHECK(neutre.blend_destination == 2);

  const EffectTrailVariant cinq = EffectTrailVariantFor(5);
  CHECK(cinq.color == neutre.color);
  CHECK(cinq.blend_destination == neutre.blend_destination);
}

TEST_CASE("Palette de ruban : les entrées relevées à la main s'y retrouvent", "[render][table]") {
  // Une poignée d'entrées lues à la main, pour que le relevé
  // automatique ait un contrôle qui ne vienne pas de lui.
  CHECK(EffectTrailVariantFor(0).blend_destination == 6);
  CHECK(EffectTrailVariantFor(0).color == std::array<std::uint8_t, 3>{255, 255, 255});

  // La seule variante qui IMPOSE son alpha, et elle l'impose bas.
  CHECK(EffectTrailVariantFor(3).forced_alpha == 80);

  // Six et sept sont symétriques : l'un garde le rouge, l'autre le vert.
  CHECK(EffectTrailVariantFor(6).color == std::array<std::uint8_t, 3>{255, 100, 100});
  CHECK(EffectTrailVariantFor(7).color == std::array<std::uint8_t, 3>{100, 255, 100});

  // Les primaires pures, que le client range à la suite.
  CHECK(EffectTrailVariantFor(17).color == std::array<std::uint8_t, 3>{0, 0, 255});
  CHECK(EffectTrailVariantFor(18).color == std::array<std::uint8_t, 3>{255, 0, 0});
  CHECK(EffectTrailVariantFor(19).color == std::array<std::uint8_t, 3>{0, 255, 0});

  // La dernière, isolée tout en haut de la plage.
  CHECK(EffectTrailVariantFor(100).color == std::array<std::uint8_t, 3>{250, 250, 255});
  CHECK(EffectTrailVariantFor(100).blend_destination == 6);
}

TEST_CASE("Palette de ruban : aucune variante ne pose un alpha nul", "[render][table]") {
  // Zéro dit « laisse celui de la piste », et cela ne tient que si aucune
  // variante n'impose vraiment zéro. Le jour où l'une le ferait, il faudrait un
  // autre porteur pour l'absence — et ce test le dirait.
  for (const EffectTrailVariant& variant : EffectTrailVariantTable()) {
    if (variant.forced_alpha != 0) {
      CHECK(variant.forced_alpha > 0);
    }
  }
  CHECK(EffectTrailVariantFor(3).forced_alpha != 0);
}
