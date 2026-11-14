#include "ro/render/actor_wardrobe.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ActorWardrobe;
using ro::render::AttachTo;
using ro::render::ComposedActor;
using ro::render::kAnimationDie;
using ro::render::kAnimationIdle;
using ro::render::kAnimationWalk;
using ro::render::kDirectionCount;
using ro::render::LoadSpriteCatalog;
using ro::render::PaletteRecipe;
using ro::render::RampAdjust;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildPal;
using ro::test_support::BuildSpr;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Utf8ToCp949;
using ro::vfs::Vfs;

using ro::test_support::AnimationBytes;
using ro::test_support::BodySpriteBytes;
using ro::test_support::kBag;
using ro::test_support::kBrokenHeadgear;
using ro::test_support::kFrameIntervalMs;
using ro::test_support::kFrameSpeed;
using ro::test_support::kFramesPerAction;
using ro::test_support::kGoggles;
using ro::test_support::kSummoner;
using ro::test_support::kSwordman;
using ro::test_support::kWings;
using ro::test_support::MakeClient;
using ro::test_support::Novice;
using ro::test_support::Poring;
using ro::test_support::SingleActionBytes;
using ro::test_support::SpriteBytes;

TEST_CASE("Garde-robe : un joueur reçoit ses pièces, et deux acteurs semblables les partagent",
          "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  look.headgears = {0, 0, kGoggles};
  look.body_palette = 7;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  REQUIRE(outfit->pieces().size() == 3);
  const ActorPiece& body = outfit->pieces()[0];
  const ActorPiece& head = outfit->pieces()[1];
  const ActorPiece& headgear = outfit->pieces()[2];
  CHECK(body.part == ActorPart::kBody);
  CHECK(body.palette != nullptr);
  CHECK(head.part == ActorPart::kHead);
  CHECK(head.is_head);
  CHECK(head.palette == nullptr);
  CHECK(headgear.part == ActorPart::kHeadgearTop);
  CHECK(headgear.attach == AttachTo::kHead);
  CHECK(wardrobe.missing().empty());

  // La même apparence est la même tenue : rien n'est rechargé ni recomposé.
  CHECK(wardrobe.OutfitFor(look) == outfit);

  ActorOutfit* poring = wardrobe.OutfitFor(Poring());
  REQUIRE(poring != nullptr);
  CHECK(poring->pieces().size() == 1);
  CHECK(poring->pieces()[0].palette == nullptr);
}

TEST_CASE("Garde-robe : une pièce absente n'emporte que ce qu'elle habille", "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // Une coiffe abîmée s'omet ; le reste s'habille.
  ActorLook look = Novice();
  look.headgears = {kBrokenHeadgear, 0, 0};
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  CHECK(outfit->pieces().size() == 2);
  REQUIRE(wardrobe.missing().size() == 1);
  CHECK(wardrobe.missing()[0] == "data\\sprite\\악세사리\\남\\남_casse");

  // Le corps absent emporte l'acteur : rien ne s'y ancrerait.
  ActorLook swordman = Novice();
  swordman.job = kSwordman;
  CHECK(wardrobe.OutfitFor(swordman) == nullptr);
  REQUIRE(wardrobe.missing().size() == 2);
  CHECK(wardrobe.missing()[1] == "data\\sprite\\인간족\\몸통\\남\\검사_남");

  // Redemander ne recharge pas et ne note pas deux fois.
  CHECK(wardrobe.OutfitFor(swordman) == nullptr);
  CHECK(wardrobe.OutfitFor(look) == outfit);
  CHECK(wardrobe.missing().size() == 2);
}

TEST_CASE("Garde-robe : le premier chemin qui existe l'emporte", "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // Les lunettes n'ont pas de variante Doram : le Doram porte les humaines,
  // les MÊMES, prises dans le même cache.
  ActorLook human = Novice();
  human.headgears = {0, 0, kGoggles};
  ActorLook doram = Novice();
  doram.job = kSummoner;
  doram.hair = 3;
  doram.headgears = {0, 0, kGoggles};
  ActorOutfit* human_outfit = wardrobe.OutfitFor(human);
  ActorOutfit* doram_outfit = wardrobe.OutfitFor(doram);
  REQUIRE(human_outfit != nullptr);
  REQUIRE(doram_outfit != nullptr);
  REQUIRE(human_outfit->pieces().size() == 3);
  REQUIRE(doram_outfit->pieces().size() == 3);
  CHECK(doram_outfit->pieces()[2].sprite == human_outfit->pieces()[2].sprite);
  CHECK(doram_outfit->pieces()[0].sprite != human_outfit->pieces()[0].sprite);
  // Le chemin Doram essayé en premier n'est pas une absence à signaler.
  CHECK(wardrobe.missing().empty());
}

