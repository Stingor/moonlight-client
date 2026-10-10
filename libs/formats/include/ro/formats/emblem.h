#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// ── L'EMBLÈME DE GUILDE ─────────────────────────────────────────────────────
//
// Un BMP de 24 × 24 pixels, et le jeu n'en rend pas d'autre taille. Le magenta
// pur y vaut « transparent », comme partout dans l'interface du client.
//
// 🔴 LE SERVEUR NE DÉCODE RIEN, IL COMPTE. Son service web vérifie la signature,
// la taille annoncée par l'en-tête et un taux de pixels magenta ; il ne regarde
// ni les dimensions ni la profondeur. Ce que le jeu n'afficherait pas, il
// l'accepte quand même : c'est donc ICI que se refuse un fichier hors format,
// avant l'envoi, et le motif se dit au joueur — un emblème rejeté en silence
// est indiscernable d'un serveur muet.

inline constexpr int kEmblemSide = 24;
inline constexpr std::size_t kEmblemPixelCount =
    static_cast<std::size_t>(kEmblemSide) * static_cast<std::size_t>(kEmblemSide);

// Un BMP 24 × 24 en 24 bits pèse 1782 octets. Le plafond n'écarte que ce qui
// est manifestement hors format.
inline constexpr std::size_t kEmblemMostBytes = 1800;

// LE PLUS GRAND CÔTÉ QU'UN EMBLÈME REÇU PEUT AVOIR, jugé sur son en-tête AVANT
// tout décodage.
//
// 🔴 UNE BORNE DE SÛRETÉ, PAS UNE RÈGLE DU JEU. Le service accepte un GIF de
// n'importe quelle dimension sous son poids, et un en-tête de quelques octets
// peut en annoncer seize mille de côté : des gigaoctets à allouer chez chaque
// joueur qui voit un membre de cette guilde. Un emblème en fait vingt-quatre ;
// au-delà de quatre fois, il se refuse sans être décodé.
inline constexpr int kEmblemMostSide = 4 * kEmblemSide;

// LA COULEUR « VIDE » : le magenta pur, `0xFF00FF`, que le jeu rend transparent.
inline constexpr std::uint32_t kEmblemClearColor = 0xFF00FFU;

// Les 576 pixels d'un emblème en `0xRRGGBB`, rangée zéro EN HAUT.
using EmblemPixels = std::array<std::uint32_t, kEmblemPixelCount>;

// POURQUOI UN BMP CANDIDAT SERAIT REFUSÉ, dans l'ordre où le serveur ou le jeu
// le refuserait.
enum class EmblemFault : std::uint8_t {
  kNone,
  // Moins que l'en-tête d'un BMP.
  kTooShort,
  // La signature « BM » manque.
  kNotBmp,
  // 🔴 LA TAILLE ANNONCÉE PAR L'EN-TÊTE N'EST PAS CELLE DU FICHIER. Le serveur
  // les compare, et rejette sans un mot : c'est le défaut le plus fréquent d'un
  // BMP passé par un convertisseur.
  kDeclaredSize,
  // Les pixels commenceraient hors du fichier.
  kPixelsOutside,
  // Pas du 24 × 24 : le jeu ne l'afficherait pas.
  kWrongSize,
  // Ni 24 bits, ni 256 couleurs.
  kDepth,
  // Compression par plages.
  kCompressed,
  // Plus lourd que ce qu'un emblème pèse.
  kTooHeavy,
};

struct EmblemVerdict {
  EmblemFault fault = EmblemFault::kNone;
  // Les dimensions lues, pour dire lesquelles quand elles sont fausses.
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::size_t bytes = 0;

  [[nodiscard]] bool usable() const noexcept { return fault == EmblemFault::kNone; }
};

[[nodiscard]] EmblemVerdict CheckEmblemBmp(std::span<const std::uint8_t> bmp);

// LE TAUX DE TRANSPARENCE, TEL QUE LE SERVEUR LE CALCULE.
//
// ⚠ SA FORMULE EST APPROXIMATIVE, ET ELLE EST REPRISE TELLE QUELLE : il compte
// les PAIRES de pixels magenta consécutifs, lues par fenêtres de quatre octets
// alignées sur l'index absolu, et rapporte leur nombre, multiplié par trois
// cents, à la taille des pixels. Un calcul « juste » donnerait un
// avertissement qui ne correspondrait plus au verdict réel.
[[nodiscard]] int EmblemTransparencyPercent(std::span<const std::uint8_t> bmp);

// LE SEUIL AU-DELÀ DUQUEL LE SERVEUR REFUSE, en pour cent. C'est la valeur de
// sa configuration inter-serveurs.
inline constexpr int kEmblemTransparencyLimit = 80;

// Fabrique le BMP 24 bits rangé de bas en haut qu'attend le serveur : 54
// octets d'en-tête et 24 rangées de 72 octets, déjà alignées sur quatre.
//
// ⚠ LA TAILLE DE L'EN-TÊTE EST CELLE DU FICHIER, au octet près : c'est le
// premier champ que le serveur compare.
[[nodiscard]] std::vector<std::uint8_t> EncodeEmblemBmp(const EmblemPixels& pixels);

// REPREND UN BMP 24 × 24, en 24 bits ou en 256 couleurs, en pixels d'éditeur.
//
// ⚠ LE MAGENTA RESTE DU MAGENTA : c'est la couleur « vide » de l'éditeur, et
// la rendre transparente ici la ferait disparaître du dessin qu'on retouche.
[[nodiscard]] core::Result<EmblemPixels> EmblemPixelsFromBmp(std::span<const std::uint8_t> bmp);

// LES PIXELS D'UNE IMAGE QUELCONQUE, posés au CENTRE d'un emblème vide.
//
// Une icône d'objet fait 24 × 24, la taille exacte d'un emblème : la copie est
// alors pixel pour pixel. Une image d'un autre format est centrée, et ce qui
// déborde est rogné. Un pixel transparent reste vide, il ne devient pas noir.
[[nodiscard]] EmblemPixels EmblemPixelsFromImage(const Image& image);

// DÉCODE UN EMBLÈME POUR L'AFFICHER : un BMP, dont le magenta devient
// transparent, ou un GIF, dont on prend la première image.
//
// ⚠ LE SERVICE WEB ACCEPTE LES DEUX, et les emblèmes déposés par le client
// d'origine peuvent être des GIF. L'éditeur, lui, n'en produit jamais.
[[nodiscard]] core::Result<Image> DecodeEmblemImage(std::span<const std::uint8_t> data);

// LE CHEMIN HISTORIQUE : l'image que le serveur pousse par le fil est un BMP
// COMPRESSÉ en zlib. Rend le BMP, ou refuse un flux qui gonflerait au-delà de
// ce qu'un emblème peut peser.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> InflateEmblem(
    std::span<const std::uint8_t> compressed);

}  // namespace ro::formats
