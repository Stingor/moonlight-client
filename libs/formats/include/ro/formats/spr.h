#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/formats/palette.h"

namespace ro::formats {

// Une image palettisée du .spr : un index de palette par pixel.
//
// Les index sont CONSERVÉS plutôt que résolus en couleurs, parce que la couleur
// n'est pas une propriété du sprite : cheveux et vêtements se recolorent avec
// un .pal externe. Résoudre trop tôt obligerait à relire le fichier pour chaque
// teinte, et coûterait quatre octets par pixel au lieu d'un.
struct SprIndexedFrame {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // width × height entrées, rangée 0 en HAUT.
  std::vector<std::uint8_t> indices;
};

// Un fichier .spr : deux sections d'images, plus une palette embarquée.
//
// Le format admet beaucoup de variantes ; le recensement des 113 644 .spr de
// l'installation dit lesquelles existent :
//
//   version 2.1   113 425      palettisées seules   112 209
//   version 2.0       219      palettisées + BGRA       858
//                              BGRA seules              577
//
// Ni 2.2 ni 3.2 n'apparaissent. Deux conséquences directes : la longueur d'une
// image compressée est toujours un u16 (le i32 arrive en 2.2), et la section
// BGRA n'est jamais compressée par zlib (ce qui arrive en 3.2). La 3.2 est
// refusée explicitement plutôt que devinée sans pouvoir l'éprouver.
class SprSprite {
 public:
  [[nodiscard]] static core::Result<SprSprite> Parse(std::span<const std::uint8_t> data);

  // Version condensée : majeure × 10 + mineure, donc 21 pour la 2.1.
  [[nodiscard]] std::int32_t version() const noexcept { return version_; }

  // Les deux sections, dans l'ordre du fichier. Un calque de .act indexe LA
  // SIENNE, pas la concaténation des deux : les confondre affiche la mauvaise
  // image sans jamais sortir des bornes.
  [[nodiscard]] const std::vector<SprIndexedFrame>& indexed_frames() const noexcept {
    return indexed_frames_;
  }
  // Déjà converties en RGBA8, rangée 0 en haut : le format les range à
  // l'envers, et c'est au lecteur de format de le corriger.
  [[nodiscard]] const std::vector<Image>& bgra_frames() const noexcept { return bgra_frames_; }

  // La palette embarquée, vide si le fichier n'a aucune image palettisée.
  [[nodiscard]] const std::optional<Palette>& palette() const noexcept { return palette_; }

  // Vrai si le fichier annonçait plus d'images qu'il n'en contient.
  //
  // Un seul .spr de l'installation est dans ce cas — un effet qui déclare 21
  // images BGRA et s'arrête au milieu de la vingtième. Les images déjà lues
  // sont conservées, parce que perdre un effet entier pour une image manquante
  // serait pire ; mais rien n'est tu, et la palette d'un fichier tronqué n'est
  // PAS fiable : le dernier kilo-octet n'y est plus une palette.
  //
  // Un fichier qui ne livre AUCUNE image exploitable échoue, lui : il n'y a
  // rien à sauver et le taire masquerait un vrai défaut.
  [[nodiscard]] bool truncated() const noexcept { return truncated_; }

 private:
  std::int32_t version_ = 0;
  bool truncated_ = false;
  std::vector<SprIndexedFrame> indexed_frames_;
  std::vector<Image> bgra_frames_;
  std::optional<Palette> palette_;
};

// Résout une image palettisée en RGBA8 sous la palette donnée.
//
// Deux conventions RO, toutes deux nécessaires :
//
// ⚠ Le quatrième octet d'une entrée ne veut rien dire — les outils le laissent
// à zéro. S'y fier rendrait la quasi-totalité des sprites invisibles ; l'opacité
// est donc forcée.
//
// ⚠ L'index 0 sort NOIR transparent, et non « sa couleur, en transparent ».
// Son entrée porte une couleur bien réelle, souvent un vert vif, et un texel
// transparent participe quand même au filtrage : à toute échelle non entière,
// cette couleur bave sur la silhouette et dessine un halo. C'est le même piège
// que le magenta des textures face aux mipmaps.
[[nodiscard]] Image ApplyPalette(const SprIndexedFrame& frame, const Palette& palette);

}  // namespace ro::formats