TEST_CASE("Garde-robe : la composition se refait quand l'image change, pas avant",
          "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorOutfit* outfit = wardrobe.OutfitFor(Novice());
  REQUIRE(outfit != nullptr);
  const ComposedActor& first = outfit->Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE_FALSE(first.sources.empty());
  CHECK(first.sources[0].image_index == 0);
  // Avant la fin de la première image, la même composition sert encore.
  const ComposedActor& still = outfit->Composed(kAnimationIdle, 0, kFrameIntervalMs / 2.0F);
  CHECK(&still == &first);
  CHECK(still.sources[0].image_index == 0);
  // Passé l'intervalle, l'image suivante.
  const ComposedActor& next = outfit->Composed(kAnimationIdle, 0, kFrameIntervalMs * 1.5F);
  REQUIRE_FALSE(next.sources.empty());
  CHECK(next.sources[0].image_index == 1);

  // Une image TENUE ne se déduit plus du temps : c'est ainsi que le repos
  // garde l'orientation du regard au lieu de la faire défiler.
  const ComposedActor& held =
      outfit->Composed(kAnimationIdle, 0, kFrameIntervalMs * 10.0F, true, 1);
  REQUIRE_FALSE(held.sources.empty());
  CHECK(held.sources[0].image_index == 1);
  const ComposedActor& still_held =
      outfit->Composed(kAnimationIdle, 0, kFrameIntervalMs * 40.0F, true, 1);
  CHECK(still_held.sources[0].image_index == 1);

  // Un .act sans marche joue l'attente plutôt que de lire hors de sa table.
  ActorOutfit* poring = wardrobe.OutfitFor(Poring());
  REQUIRE(poring != nullptr);
  const ComposedActor& walking = poring->Composed(kAnimationWalk, 2, 0.0F);
  CHECK_FALSE(walking.quads.empty());
}

TEST_CASE("Garde-robe : deux acteurs semblables gardent chacun leur image", "[render][wardrobe]") {
  // 🔴 LA TENUE EST PARTAGÉE, ET LA SCÈNE GARDE UN POINTEUR vers chaque
  // composition. Composer le second acteur ne doit pas réécrire ce que le
  // premier a déjà reçu : sinon deux Porings qui marchent le font au même pas.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorOutfit* outfit = wardrobe.OutfitFor(Novice());
  REQUIRE(outfit != nullptr);
  const ComposedActor& first_frame = outfit->Composed(kAnimationIdle, 0, 0.0F);
  const ComposedActor& second = outfit->Composed(kAnimationIdle, 0, kFrameIntervalMs * 1.5F);
  REQUIRE_FALSE(first_frame.sources.empty());
  REQUIRE_FALSE(second.sources.empty());
  CHECK(&first_frame != &second);
  CHECK(first_frame.sources[0].image_index == 0);
  CHECK(second.sources[0].image_index == 1);
  // Et redemander la première image rend LA MÊME composition, sans refaire.
  CHECK(&outfit->Composed(kAnimationIdle, 0, 0.0F) == &first_frame);
}

TEST_CASE("Garde-robe : ce que les mains tiennent manque sans un mot", "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // Le couteau existe ; sa traînée, non — et ce n'est pas une absence à
  // signaler, le client la sonde et n'en dit rien.
  ActorLook look = Novice();
  look.weapon = 1201;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  REQUIRE(outfit->pieces().size() == 3);
  CHECK(outfit->pieces()[2].part == ActorPart::kWeapon);
  CHECK(outfit->pieces()[2].attach == AttachTo::kBody);
  CHECK(wardrobe.missing().empty());
}

