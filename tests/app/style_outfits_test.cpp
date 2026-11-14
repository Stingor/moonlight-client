// Le guichet des tenues : ce qu'une tenue prend du personnage, et ce qu'un code
// reçu devient — une proposition, consommée une fois.

#include "character/style_outfits.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style.h"
#include "ro/sim/style_outfit.h"
#include "ro/sim/world.h"

using moonclient::StyleOutfits;
using ro::sim::OutfitAction;
using ro::sim::OutfitReply;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::sim::StyleOutfit;
using ro::sim::World;

namespace {

constexpr std::uint32_t kMe = 2000001;
constexpr std::uint32_t kBody = 0xB0D1;
constexpr std::uint32_t kMount = 0xB0D2;
constexpr std::uint32_t kCadenceMs = 1000;

[[nodiscard]] StyleOutfit OneHead() {
  StyleOutfit outfit;
  StyleLayerRecipe head{.layer = StyleLayerKind::kHead, .key = (3U << 16U) | 2U, .profile = 1};
  head.adjusts[0].hue = 45;
  outfit.layers.push_back(head);
  return outfit;
}

}  // namespace

TEST_CASE("Tenues : on capture ce que le serveur a validé, repli compris", "[app][outfits]") {
  ro::sim::StyleBook book;
  CHECK_FALSE(StyleOutfits::Capture(kMe, book).has_value());
  ro::sim::StyleRecipe recipe;
  recipe.ramps[0].hue = 10;
  book.BeginBatchFor(kMe);
  book.Remember(kMe, kBody, recipe, false);
  book.Remember(kMe, kMount, recipe, true);
  book.RememberLayers(kMe, OneHead().layers);

  const std::optional<StyleOutfit> outfit = StyleOutfits::Capture(kMe, book);
  REQUIRE(outfit.has_value());
  CHECK(outfit->bodies.size() == 2);
  CHECK(outfit->fallback_body == kMount);
  REQUIRE(outfit->layers.size() == 1);
  CHECK(outfit->layers[0].adjusts[0].hue == 45);
}

TEST_CASE("Tenues : rien ne part avant que le serveur ait dit sa cadence", "[app][outfits]") {
  World world;
  StyleOutfits outfits;
  CHECK_FALSE(outfits.CanSend(world, 0));
  world.session().style_outfit_cadence_ms = kCadenceMs;
  CHECK(outfits.CanSend(world, 0));
}

TEST_CASE("Tenues : un code reçu devient une proposition, une seule fois", "[app][outfits]") {
  World world;
  StyleOutfits outfits;
  OutfitReply reply;
  reply.action = OutfitAction::kFetch;
  reply.code = "K7QM3XPA";
  reply.outfit = OneHead();
  world.outfits().Hear(reply);

  outfits.Update(world);
  std::optional<StyleOutfit> proposal = outfits.TakeProposal();
  REQUIRE(proposal.has_value());
  CHECK(proposal->layers.size() == 1);
  // 🔴 CONSOMMÉE : l'image suivante ne la rend pas, ni la même réponse relue.
  outfits.Update(world);
  CHECK_FALSE(outfits.TakeProposal().has_value());

  // Un refus ne propose rien.
  OutfitReply refused;
  refused.action = OutfitAction::kFetch;
  refused.raw_result = static_cast<std::uint8_t>(ro::sim::OutfitResult::kUnknownCode);
  world.outfits().Hear(refused);
  outfits.Update(world);
  CHECK_FALSE(outfits.TakeProposal().has_value());

  // Le code publié se garde, pour l'afficher.
  OutfitReply published;
  published.action = OutfitAction::kPublish;
  published.code = "ABCDEFGH";
  world.outfits().Hear(published);
  outfits.Update(world);
  CHECK(outfits.published_code() == "ABCDEFGH");
}

TEST_CASE("Tenues : une tenue rangée se propose depuis la garde-robe", "[app][outfits]") {
  World world;
  StyleOutfits outfits;
  OutfitReply list;
  list.action = OutfitAction::kWardrobeList;
  std::vector<ro::sim::StyleWardrobeEntry> wardrobe;
  wardrobe.push_back(ro::sim::StyleWardrobeEntry{.slot = 4, .name = "Soir", .outfit = OneHead()});
  world.outfits().Hear(list, std::move(wardrobe));

  CHECK_FALSE(outfits.ProposeFromWardrobe(world, 3));
  REQUIRE(outfits.ProposeFromWardrobe(world, 4));
  CHECK(outfits.TakeProposal().has_value());
}
