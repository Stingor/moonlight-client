#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ro::ui {

// ── LES ONGLETS DE CALQUE ────────────────────────────────────────────────────
//
// 🔴 UN ONGLET PAR CALQUE PORTÉ, ET SEULEMENT PORTÉ : un chapeau qu'on n'a pas
// sur la tête n'a rien à montrer, et son onglet serait une case vide. L'onglet
// d'un objet porte le nom de l'objet, pas celui de l'emplacement : c'est ce
// chapeau-là qu'on teint, et la teinture lui appartient.
//
// ⚠ LES NUMÉROS VOYAGENT : le styliste ouvre l'éditeur sur un onglet par son
// numéro, voir `ro/sim/style.h`. On en ajoute en queue, jamais au milieu.
//
// 🔴 LE 1 N'EST PLUS UN ONGLET : la peau du visage se règle sous la Tête, celle
// du corps sous le Corps. Un styliste qui le demande ouvre la Tête, voir
// `PaletteTabOfNumber`.
enum class PaletteTabKind : std::uint8_t {
  kBody = 0,
  // LA TÊTE : la coupe, la couleur et les pièces des cheveux, et la peau du
  // visage.
  kHair = 2,
  kHeadgearTop = 3,
  kHeadgearMiddle = 4,
  kHeadgearLow = 5,
  kGarment = 6,
  // LES COMPAGNONS SORTIS : l'homoncule et le familier, chacun sous son nom. La
  // teinture appartient au compagnon, et vaut pour sa forme d'aujourd'hui.
  kHomunculus = 7,
  kPet = 8,
  // LES EFFETS DE CHAPEAU AFFICHÉS : un réglage par effet, sans pièces. Voir
  // `ro/ui/palette_effects.h`.
  kEffects = 9,
  // L'OISEAU ET LE CHARIOT QUE LE JOUEUR PORTE : la teinture appartient au
  // personnage, une par fichier d'image.
  kFalcon = 10,
  kCart = 11,
};
inline constexpr std::size_t kPaletteTabKinds = 12;

// L'ONGLET QU'UN NUMÉRO DU STYLISTE DÉSIGNE : un de plus que `PaletteTabKind`,
// zéro pour aucun. Le numéro de l'ancien onglet de la peau ouvre la Tête.
[[nodiscard]] std::optional<PaletteTabKind> PaletteTabOfNumber(std::uint8_t number) noexcept;

// Le nom d'un onglet, déjà traduit, quand ce n'est pas un objet qui le nomme.
[[nodiscard]] const char* PaletteTabName(PaletteTabKind kind);

// POURQUOI UN CALQUE NE SE RÈGLE PAS, dit dans son onglet grisé.
enum class PaletteTabBlock : std::uint8_t {
  kNone,
  // Le profil n'y trouve aucune pièce assez grande.
  kNoPiece,
  // L'habit du GM garde les couleurs de ses sprites.
  kGmOutfit,
  // La tête d'orc est un visage d'effet, sans palette de cheveux.
  kOrcHead,
  // Sur cette monture, la peau peint aussi l'animal.
  kSkinOnMount,
  // L'apparence affichée ne correspond à aucun objet de l'équipement.
  kNotFound,
  // Le serveur ne range pas la peau du corps : le corps suit le visage.
  kBodySkinNotServed,
  // Le serveur ne connaît pas le calque des compagnons (règle 25 sous 7).
  kCompanionNotServed,
  // Le serveur ne connaît pas les calques de l'oiseau et du chariot.
  kAccessoryNotServed,
  // L'oiseau est rentré, le chariot n'est pas poussé.
  kAccessoryAbsent,
};

struct PaletteTab {
  PaletteTabKind kind = PaletteTabKind::kBody;
  // Le nom de l'objet porté, prêt à l'affichage ; vide pour le corps, la peau
  // et les cheveux, que la fenêtre nomme elle-même.
  std::string name;
  PaletteTabBlock blocked = PaletteTabBlock::kNone;
  // La part du calque qu'un réglage atteint, en pour-cent, ou moins un.
  int coverage_percent = -1;
  // Une retouche attend d'être validée.
  bool edited = false;
  // Le calque porte une teinture, validée.
  bool dyed = false;
  // UN OBJET : la teinture appartient à l'exemplaire équipé.
  bool item = false;
  // UN COMPAGNON : la teinture lui appartient, pour sa forme d'aujourd'hui. Vrai
  // aussi pour l'oiseau et le chariot, qui se règlent de même.
  bool companion = false;
  // L'OISEAU OU LE CHARIOT : la teinture appartient au personnage.
  bool accessory = false;
  // Le compagnon est surtout gris : seul le réglage absolu le colore.
  bool mostly_grey = false;
  // L'OBJET POSE UN EFFET DE CHAPEAU affiché : son onglet mène à l'onglet des
  // effets.
  bool poses_effect = false;
};

}  // namespace ro::ui
