// CE QUE L'ÉDITEUR DE STYLE RANGE SUR LE DISQUE.
//
// ⚠ ÉPROUVÉ SUR L'ALLER-RETOUR COMPLET — mémoire, fichier, mémoire — parce que
// c'est là que vivent les pannes de ce genre de module : une clé écrite et
// jamais relue, un ordre qui se perd, une entrée périmée qui reste dans la
// liste et donne une ligne qui ne fait rien.

#include "character/style_store.h"

#include <array>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/file_write.h"
#include "ro/sim/style.h"
#include "ro/sim/style_code.h"

#include "support/temp_directory.h"

using moonclient::kMaxStylePresets;
using moonclient::LoadStyleDrafts;
using moonclient::LoadStylePresets;
using moonclient::SaveStyleDrafts;
using moonclient::SaveStylePresets;
using moonclient::StyleDrafts;
using moonclient::StylePresets;
using moonclient::StylePresetsPath;
using ro::sim::StyleAdjust;
using ro::sim::StyleRecipe;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] StyleRecipe Tinted(std::int16_t hue) {
  StyleRecipe recipe;
  recipe.body_palette = 42;
  recipe.ramps[0] = StyleAdjust{.hue = hue, .saturation = -20, .value = 10, .absolute = true};
  return recipe;
}

constexpr std::uint32_t kKnightBody = 0x1F87791DU;
constexpr std::uint32_t kSmithBody = 0x1EB3DE79U;
// Deux personnages du même compte.
constexpr std::uint32_t kFirstCharacter = 150001;
constexpr std::uint32_t kSecondCharacter = 150002;

}  // namespace

TEST_CASE("Préréglages : ce qui est rangé se relit", "[app][style]") {
  TempDirectory client;
  StylePresets written;
  REQUIRE(written.Save("soirée", Tinted(180)));
  REQUIRE(written.Save("ma monture", Tinted(60)));
  REQUIRE(SaveStylePresets(client.path(), written));

  StylePresets read;
  LoadStylePresets(client.path(), read);
  REQUIRE(read.all().size() == 2);
  // ⚠ DANS L'ORDRE OÙ ILS ONT ÉTÉ POSÉS : une liste qui se réordonne à chaque
  // écriture est illisible en diff, et ce fichier se corrige à la main.
  CHECK(read.all()[0].name == "soirée");
  CHECK(read.all()[1].name == "ma monture");
  const StyleRecipe* found = read.Find("soirée");
  REQUIRE(found != nullptr);
  CHECK(found->ramps[0].hue == 180);
  CHECK(found->body_palette == 42);
  CHECK(found->ramps[0].absolute);
}

TEST_CASE("Préréglages : mettre à jour ne déplace pas la ligne", "[app][style]") {
  // ⚠ SINON ELLE SAUTE EN FIN DE LISTE sous les doigts de qui la retouche, et
  // le geste le plus courant — charger, régler, réenregistrer — réordonne la
  // liste à chaque passage.
  StylePresets kept;
  REQUIRE(kept.Save("un", Tinted(10)));
  REQUIRE(kept.Save("deux", Tinted(20)));
  REQUIRE(kept.Save("trois", Tinted(30)));
  REQUIRE(kept.Save("deux", Tinted(200)));

  REQUIRE(kept.all().size() == 3);
  CHECK(kept.all()[1].name == "deux");
  CHECK(kept.all()[1].recipe.ramps[0].hue == 200);
}

