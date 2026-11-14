// LA TEINTURE D'UN EFFET DE CHAPEAU AU DESSIN : la TSL, la clarté,
// « assombrir », et les mélanges qu'elle change.

#include "ro/render/effect_dye.h"

#include <array>
#include <cstdint>
#include <cstdlib>

#include <catch2/catch_test_macros.hpp>

using ro::render::DyeEffectTexel;
using ro::render::EffectBlendAdds;
using ro::render::EffectDye;
using ro::render::IsNeutralEffectDye;
using ro::render::MakeEffectDye;

namespace {

using Texel = std::array<std::uint8_t, 4>;

// À un près : la TSL se calcule en flottants.
bool Near(const Texel& left, const Texel& right) {
  for (std::size_t channel = 0; channel < left.size(); ++channel) {
    if (std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel])) > 1) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("Un réglage nul est la teinture neutre", "[effect_dye]") {
  CHECK(IsNeutralEffectDye(MakeEffectDye(0, 0, 0, false)));
  CHECK_FALSE(IsNeutralEffectDye(MakeEffectDye(30, 0, 0, false)));
  CHECK_FALSE(IsNeutralEffectDye(MakeEffectDye(0, 0, 0, true)));
}

TEST_CASE("La teinture neutre rend le texel tel quel", "[effect_dye]") {
  const Texel texel = {37, 190, 222, 140};
  CHECK(DyeEffectTexel(texel, EffectDye{}) == texel);
}

TEST_CASE("Un gris reste un gris sous tout tour de teinte", "[effect_dye]") {
  const Texel gray = {120, 120, 120, 255};
  for (const int hue : {45, 120, 200, -90}) {
    CHECK(Near(DyeEffectTexel(gray, MakeEffectDye(hue, 0, 0, false)), gray));
  }
}

TEST_CASE("Un tour de teinte de 120° envoie le rouge pur au vert pur", "[effect_dye]") {
  // La TSL garde la saturation : une matrice linéaire rendrait un vert sombre.
  CHECK(DyeEffectTexel({255, 0, 0, 200}, MakeEffectDye(120, 0, 0, false)) == Texel{0, 255, 0, 200});
}

TEST_CASE("Un tour de teinte change la couleur dominante", "[effect_dye]") {
  // Un jaune d'aura, tourné de 120° : le vert et le bleu dominent le rouge.
  const Texel yellow = {230, 200, 40, 255};
  const Texel turned = DyeEffectTexel(yellow, MakeEffectDye(120, 0, 0, false));
  CHECK(turned[2] > turned[0]);
  CHECK(turned[3] == 255);
}

TEST_CASE("La saturation à -100 ne laisse que la luminance", "[effect_dye]") {
  const Texel red = {255, 0, 0, 255};
  const Texel dyed = DyeEffectTexel(red, MakeEffectDye(0, -100, 0, false));
  CHECK(dyed[0] == dyed[1]);
  CHECK(dyed[1] == dyed[2]);
}

TEST_CASE("La clarté à -100 donne du noir, l'alpha reste", "[effect_dye]") {
  const Texel dyed = DyeEffectTexel({200, 150, 90, 180}, MakeEffectDye(0, 0, -100, false));
  CHECK(dyed == Texel{0, 0, 0, 180});
}

TEST_CASE("Assombrir efface un texel noir et rend un texel vif opaque", "[effect_dye]") {
  const EffectDye darken = MakeEffectDye(0, 0, 0, true);
  CHECK(DyeEffectTexel({0, 0, 0, 255}, darken)[3] == 0);
  const Texel bright = DyeEffectTexel({255, 128, 0, 255}, darken);
  CHECK(bright[3] == 255);
  CHECK(Near(bright, Texel{255, 128, 0, 255}));
}

TEST_CASE("Assombrir porte un texel sombre à pleine intensité, translucide", "[effect_dye]") {
  const Texel dim = DyeEffectTexel({64, 32, 0, 255}, MakeEffectDye(0, 0, 0, true));
  CHECK(dim[3] == 64);
  CHECK(Near(dim, Texel{255, 128, 0, 64}));
}

TEST_CASE("Les bornes du réglage se ramènent", "[effect_dye]") {
  CHECK(MakeEffectDye(0, 0, -400, false) == MakeEffectDye(0, 0, -100, false));
  CHECK(MakeEffectDye(900, 0, 0, false) == MakeEffectDye(359, 0, 0, false));
}

TEST_CASE("Seuls les mélanges qui gardent la destination ajoutent", "[effect_dye]") {
  CHECK(EffectBlendAdds(5, 2));
  CHECK(EffectBlendAdds(5, 7));
  CHECK_FALSE(EffectBlendAdds(5, 6));
  CHECK_FALSE(EffectBlendAdds(2, 1));
  CHECK_FALSE(EffectBlendAdds(1, 2));
}
