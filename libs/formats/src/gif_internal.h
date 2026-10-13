#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/formats/image.h"

// LES PIÈCES COMMUNES AUX DEUX ENCODEURS GIF : les octets du format, l'écriture
// des codes, le LZW et la palette globale.
//
// En-tête PRIVÉ : il vit dans `src/` parce que personne hors de cette
// bibliothèque n'a à écrire un flux LZW. Les deux encodeurs — celui des
// captures d'écran, opaque et différentiel, et celui du pantin, à fond
// transparent — ne diffèrent que par la façon dont ils découpent leurs images ;
// tout ce qui touche au format lui-même vit ici, une seule fois.
namespace ro::formats::gif_internal {

// ── LES OCTETS DU FORMAT ────────────────────────────────────────────────────
// Ce sont des valeurs de la spécification GIF89a, pas des choix : elles portent
// leur nom pour qu'une relecture n'ait pas à ouvrir le document.
inline constexpr std::array<std::uint8_t, 6> kSignature = {'G', 'I', 'F', '8', '9', 'a'};
inline constexpr std::uint8_t kExtensionIntroducer = 0x21;
inline constexpr std::uint8_t kGraphicControlLabel = 0xF9;
inline constexpr std::uint8_t kApplicationLabel = 0xFF;
inline constexpr std::uint8_t kImageSeparator = 0x2C;
inline constexpr std::uint8_t kTrailer = 0x3B;
inline constexpr std::uint8_t kBlockTerminator = 0x00;
// La longueur du bloc de contrôle graphique : drapeaux, délai, index transparent.
inline constexpr std::uint8_t kGraphicControlLength = 0x04;

// Table globale présente, 8 bits par couleur d'origine, 256 entrées.
inline constexpr std::uint8_t kGlobalTableFlags = 0xF7;
// Le drapeau « cet index est transparent » du bloc de contrôle graphique.
inline constexpr std::uint8_t kTransparentFlag = 0x01;
// Le rang des bits de disposition dans les drapeaux du bloc de contrôle.
inline constexpr int kDisposalShift = 2;
// L'index réservé à la transparence : la première entrée de la table.
inline constexpr std::uint8_t kTransparentIndex = 0;
// Une table de 256 entrées se code sur 8 bits.
inline constexpr int kGlobalMinCodeSize = 8;
// Un bloc de données ne porte que 255 octets : c'est un compte sur un octet, et
// zéro termine la suite.
inline constexpr std::size_t kLargestBlock = 255;

// La boucle sans fin, telle que Netscape l'a introduite et que tout le monde lit
// depuis. ⚠ SANS ELLE, LE GIF NE JOUE QU'UNE FOIS.
inline constexpr std::array<std::uint8_t, 19> kLoopForever = {kExtensionIntroducer,
                                                              kApplicationLabel,
                                                              0x0B,
                                                              'N',
                                                              'E',
                                                              'T',
                                                              'S',
                                                              'C',
                                                              'A',
                                                              'P',
                                                              'E',
                                                              '2',
                                                              '.',
                                                              '0',
                                                              0x03,
                                                              0x01,
                                                              0x00,
                                                              0x00,
                                                              0x00};

// ── L'HISTOGRAMME ───────────────────────────────────────────────────────────
// Les couleurs sont d'abord rangées dans une grille de 5 bits par canal : 32 768
// cases, assez petit pour se parcourir plusieurs fois, assez fin pour que
// l'arrondi ne se voie pas une fois le tout ramené à 255 couleurs.
inline constexpr int kChannelBits = 5;
inline constexpr int kChannelSteps = 1 << kChannelBits;  // 32
inline constexpr int kHistogramCells = kChannelSteps * kChannelSteps * kChannelSteps;
inline constexpr int kLargestStep = kChannelSteps - 1;

void PutU16(std::vector<std::uint8_t>& out, unsigned value);

[[nodiscard]] constexpr std::uint16_t CellOf(std::uint8_t red, std::uint8_t green,
                                             std::uint8_t blue) noexcept {
  const unsigned shift = 8 - kChannelBits;
  return static_cast<std::uint16_t>(((red >> shift) << (2 * kChannelBits)) |
                                    ((green >> shift) << kChannelBits) | (blue >> shift));
}

// ── L'ÉCRITURE DES CODES ────────────────────────────────────────────────────
//
// Les codes LZW sortent bit de poids FAIBLE en tête, et la suite se découpe en
// blocs d'au plus 255 octets précédés de leur longueur.
class CodeWriter {
 public:
  explicit CodeWriter(std::vector<std::uint8_t>& out) : out_(out) {}

  void Write(unsigned code, int bits);
  // Les bits qui restent, le bloc entamé, puis le terminateur.
  void Finish();

 private:
  void PushByte(std::uint8_t value);

  std::vector<std::uint8_t>& out_;
  std::vector<std::uint8_t> block_;
  unsigned buffer_ = 0;
  int held_ = 0;
};

// LZW de GIF, dans sa forme standard. Voir `gif.cpp`.
void EmitLzw(CodeWriter& writer, std::span<const std::uint8_t> indices, int min_code_size);

// ── LA PALETTE GLOBALE ──────────────────────────────────────────────────────

// La table globale, et le chemin direct d'une case vers son entrée.
//
// ⚠ L'ENTRÉE ZÉRO N'EST JAMAIS UNE COULEUR : elle appartient à la transparence,
// et une case que la quantification n'a pas vue y mène.
struct Palette {
  std::array<std::uint8_t, 256 * 3> colors = {};
  std::vector<std::uint8_t> of_cell;
};

// LE SEUIL D'OPACITÉ : en deçà, un pixel est transparent. Le GIF n'a pas de
// translucidité — un pixel est là ou il n'y est pas —, et la moitié est le seul
// partage qui ne favorise aucun des deux côtés.
inline constexpr std::uint8_t kOpaqueAlpha = 128;

// Quantifie les images en une palette unique, par coupe médiane.
//
// `opaque_only` écarte de l'histogramme les pixels sous `kOpaqueAlpha` : ils
// seront transparents, et leur couleur ne doit pas voler d'entrée.
[[nodiscard]] Palette BuildPalette(std::span<const Image> frames, bool opaque_only);

// L'en-tête, l'écran logique, la table globale et la boucle sans fin.
void PutHeader(std::vector<std::uint8_t>& out, int width, int height, const Palette& palette);

// Un bloc de contrôle graphique.
void PutGraphicControl(std::vector<std::uint8_t>& out, std::uint8_t flags, int delay_centiseconds);

// Un descripteur d'image sans table locale, puis ses index compressés.
void PutImage(std::vector<std::uint8_t>& out, int left, int top, int width, int height,
              std::span<const std::uint8_t> indices);

}  // namespace ro::formats::gif_internal
