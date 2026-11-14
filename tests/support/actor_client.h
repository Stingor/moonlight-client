#pragma once

// UN CLIENT SYNTHÉTIQUE POUR HABILLER QUELQU'UN : les tables sur le disque,
// les sprites dans une archive, aux chemins exacts que le catalogue produit.
//
// ⚠ PARTAGÉ PAR DEUX ESSAIS — l'habillage et les couleurs —, parce qu'ils
// posent leurs questions au MÊME client. Deux copies de ce montage auraient
// divergé au premier chemin ajouté, et l'un des deux aurait alors éprouvé une
// installation qui n'existe nulle part.
//
// ⚠ AUCUN ASSET DU JEU : tout est bâti en mémoire, comme partout ailleurs dans
// cette suite.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/sprite_catalog.h"
#include "ro/vfs/encoding.h"

#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

namespace ro::test_support {

using ro::render::ActorLook;
using ro::render::kDirectionCount;
using ro::vfs::Utf8ToCp949;

// La cadence des animations d'essai, en unités du fichier : quatre, soit
// quatre-vingt-seize millisecondes par image.
inline constexpr float kFrameSpeed = 4.0F;
inline constexpr float kFrameIntervalMs = 96.0F;
inline constexpr std::int32_t kFramesPerAction = 2;

// Un sprite de deux images de deux pixels, palettisé.
inline std::vector<std::uint8_t> SpriteBytes() {
  SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}, {2, 2, {2, 2, 2, 2}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}};
  return BuildSpr(options);
}

// Une animation : `animations` groupes de huit actions, deux images chacune.
inline std::vector<std::uint8_t> AnimationBytes(std::size_t animations) {
  ActBuildOptions options;
  for (std::size_t action = 0; action < animations * kDirectionCount; ++action) {
    ActActionSpec spec;
    for (std::int32_t frame = 0; frame < kFramesPerAction; ++frame) {
      ActFrameSpec frame_spec;
      ActLayerSpec layer;
      layer.frame_index = frame;
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ActAnchorSpec{});
      spec.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(kFrameSpeed);
  }
  return BuildAct(options);
}

// Le .act d'un objet au sol : une action, une image.
inline std::vector<std::uint8_t> SingleActionBytes() {
  ActFrameSpec frame_spec;
  ActLayerSpec layer;
  layer.frame_index = 0;
  frame_spec.layers.push_back(layer);
  frame_spec.anchors.push_back(ActAnchorSpec{});
  ActActionSpec spec;
  spec.frames.push_back(std::move(frame_spec));
  ActBuildOptions options;
  options.actions.push_back(std::move(spec));
  options.speeds.push_back(kFrameSpeed);
  return BuildAct(options);
}

// UN CORPS ASSEZ GRAND POUR QUE SES RAMPES COMPTENT.
//
// 🔴 LE SEUIL EST DE DEUX CENTS PIXELS PAR RAMPE, et deux images de deux
// pixels n'en font que huit : sur le sprite ordinaire de ces essais, AUCUNE
// rampe n'est détectée, donc une recette n'y change rien. Un essai de style
// bâti dessus passerait en comparant deux fois la même palette intacte — il a
// commencé ainsi, et c'est le cas « la recette a bien mordu » qui l'a dit.
//
// Seize sur seize, deux images : cinq cent douze pixels étalés sur un dégradé
// de huit index, ce qui franchit le seuil d'une seule rampe.
inline constexpr std::int32_t kBodySide = 16;
inline constexpr std::uint8_t kBodyShades = 8;

inline std::vector<std::uint8_t> BodySpriteBytes() {
  SprBuildOptions options;
  for (int frame = 0; frame < 2; ++frame) {
    SprIndexedFrameSpec spec;
    spec.width = kBodySide;
    spec.height = kBodySide;
    spec.indices.reserve(static_cast<std::size_t>(kBodySide) * kBodySide);
    for (std::int32_t pixel = 0; pixel < kBodySide * kBodySide; ++pixel) {
      // Les index 1 à 8, à parts égales : l'index zéro reste le transparent.
      spec.indices.push_back(static_cast<std::uint8_t>(1 + (pixel % kBodyShades)));
    }
    options.indexed.push_back(std::move(spec));
  }
  // Un dégradé ROUGE qui descend du clair au foncé : la forme d'une pièce de
  // costume, et donc une rampe que la détection reconnaît.
  options.palette.push_back({0, 0, 0});
  for (std::uint8_t shade = 0; shade < kBodyShades; ++shade) {
    const auto level = static_cast<std::uint8_t>(240 - shade * 25);
    options.palette.push_back({level, static_cast<std::uint8_t>(level / 6), 0});
  }
  return BuildSpr(options);
}

inline GrfFileSpec Entry(std::string_view utf8_name, std::vector<std::uint8_t> content) {
  auto cp949 = Utf8ToCp949(utf8_name);
  REQUIRE(cp949.has_value());
  return GrfFileSpec{*cp949, std::move(content)};
}

