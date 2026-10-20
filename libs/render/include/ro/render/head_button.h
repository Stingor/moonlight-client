#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/formats/image.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {

// LE BOUTON DE STATUT À LA TÊTE DU PERSONNAGE : sa tête seule, de face, posée
// dans le même bouton que les icônes de menu.
//
// ⚠ AUCUNE DÉPENDANCE À OPENGL : les deux images se fabriquent en mémoire, donc
// le cadrage se vérifie par des nombres. Ce qui les téléverse vit à côté, dans
// `status_head_icon.h`.
//
// 🔴 TOUTES LES MESURES SONT EN PIXELS DE L'IMAGE, À DOUBLE DENSITÉ : un pixel
// d'interface y vaut `kHeadButtonDensity` pixels de côté, comme nos icônes de
// menu livrées en PNG. L'image prend donc la place exacte du bitmap du jeu, et
// la tête reste nette quand l'échelle d'interface grandit.

// ── LE BOUTON, en pixels d'interface ────────────────────────────────────────
inline constexpr std::int32_t kHeadButtonDensity = 2;
// La face : un carré aux coins coupés, cerné d'un liseré clair d'un pixel.
inline constexpr std::int32_t kHeadButtonFaceSide = 30;
inline constexpr std::int32_t kHeadButtonCornerCut = 2;
inline constexpr std::int32_t kHeadButtonRim = 1;
// Le filet sombre, sous le liseré, en haut et à gauche seulement.
inline constexpr std::int32_t kHeadButtonFilet = 1;
// L'ombre portée : la face répétée, décalée vers la droite et vers le bas.
inline constexpr std::int32_t kHeadButtonShadowRight = 2;
inline constexpr std::int32_t kHeadButtonShadowDown = 3;
// ENFONCÉ, TOUT DESCEND D'UN PIXEL : face, ombre et tête.
inline constexpr std::int32_t kHeadButtonPressDrop = 1;

// La taille de l'image, en pixels de l'image : la face et son ombre, plus la
// descente de l'enfoncé, réservée dans les deux états pour qu'ils aient la
// même taille.
inline constexpr std::int32_t kHeadButtonWidthPx =
    (kHeadButtonFaceSide + kHeadButtonShadowRight) * kHeadButtonDensity;
inline constexpr std::int32_t kHeadButtonHeightPx =
    (kHeadButtonFaceSide + kHeadButtonShadowDown + kHeadButtonPressDrop) * kHeadButtonDensity;

// LES COULEURS DU BOUTON, celles des icônes de menu du jeu.
struct ButtonColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};
inline constexpr ButtonColor kHeadButtonRimColor{.red = 155, .green = 177, .blue = 241};
inline constexpr ButtonColor kHeadButtonShadowColor{.red = 20, .green = 20, .blue = 20};
inline constexpr ButtonColor kHeadButtonFillColor{.red = 42, .green = 57, .blue = 79};
inline constexpr ButtonColor kHeadButtonFiletColor{.red = 15, .green = 20, .blue = 28};
// Enfoncé, le fond s'éclaircit, et son filet avec lui.
inline constexpr ButtonColor kHeadButtonPressedFillColor{.red = 60, .green = 88, .blue = 131};
inline constexpr ButtonColor kHeadButtonPressedFiletColor{.red = 21, .green = 31, .blue = 46};

// UN RECTANGLE AUX COINS COUPÉS À QUARANTE-CINQ DEGRÉS, en pixels de l'image,
// bornes comprises.
struct CutCornerBox {
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;
  // Combien de pixels chaque coin perd le long de chaque bord.
  std::int32_t cut = 0;

  // Le pixel est dedans quand il tient dans le rectangle ET qu'à chaque coin,
  // la somme de ses distances aux deux bords atteint la coupe.
  [[nodiscard]] bool Holds(std::int32_t x, std::int32_t y) const noexcept;
};

// LE FOND DE LA FACE : ce que ni le liseré ni le filet ne couvrent.
[[nodiscard]] CutCornerBox HeadButtonFillArea(bool pressed) noexcept;

// 🔴 OÙ LA TÊTE A LE DROIT DE TOMBER : le fond, rogné d'un pixel de l'image sur
// tout son tour. Aucun pixel de la tête ne touche donc le liseré ni le filet,
// pas même en diagonale, et la marge ne dépasse pas un demi-pixel d'interface.
[[nodiscard]] CutCornerBox HeadButtonPortraitArea(bool pressed) noexcept;

// Le bouton seul, sans tête, à double densité.
[[nodiscard]] formats::Image DrawHeadButtonFrame(bool pressed);