TEST_CASE("Préréglages : le plafond refuse d'ajouter, jamais de remplacer", "[app][style]") {
  StylePresets kept;
  for (std::size_t rank = 0; rank < kMaxStylePresets; ++rank) {
    REQUIRE(kept.Save("n" + std::to_string(rank), Tinted(static_cast<std::int16_t>(rank))));
  }
  CHECK(kept.full());
  CHECK_FALSE(kept.Save("de trop", Tinted(1)));
  // 🔴 MAIS METTRE À JOUR RESTE POSSIBLE : refuser aussi cela enfermerait le
  // joueur, qui ne pourrait plus retoucher ce qu'il a déjà rangé.
  CHECK(kept.Save("n0", Tinted(123)));
  CHECK(kept.all().size() == kMaxStylePresets);

  kept.Erase("n0");
  CHECK_FALSE(kept.full());
  CHECK(kept.Find("n0") == nullptr);
  CHECK(kept.Save("de trop", Tinted(1)));
}

TEST_CASE("Préréglages : une entrée d'une autre version est écartée à la lecture", "[app][style]") {
  // 🔴 ELLE NE SE DÉCODERA JAMAIS — le format refuse la migration —, si bien
  // que la garder donnerait une ligne qui ne fait rien quand on clique dessus,
  // ce qui se lit comme une panne.
  TempDirectory client;
  const std::string good = ro::sim::EncodeStyleCode(Tinted(180));
  std::string stale = good;
  stale[0] = '6';
  const std::string text = std::string("presets:\n") + "  - name: bon\n    style: " + good +
                           "\n  - name: perime\n    style: " + stale + "\n";
  REQUIRE(ro::core::WriteFileAtomically(StylePresetsPath(client.path()), text).has_value());

  StylePresets read;
  LoadStylePresets(client.path(), read);
  REQUIRE(read.all().size() == 1);
  CHECK(read.all()[0].name == "bon");
}

TEST_CASE("Préréglages : un fichier absent ou abîmé laisse la liste vide", "[app][style]") {
  TempDirectory client;
  StylePresets read;
  LoadStylePresets(client.path(), read);
  CHECK(read.all().empty());

  REQUIRE(ro::core::WriteFileAtomically(StylePresetsPath(client.path()), "{ ceci n'est pas")
              .has_value());
  StylePresets broken;
  LoadStylePresets(client.path(), broken);
  CHECK(broken.all().empty());
}

TEST_CASE("Brouillon : un par corps, et l'identique ne se réécrit pas", "[app][style]") {
  StyleDrafts drafts;
  CHECK(drafts.Put(kKnightBody, Tinted(180)));
  // ⚠ RIEN DE NEUF, RIEN À ÉCRIRE : un brouillon identique à celui déjà rangé
  // ne vaut pas une écriture de fichier, et l'éditeur en propose une à chaque
  // fois que le joueur s'arrête.
  CHECK_FALSE(drafts.Put(kKnightBody, Tinted(180)));
  CHECK(drafts.Put(kKnightBody, Tinted(90)));

  // 🔴 UN AUTRE CORPS EST UN AUTRE BROUILLON : une recette ne désigne ses
  // pièces que par des index de palette, et les index n'ont aucune
  // signification commune d'un corps à l'autre.
  CHECK(drafts.Put(kSmithBody, Tinted(10)));
  CHECK(drafts.Has(kKnightBody));
  CHECK(drafts.Has(kSmithBody));
  REQUIRE(drafts.Find(kKnightBody) != nullptr);
  CHECK(drafts.Find(kKnightBody)->ramps[0].hue == 90);

  CHECK(drafts.Forget(kKnightBody));
  CHECK_FALSE(drafts.Forget(kKnightBody));
  CHECK_FALSE(drafts.Has(kKnightBody));
  CHECK(drafts.Has(kSmithBody));

  // Le corps inconnu n'en est pas un.
  CHECK_FALSE(drafts.Put(0, Tinted(180)));
}