TEST_CASE(
    "Garde-robe : la cape lit ses deux fichiers à deux chemins, et passe derrière quand sa "
    "table le dit",
    "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  look.robe = kWings;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  REQUIRE(outfit->pieces().size() == 3);
  const ActorPiece& garment = outfit->pieces()[2];
  CHECK(garment.part == ActorPart::kGarment);
  CHECK(garment.attach == AttachTo::kBody);
  CHECK(wardrobe.missing().empty());

  // De face à l'arrêt, la cape se dessine la première ; de dos, la dernière.
  const ComposedActor& front = outfit->Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE_FALSE(front.sources.empty());
  CHECK(front.sources.front().sprite == garment.sprite);
  const ComposedActor& back = outfit->Composed(kAnimationIdle, 4, 0.0F);
  REQUIRE_FALSE(back.sources.empty());
  CHECK(back.sources.back().sprite == garment.sprite);

  // Un sac sans .act ne s'habille pas, et ne se signale pas.
  look.robe = kBag;
  ActorOutfit* bagged = wardrobe.OutfitFor(look);
  REQUIRE(bagged != nullptr);
  CHECK(bagged->pieces().size() == 2);
  CHECK(wardrobe.missing().empty());
}

TEST_CASE("Garde-robe : monté, un joueur change de corps et lâche ce qu'il tient",
          "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  look.weapon = 1201;
  ActorOutfit* walking = wardrobe.OutfitFor(look);
  REQUIRE(walking != nullptr);
  REQUIRE(walking->pieces().size() == 3);

  // L'état fait une autre tenue : un autre corps, la même tête, plus de
  // couteau.
  look.cash_mount = true;
  ActorOutfit* riding = wardrobe.OutfitFor(look);
  REQUIRE(riding != nullptr);
  CHECK(riding != walking);
  REQUIRE(riding->pieces().size() == 2);
  CHECK(riding->pieces()[0].sprite != walking->pieces()[0].sprite);
  CHECK(riding->pieces()[1].sprite == walking->pieces()[1].sprite);
  CHECK(wardrobe.missing().empty());
}

TEST_CASE(
    "Tenue : jouée une fois, une animation tient sa dernière image, et sa durée se "
    "connaît",
    "[render][wardrobe]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  // Neuf animations : de quoi porter la mort d'un joueur, au rang huit.
  auto animation = ro::formats::ActAnimation::Parse(AnimationBytes(9));
  REQUIRE(animation.has_value());
  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});

  CHECK(outfit.AnimationDurationMs(kAnimationDie) == kFramesPerAction * kFrameIntervalMs);
  CHECK(outfit.AnimationDurationMs(20) == 0.0F);

  const ComposedActor& fallen = outfit.Composed(kAnimationDie, 0, 10.0F * kFrameIntervalMs, false);
  REQUIRE_FALSE(fallen.sources.empty());
  CHECK(fallen.sources[0].image_index == kFramesPerAction - 1);
  // En boucle, la même durée ramène à la première image.
  const ComposedActor& looping = outfit.Composed(kAnimationDie, 0, 10.0F * kFrameIntervalMs, true);
  REQUIRE_FALSE(looping.sources.empty());
  CHECK(looping.sources[0].image_index == 0);
  // Une animation que le .act ne porte pas joue l'attente.
  CHECK_FALSE(outfit.Composed(20, 0, 0.0F).quads.empty());
}

TEST_CASE("Garde-robe : un objet au sol n'a qu'une action, vue de partout pareil",
          "[render][wardrobe]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook potion;
  potion.item = true;
  potion.job = 501;
  ActorOutfit* outfit = wardrobe.OutfitFor(potion);
  REQUIRE(outfit != nullptr);
  CHECK(outfit->pieces().size() == 1);
  // Quelle que soit la direction ou l'animation demandée, la même image.
  CHECK_FALSE(outfit->Composed(kAnimationIdle, 3, 0.0F).quads.empty());
  CHECK_FALSE(outfit->Composed(kAnimationWalk, 5, 1000.0F).quads.empty());
  CHECK(outfit->AnimationDurationMs(kAnimationIdle) == kFrameIntervalMs);
  CHECK(wardrobe.missing().empty());

  // Un objet que la table ne connaît pas ne se montre pas.
  potion.job = 999;
  CHECK(wardrobe.OutfitFor(potion) == nullptr);
}

