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
using ro::render::SkinSet;
using ro::render::StyleLayer;

namespace {

constexpr std::uint8_t kSkinFirst = 129;
constexpr std::uint8_t kSkinLast = 135;
// Là où un corps réindexé range sa peau.
constexpr std::uint8_t kLowSkinFirst = 33;

[[nodiscard]] SkinSet SkinFrom(std::uint8_t first) {
  SkinSet skin;
  skin.count = static_cast<std::uint8_t>(ro::render::kSkinLength);
  for (std::size_t offset = 0; offset < ro::render::kSkinLength; ++offset) {
    skin.entries[offset] = static_cast<std::uint8_t>(first + offset);
  }
  return skin;
}
constexpr std::uint32_t kBodyKey = 0x1234ABCDU;
// Le dernier calque d'un serveur qui range la peau du corps, et d'un plus ancien.
constexpr int kServerWithBodySkin = 6;
constexpr int kServerBefore = 5;

const std::vector<std::string> kOnFoot = {"data\\sprite\\인간족\\몸통\\남\\초보자_남"};
const std::vector<std::string> kOstrich = {"data\\sprite\\인간족\\몸통\\남\\타조_로드나이트_남"};

}  // namespace

TEST_CASE("Peaux : la pipette ouvre le visage sur la tête, le corps sur les bras",
          "[app][style][skin]") {
  const SkinSet head_like = ro::render::HeadSkin(ro::render::SkinRace::kHuman);
  for (int index = kSkinFirst; index <= kSkinLast; ++index) {
    const auto at = static_cast<std::uint8_t>(index);
    CHECK(SkinLineUnder(ActorPart::kHead, at, head_like) == SkinLine::kFace);
    CHECK(SkinLineUnder(ActorPart::kBody, at, head_like) == SkinLine::kBody);
    // Une coiffe ou la cape n'ont pas de peau.
    CHECK_FALSE(SkinLineUnder(ActorPart::kHeadgearTop, at, head_like).has_value());
    CHECK_FALSE(SkinLineUnder(ActorPart::kGarment, at, head_like).has_value());
  }
  // Hors de la peau : les cheveux, une pièce du corps.
  CHECK_FALSE(SkinLineUnder(ActorPart::kHead, kSkinFirst - 1, head_like).has_value());
  CHECK_FALSE(SkinLineUnder(ActorPart::kBody, kSkinLast + 1, head_like).has_value());
}

TEST_CASE("Peaux : sur un corps, la pipette suit la peau que son sprite range",
          "[app][style][skin]") {
  const SkinSet low = SkinFrom(kLowSkinFirst);
  // Les bras peints en 33 ouvrent la peau du corps ; 129, un vêtement, non.
  CHECK(SkinLineUnder(ActorPart::kBody, kLowSkinFirst, low) == SkinLine::kBody);
  CHECK_FALSE(SkinLineUnder(ActorPart::kBody, kSkinFirst, low).has_value());
  // La tête garde son ensemble fixe, quel que soit le corps.
  CHECK(SkinLineUnder(ActorPart::kHead, kSkinFirst, low) == SkinLine::kFace);
  CHECK_FALSE(SkinLineUnder(ActorPart::kHead, kLowSkinFirst, low).has_value());
  // Un corps sans peau : aucun index n'ouvre la ligne du corps.
  CHECK_FALSE(SkinLineUnder(ActorPart::kBody, kSkinFirst, SkinSet{}).has_value());
  // La pièce réglée est la peau du corps.
  CHECK(SkinRamp(low).start == kLowSkinFirst);
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
  // Sans peau propre au corps, la pièce réglée couvre l'ensemble d'une tête.
  const ro::render::PaletteRamp ramp = SkinRamp(SkinSet{});
  CHECK(ramp.start == kSkinFirst);
  CHECK(ramp.start + ramp.length - 1 == kSkinLast);
  // Les teints vont du plus clair au plus foncé.
  for (std::size_t rank = 1; rank < kSkinTones.size(); ++rank) {
    CHECK(kSkinTones[rank][0] < kSkinTones[rank - 1][0]);
  }
}
