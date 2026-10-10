#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA LISTE DES CHARRETTES — « Cart List » —, la fenêtre du client où l'on
// choisit l'apparence de sa charrette.
//
// 🔴 CE N'EST PAS UNE LISTE À LIRE : c'est une rangée de BOUTONS-IMAGES, cinq
// par ligne, chacun montrant la charrette qu'il donne. Ni nom, ni niveau requis,
// ni case cochée : le client ne dit rien d'autre que l'image, et un clic suffit.
//
// ⚠ SES BOUTONS SE CHEVAUCHENT, et c'est le dessin du client : cent pixels de
// côté posés tous les soixante-dix, avec un décalage propre à chaque type pour
// que la charrette tombe au milieu de sa case. Là où deux se recouvrent, c'est
// celui dont le CENTRE est le plus proche qui prend le clic : un clic sur une
// charrette doit la choisir, elle.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la compétence, ou la liste
// du serveur, qui l'ouvre. Un choix la referme, comme la croix.

inline constexpr const char* kCartChoiceWindowId = "cart_list";

// ── LA GÉOMÉTRIE DU CLIENT, en pixels d'art, depuis le coin de la fenêtre ───
inline constexpr float kCartChoiceWindowWidth = 385.0F;
// La hauteur d'une rangée de la FENÊTRE : elle grandit d'autant par ligne de
// cinq boutons, barre de titre comprise.
inline constexpr float kCartChoiceWindowRowHeight = 98.0F;
inline constexpr std::size_t kCartChoiceColumns = 5;
inline constexpr float kCartChoiceColumnStep = 70.0F;
// La hauteur d'une rangée de BOUTONS : ⚠ ELLE N'EST PAS CELLE DE LA FENÊTRE.
inline constexpr float kCartChoiceButtonRowStep = 82.0F;
inline constexpr float kCartChoiceButtonSide = 100.0F;

// Le coin haut-gauche du bouton de rang `rank`, qui donne le type `cart_type`.
[[nodiscard]] ImVec2 CartChoiceButtonOrigin(std::size_t rank, std::uint8_t cart_type) noexcept;

// La hauteur de la fenêtre pour `count` boutons. ⚠ UNE LISTE VIDE GARDE UNE
// RANGÉE : le client ne la redimensionne pas.
[[nodiscard]] float CartChoiceWindowHeight(std::size_t count) noexcept;

// LE BOUTON SOUS UN POINT de la fenêtre, en pixels d'art : le centre le plus
// proche l'emporte là où deux se recouvrent. Vide hors de tout bouton.
[[nodiscard]] std::optional<std::size_t> CartChoiceButtonAt(std::span<const std::uint8_t> types,
                                                            ImVec2 point) noexcept;

// CE QUE LA FENÊTRE MONTRE, recomposé par l'appelant à chaque image.
struct CartChoiceView {
  // Le titre de la barre. ⚠ CE N'EST PAS UN TEXTE À NOUS : il sort de la table
  // de messages du client, déjà dans la langue du joueur.
  std::string_view title;
  std::span<const std::uint8_t> types;
  // Une image par type, dans le même ordre. ⚠ UNE IMAGE ABSENTE laisse le
  // bouton cliquable et vide : le type existe, c'est son dessin qui manque.
  std::span<const SkinImage> images;
};

struct CartChoiceWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la liste du monde.
  bool open = false;
  // Le front montant : la fenêtre passe au PREMIER PLAN à l'ouverture.
  bool was_open = false;
  // LE BOUTON ENFONCÉ, jusqu'au relâchement : le choix ne part que si l'on
  // relâche sur le MÊME bouton, comme un bouton du client.
  std::optional<std::size_t> pressed;
};

struct CartChoiceWindowAction {
  // Le type choisi. Il ferme la fenêtre.
  std::optional<std::uint8_t> chosen;
  // La croix : fermée sans choix, sans un octet au serveur.
  bool closed = false;
};

[[nodiscard]] CartChoiceWindowAction DrawCartChoiceWindow(CartChoiceWindowState& state,
                                                          const CartChoiceView& view,
                                                          const SkinArt& art);

}  // namespace ro::ui