// Un client : les tables sur le disque, les sprites dans une archive, aux
// chemins que le catalogue produit pour un novice, un Doram et un Poring.
// `body` remplace le sprite du CORPS du novice, et lui seul : les essais de
// style en veulent un dont les rampes comptent, les autres se contentent du
// sprite minuscule qui rend leurs assertions lisibles.
inline void MakeClient(const TempDirectory& client,
                       std::vector<std::uint8_t> body = SpriteBytes()) {
  // Le Poring encombre une cellule de rayon : son ombre est trois fois celle
  // d'un joueur.
  client.WriteText("data/yaml_files/class_sprites.yml",
                   "version: 1\nclasses:\n  1002: {sprite: \"Poring\", radius: 1}\n");
  client.WriteText("data/yaml_files/headgear_sprites.yml",
                   "version: 1\nheadgears:\n  1: {symbol: \"GOGGLES\", sprite: \"_고글\"}\n"
                   "  2: {symbol: \"BROKEN\", sprite: \"_casse\"}\n");
  client.WriteText("data/yaml_files/weapon_sprites.yml",
                   "version: 1\nweapons:\n  1: {sprite: \"_dagger\"}\n");
  // Deux capes, et la table qui met celle du novice derrière lui, de face, à
  // l'arrêt.
  client.WriteText("data/yaml_files/garment_sprites.yml",
                   "version: 1\ngarments:\n  1: {symbol: \"WINGS\", sprite: \"ailes\"}\n"
                   "  2: {symbol: \"BAG\", sprite: \"sac\"}\n");
  client.WriteText("data/yaml_files/garment_layers.yml", "version: 1\nbig_male:\n  0: {0: [0]}\n");
  // La table des objets : le couteau est une dague.
  client.WriteText("data/yaml_files/item_info.yml",
                   "version: 1\nitems: {1201: {name: \"Knife\", view: 1}, "
                   "501: {name: \"Red Potion\", resource: \"potion\"}}\n");

  std::vector<GrfFileSpec> files;
  for (const std::string_view base :
       {"data\\sprite\\인간족\\몸통\\남\\초보자_남", "data\\sprite\\인간족\\머리통\\남\\2_남",
        "data\\sprite\\인간족\\몸통\\남\\노비스포링_남", "data\\sprite\\악세사리\\남\\남_고글",
        "data\\sprite\\도람족\\몸통\\남\\summoner_남", "data\\sprite\\도람족\\머리통\\남\\3_남",
        "data\\sprite\\인간족\\초보자\\초보자_남_dagger"}) {
    const bool is_novice_body = base == "data\\sprite\\인간족\\몸통\\남\\초보자_남";
    files.push_back(Entry(std::string(base) + ".spr", is_novice_body ? body : SpriteBytes()));
    files.push_back(Entry(std::string(base) + ".act", AnimationBytes(2)));
  }
  // L'ombre, commune à tout le monde : une action, une image.
  files.push_back(Entry("data\\sprite\\shadow.spr", SpriteBytes()));
  files.push_back(Entry("data\\sprite\\shadow.act", SingleActionBytes()));
  // Le Poring ne porte que l'attente.
  files.push_back(Entry("data\\sprite\\몬스터\\Poring.spr", SpriteBytes()));
  files.push_back(Entry("data\\sprite\\몬스터\\Poring.act", AnimationBytes(1)));
  // Une coiffe abîmée : son .act manque.
  files.push_back(Entry("data\\sprite\\악세사리\\남\\남_casse.spr", SpriteBytes()));
  // Une potion au sol : une seule action, une seule image.
  files.push_back(Entry("data\\sprite\\아이템\\potion.spr", SpriteBytes()));
  files.push_back(Entry("data\\sprite\\아이템\\potion.act", SingleActionBytes()));
  // Les ailes : leur .spr à plat, leur .act par métier — là où le client les
  // cherche. Le sac n'a pas de .act.
  files.push_back(Entry("data\\sprite\\로브\\ailes\\ailes.spr", SpriteBytes()));
  files.push_back(Entry("data\\sprite\\로브\\ailes\\남\\초보자_남.act", AnimationBytes(2)));
  files.push_back(Entry("data\\sprite\\로브\\sac\\sac.spr", SpriteBytes()));
  files.push_back(
      Entry("data\\palette\\몸\\body_7.pal", BuildPal({{0, 0, 0}, {0, 0, 255}, {0, 0, 255}})));
  // UNE TEINTE QUI NE DÉFINIT QU'UN SEUL INDEX, et c'est la forme des vraies :
  // une palette officielle laisse en noir tout ce qu'elle ne peint pas, et
  // c'est la FUSION qui rapatrie le reste du sprite. Sans elle, un corps de
  // troisième ou quatrième classe sort à moitié noir.
  files.push_back(Entry("data\\palette\\몸\\body_8.pal", BuildPal({{0, 0, 0}, {0, 0, 255}})));
  // UNE COULEUR DE CHEVEUX TROUÉE, de la même façon : elle ne peint que l'index
  // un, et laisse vide l'index deux que la tête emploie aussi.
  files.push_back(Entry("data\\palette\\머리\\head_5.pal", BuildPal({{0, 0, 0}, {0, 0, 255}})));
  client.WriteFile("sprites.grf", BuildGrf(files));
}

inline ActorLook Novice() {
  ActorLook look;
  look.player = true;
  look.job = 0;
  look.sex = 1;
  look.hair = 1;
  return look;
}

inline ActorLook Poring() {
  ActorLook look;
  look.job = 1002;
  return look;
}

// La couleur de cheveux trouée : voir `MakeClient`.
inline constexpr std::int16_t kHoledHairPalette = 5;
inline constexpr std::uint16_t kGoggles = 1;
inline constexpr std::uint16_t kBrokenHeadgear = 2;
inline constexpr std::uint32_t kSwordman = 1;
inline constexpr std::uint16_t kWings = 1;
inline constexpr std::uint16_t kBag = 2;
inline constexpr std::uint32_t kSummoner = 4218;

}  // namespace ro::test_support