TEST_CASE("Garde-robe : l'ombre se rend à part, à l'échelle de la classe", "[render][wardrobe]") {
  // Le client dessine toujours la même ombre, à l'échelle de la classe, et
  // avant tout le reste. Elle vit à part de la composition : « le premier
  // calque » veut dire quelque chose — c'est là que passe une cape mise
  // derrière —, et y glisser l'ombre changerait ce sens sans le dire.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorOutfit* novice = wardrobe.OutfitFor(Novice());
  REQUIRE(novice != nullptr);
  // La composition ne porte que le corps et la tête.
  CHECK(novice->Composed(kAnimationIdle, 0, 0.0F).quads.size() == 2);

  const ComposedActor& shadow = novice->Shadow();
  REQUIRE(shadow.quads.size() == 1);
  REQUIRE(shadow.sources.size() == 1);
  const float width = shadow.quads[0].corners[1].x - shadow.quads[0].corners[0].x;

  // Un Poring de rayon un porte une ombre trois fois plus large : deux fois le
  // rayon, plus un.
  ActorOutfit* poring = wardrobe.OutfitFor(Poring());
  REQUIRE(poring != nullptr);
  REQUIRE(poring->Shadow().quads.size() == 1);
  const float blob =
      poring->Shadow().quads[0].corners[1].x - poring->Shadow().quads[0].corners[0].x;
  CHECK(blob == 3.0F * width);
}

TEST_CASE("Garde-robe : l'échelle de corps rétrécit l'acteur entier", "[render][wardrobe]") {
  // Le client multiplie l'échelle d'écran de TOUS les quads de l'acteur par
  // celle de sa classe, décalages d'ancrage compris : un bébé aux trois quarts
  // a la tête aux trois quarts, et posée aux trois quarts de la hauteur.
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto animation = ro::formats::ActAnimation::Parse(AnimationBytes(1));
  REQUIRE(animation.has_value());
  const ro::render::LoadedActorSprite body{std::move(*sprite), std::move(*animation)};
  const std::vector<ActorPiece> pieces = {
      ro::render::DescribeActorPiece(body, ActorPart::kBody, nullptr)};

  ActorOutfit adult(pieces);
  ActorOutfit baby(pieces, {}, {}, 0.0F, 0.75F);
  const ComposedActor& grown = adult.Composed(kAnimationIdle, 0, 0.0F);
  const ComposedActor& small = baby.Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE(grown.quads.size() == 1);
  REQUIRE(small.quads.size() == 1);
  for (std::size_t corner = 0; corner < 4; ++corner) {
    CHECK(small.quads[0].corners[corner].x == 0.75F * grown.quads[0].corners[corner].x);
    CHECK(small.quads[0].corners[corner].y == 0.75F * grown.quads[0].corners[corner].y);
  }
}

TEST_CASE("Garde-robe : l'ombre suit l'échelle du corps", "[render][wardrobe]") {
  // Le client multiplie l'échelle du sprite de l'acteur par celle de sa classe
  // pour dimensionner l'ombre. Un bébé, plus petit, projette donc une ombre
  // plus petite ; sans cela, l'ombre déborderait de son porteur.
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto animation = ro::formats::ActAnimation::Parse(AnimationBytes(1));
  REQUIRE(animation.has_value());
  const ro::render::LoadedActorSprite body{std::move(*sprite), std::move(*animation)};
  const std::vector<ActorPiece> pieces = {
      ro::render::DescribeActorPiece(body, ActorPart::kBody, nullptr)};
  const ActorPiece shadow = ro::render::DescribeActorPiece(body, ActorPart::kBody, nullptr);

  ActorOutfit adult(pieces, {}, shadow, 1.0F);
  ActorOutfit baby(pieces, {}, shadow, 1.0F, 0.75F);
  REQUIRE(adult.Shadow().quads.size() == 1);
  REQUIRE(baby.Shadow().quads.size() == 1);
  for (std::size_t corner = 0; corner < 4; ++corner) {
    CHECK(baby.Shadow().quads[0].corners[corner].x ==
          0.75F * adult.Shadow().quads[0].corners[corner].x);
    CHECK(baby.Shadow().quads[0].corners[corner].y ==
          0.75F * adult.Shadow().quads[0].corners[corner].y);
  }
}

TEST_CASE("Garde-robe : un objet au sol porte une petite ombre", "[render][wardrobe]") {
  // Quatre dixièmes de celle d'un personnage, quel que soit l'objet.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook lying;
  lying.item = true;
  lying.job = 501;
  ActorOutfit* potion = wardrobe.OutfitFor(lying);
  REQUIRE(potion != nullptr);
  ActorOutfit* novice = wardrobe.OutfitFor(Novice());
  REQUIRE(novice != nullptr);
  REQUIRE(potion->Shadow().quads.size() == 1);
  REQUIRE(novice->Shadow().quads.size() == 1);
  const auto width = [](const ComposedActor& shadow) {
    return shadow.quads[0].corners[1].x - shadow.quads[0].corners[0].x;
  };
  CHECK(width(potion->Shadow()) == Catch::Approx(0.4F * width(novice->Shadow())));
}