TEST_CASE("Brouillon : il survit au fichier", "[app][style]") {
  TempDirectory client;
  StyleDrafts written;
  REQUIRE(written.Put(kKnightBody, Tinted(180)));
  REQUIRE(written.Put(kSmithBody, Tinted(60)));
  REQUIRE(SaveStyleDrafts(client.path(), kFirstCharacter, written));

  StyleDrafts read;
  LoadStyleDrafts(client.path(), kFirstCharacter, read);
  REQUIRE(read.Find(kKnightBody) != nullptr);
  REQUIRE(read.Find(kSmithBody) != nullptr);
  CHECK(read.Find(kKnightBody)->ramps[0].hue == 180);
  CHECK(read.Find(kSmithBody)->ramps[0].hue == 60);
  CHECK(read.Find(kKnightBody)->body_palette == 42);
}

TEST_CASE("Brouillon : chaque personnage a le sien", "[app][style]") {
  // 🔴 DEUX PERSONNAGES DE MÊME CLASSE PORTENT LE MÊME CORPS, et l'essai de
  // l'un ne doit ni se proposer à l'autre, ni effacer le sien en s'écrivant.
  TempDirectory client;
  StyleDrafts first;
  REQUIRE(first.Put(kKnightBody, Tinted(180)));
  REQUIRE(SaveStyleDrafts(client.path(), kFirstCharacter, first));
  StyleDrafts second;
  REQUIRE(second.Put(kKnightBody, Tinted(60)));
  REQUIRE(SaveStyleDrafts(client.path(), kSecondCharacter, second));

  StyleDrafts read_first;
  LoadStyleDrafts(client.path(), kFirstCharacter, read_first);
  REQUIRE(read_first.Find(kKnightBody) != nullptr);
  CHECK(read_first.Find(kKnightBody)->ramps[0].hue == 180);
  StyleDrafts read_second;
  LoadStyleDrafts(client.path(), kSecondCharacter, read_second);
  REQUIRE(read_second.Find(kKnightBody) != nullptr);
  CHECK(read_second.Find(kKnightBody)->ramps[0].hue == 60);
  StyleDrafts read_other;
  LoadStyleDrafts(client.path(), kSecondCharacter + 1, read_other);
  CHECK(read_other.all().empty());
}

TEST_CASE("Préréglages : un renommage garde sa place", "[app][style]") {
  StylePresets presets;
  REQUIRE(presets.Save("soirée", Tinted(10)));
  REQUIRE(presets.Save("monture", Tinted(20)));
  CHECK(presets.Rename("soirée", "gala"));
  REQUIRE(presets.all().size() == 2);
  CHECK(presets.all().front().name == "gala");
  CHECK(presets.Find("soirée") == nullptr);
  // Un nom pris, vide ou inconnu ne renomme rien.
  CHECK_FALSE(presets.Rename("gala", "monture"));
  CHECK_FALSE(presets.Rename("gala", ""));
  CHECK_FALSE(presets.Rename("inconnu", "autre"));
}

// ── LA TENUE DANS LE PRÉRÉGLAGE ─────────────────────────────────────────────

namespace {

using moonclient::PresetKeys;
using moonclient::PresetLayers;
using moonclient::RekeyPresetLayers;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;

// Une coupe et une couleur, puis une autre.
constexpr std::uint32_t kHeadKey = 0x000C0005U;
constexpr std::uint32_t kOtherHeadKey = 0x00170002U;
constexpr std::uint8_t kRampProfile = 1;
constexpr std::uint8_t kBodyMoreProfile = 3;
constexpr std::uint8_t kRetouchedIndex = 40;
constexpr std::uint32_t kHatView = 2286;

[[nodiscard]] StyleLayerRecipe Layer(StyleLayerKind layer, std::uint32_t key, std::int16_t hue) {
  // La suite du corps a son profil à elle ; les autres calques, celui des rampes.
  const std::uint8_t profile = layer == StyleLayerKind::kBodyMore ? kBodyMoreProfile : kRampProfile;
  StyleLayerRecipe recipe{.layer = layer, .key = key, .profile = profile};
  recipe.adjusts[0] = StyleAdjust{.hue = hue};
  return recipe;
}

[[nodiscard]] std::vector<StyleLayerRecipe> Outfit() {
  StyleLayerRecipe head = Layer(StyleLayerKind::kHead, kHeadKey, 30);
  head.retouches.push_back(
      ro::sim::StyleRetouch{.index = kRetouchedIndex, .red = 200, .green = 10, .blue = 10});
  return {head, Layer(StyleLayerKind::kSkin, 0, 15),
          Layer(StyleLayerKind::kBodyMore, kKnightBody, 60),
          Layer(StyleLayerKind::kBodySkin, kKnightBody, 90),
          Layer(StyleLayerKind::kHeadgear, kHatView, 120)};
}

}  // namespace

