// LES DEUX PEAUX DE L'ÉDITEUR, hors fenêtre : la ligne que la pipette désigne,
// ce qui interdit de régler, et le calque de chaque ligne.
//
// ⚠ LA PIPETTE SE TROMPERAIT SANS RIEN DIRE : un clic sur les bras qui ouvrirait
// la Tête ferait régler le visage en croyant teindre le corps.

#include "character/style_skin.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

using moonclient::kSkinTones;
using moonclient::SkinLayerAt;
using moonclient::SkinLine;
using moonclient::SkinLineUnder;
using moonclient::SkinRamp;
using moonclient::SkinRefusal;
using moonclient::SkinRefusalFor;
using ro::render::ActorPart;
using ro::render::StyleLayer;

namespace {

constexpr std::uint8_t kSkinFirst = 129;
constexpr std::uint8_t kSkinLast = 135;
constexpr std::uint32_t kBodyKey = 0x1234ABCDU;
// Le dernier calque d'un serveur qui range la peau du corps, et d'un plus ancien.
constexpr int kServerWithBodySkin = 6;
constexpr int kServerBefore = 5;

const std::vector<std::string> kOnFoot = {"data\\sprite\\인간족\\몸통\\남\\초보자_남"};
const std::vector<std::string> kOstrich = {"data\\sprite\\인간족\\몸통\\남\\타조_로드나이트_남"};

}  // namespace

TEST_CASE("Peaux : la pipette ouvre le visage sur la tête, le corps sur les bras",
          "[app][style][skin]") {
  for (int index = kSkinFirst; index <= kSkinLast; ++index) {
    const auto at = static_cast<std::uint8_t>(index);
    CHECK(SkinLineUnder(ActorPart::kHead, at) == SkinLine::kFace);
    CHECK(SkinLineUnder(ActorPart::kBody, at) == SkinLine::kBody);
    // Une coiffe ou la cape n'ont pas de peau.
    CHECK_FALSE(SkinLineUnder(ActorPart::kHeadgearTop, at).has_value());
    CHECK_FALSE(SkinLineUnder(ActorPart::kGarment, at).has_value());
  }
  // Hors de la peau : les cheveux, une pièce du corps.
  CHECK_FALSE(SkinLineUnder(ActorPart::kHead, kSkinFirst - 1).has_value());
  CHECK_FALSE(SkinLineUnder(ActorPart::kBody, kSkinLast + 1).has_value());
}

TEST_CASE("Peaux : chaque ligne dit pourquoi elle ne se règle pas", "[app][style][skin]") {
  for (const SkinLine line : {SkinLine::kFace, SkinLine::kBody}) {
    CHECK(SkinRefusalFor(line, false, kOnFoot, kServerWithBodySkin) == SkinRefusal::kNone);
    CHECK(SkinRefusalFor(line, true, kOnFoot, kServerWithBodySkin) == SkinRefusal::kGmOutfit);
    // 🔴 SUR UNE AUTRUCHE, LES DEUX SE FIGENT : la peau y peint l'animal.
    CHECK(SkinRefusalFor(line, false, kOstrich, kServerWithBodySkin) == SkinRefusal::kOnMount);
  }
  // Un serveur qui ne range pas la peau du corps : le visage reste réglable.
  CHECK(SkinRefusalFor(SkinLine::kFace, false, kOnFoot, kServerBefore) == SkinRefusal::kNone);
  CHECK(SkinRefusalFor(SkinLine::kBody, false, kOnFoot, kServerBefore) ==
        SkinRefusal::kNoBodySkinServer);
  CHECK(SkinRefusalFor(SkinLine::kBody, false, kOnFoot, 0) == SkinRefusal::kNoBodySkinServer);
}

TEST_CASE("Peaux : le visage sous la clé zéro, le corps sous la sienne", "[app][style][skin]") {
  CHECK(SkinLayerAt(SkinLine::kFace, kBodyKey).layer == StyleLayer::kSkin);
  CHECK(SkinLayerAt(SkinLine::kFace, kBodyKey).key == 0);
  CHECK(SkinLayerAt(SkinLine::kBody, kBodyKey).layer == StyleLayer::kBodySkin);
  CHECK(SkinLayerAt(SkinLine::kBody, kBodyKey).key == kBodyKey);
  // La pièce réglée couvre tout l'ensemble de peau.
  const ro::render::PaletteRamp ramp = SkinRamp();
  CHECK(ramp.start == kSkinFirst);
  CHECK(ramp.start + ramp.length - 1 == kSkinLast);
  // Les teints vont du plus clair au plus foncé.
  for (std::size_t rank = 1; rank < kSkinTones.size(); ++rank) {
    CHECK(kSkinTones[rank][0] < kSkinTones[rank - 1][0]);
  }
}
