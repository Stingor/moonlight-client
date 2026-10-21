#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/render/companion_falcon.h"

// LA CHARRETTE D'UN MARCHAND : un sprite-enfant qui naît derrière tout acteur
// portant l'état « pousse une charrette », et qu'il TRAÎNE au bout d'une longe
// de cinq unités.
//
// 🔴 TOUT SE COMPTE DANS LE REPÈRE DU CLIENT, comme pour le faucon : l'ordonnée
// descend et la profondeur est l'opposée de la nôtre. C'est l'appelant qui
// convertit à l'entrée et à la sortie.
namespace ro::render {

// ── LA RECETTE : quel sprite pour quel type ────────────────────────────────

// Les deux fichiers d'une charrette, sous `data\sprite\`. ⚠ DEUX CHEMINS ET NON
// UN : la charrette d'un bébé prend souvent l'image de celle de l'adulte et ses
// propres mouvements, plus bas et plus près.
struct CartArt {
  std::string_view spr_path;
  std::string_view act_path;
};

// Le plus grand type que la table du client connaisse. Au-delà, elle lirait
// hors de ses bornes : ici, rien ne se dessine.
inline constexpr std::int32_t kMostCartType = 13;

// LA CHARRETTE D'UN ACTEUR, d'après son type et le métier qu'il MONTRE.
//
// ⚠ UN SUPER NOVICE N'A QU'UNE CHARRETTE, quel que soit le type que le serveur
// annonce : la sienne, et celle du bébé super novice pour un bébé. Tout autre
// bébé a la version réduite de la charrette de son type. Un costume d'option
// rend le corps d'un adulte ordinaire, et la charrette avec.
//
// Vide pour un type nul, négatif ou inconnu de la table.
[[nodiscard]] std::optional<CartArt> CartArtFor(std::int32_t cart_type, std::uint32_t job_id,
                                                std::uint32_t option) noexcept;

// VRAI QUAND L'ACTEUR TRAÎNE SA CHARRETTE : un type annoncé, et l'acteur se
// voit. Caché, camouflé ou invisible, il la perd, et la retrouve en
// reparaissant — une charrette NEUVE, qui renaît derrière lui.
[[nodiscard]] bool CarriesCart(std::int32_t cart_type, std::uint32_t option) noexcept;

// ── LA MÉCANIQUE ──────────────────────────────────────────────────────────

// De combien la charrette naît DERRIÈRE son maître, le long de son cap.
inline constexpr float kCartBirthBehindUnits = 5.0F;
// LA LONGE : au-delà de cette distance, la charrette est ramenée à elle, dans
// l'axe qui la sépare de son maître. En deçà, elle ne bouge pas.
inline constexpr float kCartLeashUnits = 5.0F;
// L'IMAGE SUIT LA DISTANCE PARCOURUE, et non le temps : `parcouru × 0,23 × 4`,
// divisé par la cadence déclarée de l'action zéro. Une charrette arrêtée ne
// tourne pas ses roues.
inline constexpr float kCartFramesPerUnit = 0.23F;
inline constexpr float kCartFrameStride = 4.0F;
// Le plancher de la cadence : une cadence déclarée sous un tic vaut un tic.
inline constexpr float kCartMinFrameDelay = 1.0F;
// L'opacité de la charrette quand le joueur rend les invocations translucides :
// c'est le même réglage, et la même valeur, que pour le faucon.
inline constexpr std::uint8_t kCartTranslucentAlpha = 50;
inline constexpr std::uint8_t kCartOpaqueAlpha = 255;

// Une charrette, telle que le jeu la fait vivre tic après tic.
struct Cart {
  FalconPoint position;
  // LA POSITION AU TIC PRÉCÉDENT : l'image se peint entre elle et `position`.
  // Voir `tick_blend.h`.
  FalconPoint previous_position;
  // LE CAP, en degrés du client : vers le maître, recalculé à chaque tic.
  float heading_degrees = 0.0F;
  // Le cap que l'action MONTRE : celui du tic PRÉCÉDENT, comme le faucon.
  float shown_heading_degrees = 0.0F;
  // LA DISTANCE PARCOURUE depuis la naissance, en unités du monde. C'est elle
  // qui choisit l'image.
  float travelled_units = 0.0F;
  std::size_t frame = 0;
};

// Le nombre d'images d'une action du `.act` ; zéro pour une action absente.
using CartFrameCount = std::function<std::size_t(std::size_t action)>;

// La naissance : sur le sol de son maître, cinq unités derrière lui.
[[nodiscard]] Cart BornCart(const FalconMaster& master) noexcept;

// LA CADENCE qui divise la distance parcourue : celle que le `.act` déclare pour
// son action zéro, jamais sous un tic.
[[nodiscard]] float CartFrameDelay(const formats::ActAnimation& animation) noexcept;

// Un tic de seize millisecondes, vu d'une caméra tournée de
// `camera_yaw_degrees`.
void StepCart(Cart& cart, const FalconMaster& master, const FalconGround& ground,
              const CartFrameCount& frame_count, float frame_delay, float camera_yaw_degrees);

// L'action que la charrette montre : la direction d'écran de son cap précédent.
[[nodiscard]] std::size_t CartAction(const Cart& cart, float camera_yaw_degrees) noexcept;

// Où la charrette se peint, au poids du tic courant : voir `TickBlendWeight`.
// ⚠ SEULE LA POSITION GLISSE : l'action reste celle du tic, une direction
// d'écran parmi huit n'ayant pas d'entre-deux.
[[nodiscard]] FalconPoint CartDrawnPosition(const Cart& cart, float weight) noexcept;

// ── LE BOUTON DE LA LISTE DES CHARRETTES ───────────────────────────────────

// LA CHARRETTE QU'UN BOUTON DE LA LISTE MONTRE : toujours celle de l'adulte,
// quel que soit le métier de qui choisit. Vide pour un type que la table ne
// connaît pas — le type zéro compris, qui n'a pas de dessin.
[[nodiscard]] std::optional<CartArt> CartListArtFor(std::int32_t cart_type) noexcept;

// Le côté d'un bouton de la liste, en pixels.
inline constexpr std::int32_t kCartButtonSidePixels = 100;

// Le coin haut-gauche d'un calque, en pixels, depuis l'origine de son image
// d'animation.
struct ButtonLayerCorner {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

// LE COIN D'UN CALQUE, tel que le jeu le range en lisant le .act : dès la 2.0,
// l'écart moins la moitié ARRONDIE VERS LE HAUT de la taille, ramené à
// l'échelle un. La taille est celle que le calque déclare (2.5), sinon celle de
// son image ; avant la 2.0, l'écart reste tel quel. Vide pour une échelle nulle.
[[nodiscard]] std::optional<ButtonLayerCorner> CartButtonLayerCorner(
    const formats::ActLayer& layer, std::int32_t act_version, std::int32_t image_width,
    std::int32_t image_height) noexcept;

// L'IMAGE D'UN BOUTON DE LA LISTE : la première image de l'action zéro, son
// premier calque CENTRÉ dans le carré, les suivants posés par leur COIN, à la
// différence entre leur coin et celui du premier. Coupé au bord du carré.
//
// 🔴 PAR LE COIN, ET NON PAR LE CENTRE : c'est ce qui colle les roues sous la
// caisse. Poser chaque calque à l'écart de son centre, depuis le coin du
// premier, décollerait une roue étroite d'une caisse large de la moitié de leur
// différence de largeur.
//
// ⚠ CE N'EST PAS LA COMPOSITION D'UN ACTEUR : ni teinte, ni échelle, ni
// rotation, ni ancre ; seul le miroir est rendu.
[[nodiscard]] formats::Image CartButtonImage(const formats::SprSprite& sprite,
                                             const formats::ActAnimation& animation);

}  // namespace ro::render