TEST_CASE("Préréglages : la tenue du personnage se range, jamais un objet", "[app][style]") {
  // 🔴 UNE TEINTURE DE COIFFE APPARTIENT À UN EXEMPLAIRE : un préréglage, que
  // n'importe quel personnage recharge, ne doit pas la porter.
  TempDirectory client;
  StylePresets written;
  REQUIRE(written.Save("complet", Tinted(200), Outfit()));
  REQUIRE(written.FindNamed("complet") != nullptr);
  CHECK(written.FindNamed("complet")->layers.size() == 4);
  REQUIRE(SaveStylePresets(client.path(), written));

  StylePresets read;
  LoadStylePresets(client.path(), read);
  const StylePresets::Named* kept = read.FindNamed("complet");
  REQUIRE(kept != nullptr);
  CHECK(kept->recipe.ramps[0].hue == 200);
  REQUIRE(kept->layers.size() == 4);
  CHECK(kept->layers[0].layer == StyleLayerKind::kHead);
  CHECK(kept->layers[0].key == kHeadKey);
  REQUIRE(kept->layers[0].retouches.size() == 1);
  CHECK(kept->layers[0].retouches[0].index == kRetouchedIndex);
  CHECK(kept->layers[3].layer == StyleLayerKind::kBodySkin);
  CHECK(kept->layers[3].adjusts[0].hue == 90);
}

TEST_CASE("Tenue reprise : le corps qu'on règle, sinon le repli, sinon le premier",
          "[app][style]") {
  using moonclient::OutfitBodyFor;
  ro::sim::StyleOutfit outfit;
  CHECK(OutfitBodyFor(outfit, kKnightBody) == nullptr);
  outfit.bodies.emplace_back(kSmithBody, Tinted(10));
  outfit.bodies.emplace_back(kKnightBody, Tinted(20));
  REQUIRE(OutfitBodyFor(outfit, kKnightBody) != nullptr);
  CHECK(OutfitBodyFor(outfit, kKnightBody)->ramps[0].hue == 20);
  // Un corps absent de la tenue : le repli, s'il y en a un.
  outfit.fallback_body = kKnightBody;
  CHECK(OutfitBodyFor(outfit, 1U)->ramps[0].hue == 20);
  outfit.fallback_body = 0;
  CHECK(OutfitBodyFor(outfit, 1U)->ramps[0].hue == 10);
}

TEST_CASE("Préréglages : un calque illisible tombe seul", "[app][style]") {
  TempDirectory client;
  std::vector<StyleLayerRecipe> layers = Outfit();
  // Un profil que la suite du corps n'emploie pas : le fil le refuse.
  layers[2].profile = kRampProfile;
  StylePresets written;
  REQUIRE(written.Save("bancal", Tinted(5), layers));
  REQUIRE(SaveStylePresets(client.path(), written));

  StylePresets read;
  LoadStylePresets(client.path(), read);
  REQUIRE(read.FindNamed("bancal") != nullptr);
  CHECK(read.FindNamed("bancal")->layers.size() == 3);
}