// LES DEUX ÉTATS DU BOUTON, ou rien.
struct HeadButtonImages {
  formats::Image normal;
  formats::Image pressed;

  [[nodiscard]] bool empty() const noexcept { return normal.empty() || pressed.empty(); }
};

// POSE UNE TÊTE DANS LE BOUTON, dans ses deux états.
//
// La tête est recadrée sur ses pixels visibles, agrandie ou réduite sans
// déformer pour REMPLIR `HeadButtonPortraitArea`, et centrée. Chaque pixel de
// l'image prend la moyenne des pixels de sprite qu'il recouvre, pondérée par
// la surface recouverte : les grands aplats restent nets, et seul le bord
// d'un pixel de sprite se partage entre deux pixels de l'image.
//
// Vide quand la tête n'a aucun pixel visible : l'appelant garde alors l'icône
// qu'il montrait.
[[nodiscard]] HeadButtonImages ComposeHeadButton(const formats::Image& head);

// LA TÊTE SEULE DE CETTE APPARENCE, en pixels de sprite, recadrée au plus près.
//
// 🔴 RIEN QUE LA TÊTE : la coupe de `look.hair`, dans la teinte de
// `look.hair_palette`, du sexe et de la race du métier — un Doram a sa propre
// tête. Ni coiffe, ni corps, ni rien de ce que le personnage tient : voir
// `ActorWardrobe::HeadOnly`.
//
// ⚠ DE FACE, À L'ARRÊT, PREMIÈRE IMAGE : celle qui regarde droit devant.
//
// Vide quand l'installation n'a pas ce sprite.
[[nodiscard]] formats::Image HeadPortraitOf(ActorWardrobe& wardrobe, const ActorLook& look);

// ── LE STATUT DU PERSONNAGE EN COURS ────────────────────────────────────────

// CE QUI DÉCIDE DE LA TÊTE MONTRÉE, champ par champ.
//
// ⚠ COMPARÉE CHAMP PAR CHAMP, jamais par une empreinte : deux têtes confondues
// montreraient une coiffure pour une autre.
//
// ⚠ LE PERSONNAGE EN FAIT PARTIE : en changer refait l'image, même quand le
// suivant porte la même tête.
struct StatusHeadKey {
  std::uint32_t character_id = 0;
  std::uint32_t job = 0;
  // 0 = femme, 1 = homme, comme le serveur.
  std::uint8_t sex = 0;
  std::uint16_t hair = 0;
  std::int16_t hair_palette = 0;

  [[nodiscard]] bool operator==(const StatusHeadKey&) const = default;
};

// L'APPARENCE QUE CETTE CLÉ DÉSIGNE : un joueur, sa tête, et rien d'autre.
[[nodiscard]] ActorLook HeadLookOf(const StatusHeadKey& key);

// LES IMAGES DU BOUTON DE STATUT, refaites seulement quand la tête change.
//
// 🔴 LE BOUTON SE DESSINE À CHAQUE IMAGE, LA TÊTE PRESQUE JAMAIS : la recomposer
// à chaque image ouvrirait un sprite soixante fois par seconde pour dessiner
// la même chose. Elle se refait quand la clé change — coiffeur, teinture,
// changement de personnage —, une fois.
class StatusHeadArt {
 public:
  // La garde-robe est passée par référence et doit vivre plus longtemps.
  explicit StatusHeadArt(ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}

  // Recompose quand la clé a changé. Rend vrai quand les images ont changé —
  // et qu'il faut donc les téléverser de nouveau.
  //
  // ⚠ SANS CLÉ, IL N'Y A RIEN À MONTRER : aucun personnage n'est chargé. Les
  // images se vident, et le bouton garde l'icône qu'il avait sans tête.
  //
  // ⚠ UN ÉCHEC NE SE RETENTE PAS À CHAQUE IMAGE : la clé est retenue avec lui,
  // et rien ne se recompose avant qu'elle change.
  bool Refresh(const std::optional<StatusHeadKey>& key);

  // Les images en cours ; vides quand il n'y a rien à montrer.
  [[nodiscard]] const HeadButtonImages& images() const noexcept { return images_; }

  // Combien de fois une tête a été composée : ce qui prouve qu'une clé égale ne
  // recompose rien.
  [[nodiscard]] std::size_t composed_count() const noexcept { return composed_count_; }

 private:
  ActorWardrobe* wardrobe_;
  std::optional<StatusHeadKey> drawn_for_;
  HeadButtonImages images_;
  std::size_t composed_count_ = 0;
};

}  // namespace ro::render
