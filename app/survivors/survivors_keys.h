#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ro/sim/survivors_geometry.h"

namespace moonclient {

// LES TOUCHES DU MODE SURVIVORS, sans SDL : la bibliothèque ne connaît que
// des scancodes, et l'exécutable lui passe ceux de ses événements.
//
// Un scancode est une place PHYSIQUE sur le clavier : W A S D se lisent Z Q S
// D sur un clavier AZERTY, et c'est voulu (U10). Les valeurs sont les codes
// d'usage du clavier USB HID, que `SDL_Scancode` reprend tels quels.
inline constexpr std::uint32_t kScancodeA = 4;
inline constexpr std::uint32_t kScancodeD = 7;
inline constexpr std::uint32_t kScancodeS = 22;
inline constexpr std::uint32_t kScancodeW = 26;
inline constexpr std::uint32_t kScancodeRight = 79;
inline constexpr std::uint32_t kScancodeLeft = 80;
inline constexpr std::uint32_t kScancodeDown = 81;
inline constexpr std::uint32_t kScancodeUp = 82;

// LES TOUCHES DE MARCHE TENUES, et le cap qu'elles donnent. Une touche
// opposée à une autre tenue annule son axe ; rien de tenu, ou deux paires
// opposées, donnent kNoHeading.
class MoveKeys {
 public:
  // Vrai quand le scancode est une touche de marche : l'événement est pris.
  bool Take(std::uint32_t scancode, bool down) noexcept;
  // La fenêtre a perdu le focus : aucune touche ne reste tenue, car son
  // relâchement n'arrivera pas.
  void ReleaseAll() noexcept { held_ = 0; }
  // Le cap sur 64 valeurs (0 au nord, sens trigonométrique), ou kNoHeading.
  [[nodiscard]] std::uint8_t Heading() const noexcept;

 private:
  std::uint8_t held_ = 0;  // un bit par scancode de marche
};

// Le scancode est une touche de marche.
[[nodiscard]] bool IsMoveKey(std::uint32_t scancode) noexcept;

// LE BOUTON GAUCHE TENU : marcher vers le point visé au sol, en sous-unités
// de la simulation.
struct MouseAim {
  bool held = false;
  ro::sim::FieldPoint hero;
  ro::sim::FieldPoint target;
};

// Le cap de l'image : la souris prime tant qu'elle est tenue (kNoHeading si
// elle vise le héros lui-même), sinon les touches.
[[nodiscard]] std::uint8_t HeadingOf(const MoveKeys& keys, const MouseAim& mouse) noexcept;

// LES LETTRES À AFFICHER dans l'aide des contrôles : celles que le clavier du
// joueur porte aux places de W A S D. Le nommeur est injecté, parce que SDL
// reste dans l'exécutable.
using ScancodeNamer = std::function<std::string(std::uint32_t scancode)>;
struct MoveKeyLabels {
  std::string north;
  std::string west;
  std::string south;
  std::string east;
};
[[nodiscard]] MoveKeyLabels LabelsOf(const ScancodeNamer& name_of);

// LA GARDE CONTRE LE CLIC EN TROP (U11) : après l'ouverture d'un choix, d'un
// coffre, d'une pause ou d'un écran de fin, aucune réponse n'est prise avant
// kInputGuardMs. Un clic parti pour marcher ne choisit pas une offre à la
// place du joueur.
inline constexpr std::uint64_t kInputGuardMs = 400;

class InputGuard {
 public:
  void Arm(std::uint64_t now_ms) noexcept { armed_at_ms_ = now_ms; }
  // Vrai dès kInputGuardMs écoulées depuis Arm (bornes comprises).
  [[nodiscard]] bool Accepts(std::uint64_t now_ms) const noexcept {
    return now_ms >= armed_at_ms_ + kInputGuardMs;
  }

 private:
  std::uint64_t armed_at_ms_ = 0;
};

}  // namespace moonclient