TEST_CASE("Préréglages : un préréglage d'avant se relit sans calques", "[app][style]") {
  TempDirectory client;
  const std::string text = std::string("presets:\n  - name: ancien\n    style: ") +
                           ro::sim::EncodeStyleCode(Tinted(10)) +
                           "\n  - name: abime\n    style: " + ro::sim::EncodeStyleCode(Tinted(20)) +
                           "\n    layers: zz01\n";
  REQUIRE(ro::core::WriteFileAtomically(StylePresetsPath(client.path()), text).has_value());

  StylePresets read;
  LoadStylePresets(client.path(), read);
  REQUIRE(read.all().size() == 2);
  CHECK(read.all()[0].layers.empty());
  // ⚠ UN CHAMP ABÎMÉ NE PERD PAS LE CORPS : seuls les calques tombent.
  CHECK(read.all()[1].recipe.ramps[0].hue == 20);
  CHECK(read.all()[1].layers.empty());
}

TEST_CASE("Préréglages : les calques se reportent sur ce qu'on porte", "[app][style]") {
  const std::vector<StyleLayerRecipe> kept = Outfit();

  SECTION("même tête, même corps : tout se garde, retouches comprises") {
    const std::vector<StyleLayerRecipe> now = RekeyPresetLayers(
        kept, PresetKeys{.head = kHeadKey, .body = kKnightBody}, PresetLayers::kAll);
    REQUIRE(now.size() == 4);
    CHECK(now[0].retouches.size() == 1);
  }

  SECTION("une autre tête et un autre corps : les clés suivent, les retouches tombent") {
    const std::vector<StyleLayerRecipe> now = RekeyPresetLayers(
        kept, PresetKeys{.head = kOtherHeadKey, .body = kSmithBody}, PresetLayers::kAll);
    REQUIRE(now.size() == 4);
    CHECK(now[0].key == kOtherHeadKey);
    CHECK(now[0].retouches.empty());
    CHECK(now[0].adjusts[0].hue == 30);
    CHECK(now[1].key == 0);
    CHECK(now[2].key == kSmithBody);
    CHECK(now[3].key == kSmithBody);
  }

  SECTION("le corps seul, puis la tête seule") {
    const PresetKeys keys{.head = kHeadKey, .body = kKnightBody};
    const std::vector<StyleLayerRecipe> body = RekeyPresetLayers(kept, keys, PresetLayers::kBody);
    REQUIRE(body.size() == 2);
    CHECK(body[0].layer == StyleLayerKind::kBodyMore);
    CHECK(body[1].layer == StyleLayerKind::kBodySkin);
    const std::vector<StyleLayerRecipe> head = RekeyPresetLayers(kept, keys, PresetLayers::kHead);
    REQUIRE(head.size() == 2);
    CHECK(head[0].layer == StyleLayerKind::kHead);
    CHECK(head[1].layer == StyleLayerKind::kSkin);
  }

  SECTION("une tenue reprise : les objets ne se teignent que de même apparence") {
    using moonclient::OutfitLayersNow;
    using moonclient::WornItem;
    const PresetKeys keys{.head = kHeadKey, .body = kKnightBody};
    const std::array<WornItem, 1> same{
        WornItem{.layer = StyleLayerKind::kHeadgear, .view = kHatView}};
    const std::vector<StyleLayerRecipe> with_hat = OutfitLayersNow(kept, keys, same);
    REQUIRE(with_hat.size() == 5);
    CHECK(with_hat.back().layer == StyleLayerKind::kHeadgear);
    CHECK(with_hat.back().key == kHatView);
    const std::array<WornItem, 1> other{
        WornItem{.layer = StyleLayerKind::kHeadgear, .view = kHatView + 1}};
    CHECK(OutfitLayersNow(kept, keys, other).size() == 4);
  }

  SECTION("sans corps connu, la suite et la peau du corps ne se posent pas") {
    const std::vector<StyleLayerRecipe> now =
        RekeyPresetLayers(kept, PresetKeys{.head = kHeadKey, .body = 0}, PresetLayers::kAll);
    CHECK(now.size() == 2);
  }
}
