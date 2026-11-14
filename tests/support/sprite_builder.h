#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::test_support {

struct SpriteColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  // Le quatrième octet d'une entrée de palette ne veut rien dire ; les outils
  // le laissent à zéro, et le constructeur fait pareil par défaut.
  std::uint8_t fourth = 0;
};

struct SprIndexedFrameSpec {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // width × height index, rangée 0 en HAUT.
  std::vector<std::uint8_t> indices;
};

struct SprBgraFrameSpec {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // width × height couleurs, rangée 0 en HAUT. Le constructeur se charge de
  // l'inversion des rangées et de l'ordre alpha, bleu, vert, rouge du format.
  std::vector<SpriteColor> pixels;
  // L'alpha de chaque pixel, une entrée par pixel ; vide vaut opaque.
  std::vector<std::uint8_t> alpha;
};

struct SprBuildOptions {
  std::uint8_t major = 2;
  std::uint8_t minor = 1;
  std::vector<SprIndexedFrameSpec> indexed;
  std::vector<SprBgraFrameSpec> bgra;
  // 256 entrées au plus ; le reste est complété de noir. Ignorée s'il n'y a
  // aucune image palettisée, comme dans le format.
  std::vector<SpriteColor> palette;
  // Octets ajoutés APRÈS la palette. Sert à fabriquer un fichier que le lecteur
  // doit refuser, la palette n'étant plus au dernier kilo-octet.
  std::size_t trailing_bytes = 0;
};

// Construit un .spr. La compression par plages est appliquée dès la version 2.1,
// comme le fait le format.
[[nodiscard]] std::vector<std::uint8_t> BuildSpr(const SprBuildOptions& options);

// Construit un .pal : 1024 octets, plus `trailing_bytes` de bourrage.
//
// Dix fichiers réels de l'installation portent quatre octets excédentaires.
[[nodiscard]] std::vector<std::uint8_t> BuildPal(const std::vector<SpriteColor>& colors,
                                                 std::size_t trailing_bytes = 0);

struct ActLayerSpec {
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  std::int32_t frame_index = 0;
  std::int32_t section = 0;
  bool mirrored = false;
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  float scale_x = 1.0F;
  float scale_y = 1.0F;
  std::int32_t rotation_degrees = 0;
  // Dimensions déclarées, écrites dès la version 2.5. Volontairement fausses
  // par défaut : le lecteur ne doit pas les croire.
  std::int32_t declared_width = -7;
  std::int32_t declared_height = -7;
};

struct ActAnchorSpec {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t attribute = 0;
};

struct ActFrameSpec {
  std::vector<ActLayerSpec> layers;
  std::vector<ActAnchorSpec> anchors;
  std::int32_t sound_index = -1;
};

struct ActActionSpec {
  std::vector<ActFrameSpec> frames;
};

struct ActBuildOptions {
  std::uint8_t major = 2;
  std::uint8_t minor = 5;
  std::vector<ActActionSpec> actions;
  std::vector<std::string> sound_files;
  // Une cadence par action. Moins d'entrées que d'actions produit une table
  // TRONQUÉE.
  std::vector<float> speeds;
  // Écrit un compte d'ancres absurde, que le lecteur doit refuser.
  //
  // Les fichiers 2.3 et 2.4 défectueux du jeu, eux, déclarent ZÉRO ancre tout
  // en émettant leurs octets : ils désalignent la lecture sans forcément la
  // faire échouer. Ce drapeau ne reproduit donc pas leur forme exacte — il
  // éprouve le CHEMIN DE REPLI, qui est commun aux deux.
  bool absurd_anchor_count = false;
};

// Construit un .act.
[[nodiscard]] std::vector<std::uint8_t> BuildAct(const ActBuildOptions& options);

}  // namespace ro::test_support
