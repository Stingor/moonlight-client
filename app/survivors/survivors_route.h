#pragma once

#include <cstdint>

#include "survivors/survivors_keys.h"

namespace moonclient {

// LA ROUTE DES ÉVÉNEMENTS PENDANT UNE PARTIE (conception §4.3), sans SDL :
// l'exécutable traduit chaque événement SDL en `SurvivorsEvent`, et la route
// dit s'il est PRIS — il ne parvient alors pas au jeu.
//
// 🔴 TANT QUE LA PARTIE TIENT L'ÉCRAN, LE CLAVIER, LA SOURIS ET LA MOLETTE SONT
// PRIS, tous : une touche qui fuirait vers le jeu déclencherait une barre de
// raccourcis (Z, Q, 1 à 4 sur AZERTY) ou ferait marcher le personnage resté en
// ville. L'interface les a déjà vus, avant la route : ses fenêtres répondent.
//
// La fenêtre, la sortie et le redimensionnement passent. La perte de focus est
// NOTÉE (pause U6), puis passe aussi : le jeu la suit comme d'habitude.

// Les scancodes que la route lit, codes d'usage du clavier USB HID comme ceux
// de `survivors_keys.h`.
inline constexpr std::uint32_t kScancodeEscape = 41;
// La rangée des chiffres, par sa place : sur AZERTY, « 1 » sans Maj est « & ».
inline constexpr std::uint32_t kScancodeDigit1 = 30;
inline constexpr std::uint32_t kScancodeDigit4 = 33;
// Entrée, Espace et l'Entrée du pavé numérique valident le coffre.
inline constexpr std::uint32_t kScancodeReturn = 40;
inline constexpr std::uint32_t kScancodeSpace = 44;
inline constexpr std::uint32_t kScancodeKeypadEnter = 88;
// Ctrl+Tab montre ou masque la minimap, comme en partie.
inline constexpr std::uint32_t kScancodeTab = 43;

// Les boutons gauche et droit, par leur numéro SDL.
inline constexpr std::uint8_t kLeftMouseButton = 1;
inline constexpr std::uint8_t kRightMouseButton = 3;

enum class SurvivorsEventKind : std::uint8_t {
  kKeyDown,
  kKeyUp,
  kMouseDown,
  kMouseUp,
  kMouseMove,
  kWheel,
  // Le texte qu'une touche saisit : pris avec elle.
  kText,
  kFocusLost,
  // Fenêtre, sortie, redimensionnement, et tout le reste : jamais pris.
  kOther,
};

struct SurvivorsEvent {
  SurvivorsEventKind kind = SurvivorsEventKind::kOther;
  std::uint32_t scancode = 0;
  // Une touche tenue que le système répète : la pause ne bascule pas deux fois.
  bool repeat = false;
  // Ctrl tenu avec la touche.
  bool ctrl = false;
  std::uint8_t button = 0;
  // La souris, en points de la fenêtre, et ce qu'elle a parcouru vers la
  // droite depuis le mouvement précédent.
  float x = 0.0F;
  float y = 0.0F;
  float dx_px = 0.0F;
  // L'appui complète un double clic, à la règle du jeu (`DoubleClickClocks`),
  // que l'exécutable tient.
  bool second_of_double = false;
  // Les crans de molette, positifs vers l'avant.
  float wheel_notches = 0.0F;
  // La souris au-dessus d'une fenêtre de l'interface : la molette y fait
  // défiler la fenêtre, et n'est pas un geste de la partie.
  bool over_interface = false;
};

// CE QUE LES ÉVÉNEMENTS D'UNE IMAGE ONT DEMANDÉ, consommé par l'image.
struct SurvivorsGestures {
  bool escape = false;
  bool focus_lost = false;
  float wheel_notches = 0.0F;
  // La carte dite par la rangée des chiffres, 1 à 4 ; 0 sans touche.
  int pressed_card_key = 0;
  // Entrée ou Espace : le bouton « Continuer » du coffre.
  bool confirm = false;
  // Ctrl+Tab : la minimap se montre ou se masque.
  bool toggle_minimap = false;
  // LE CLIC DROIT GLISSÉ, en points vers la droite : la caméra tourne.
  float orbit_px = 0.0F;
  // LE DOUBLE CLIC DROIT, à la règle du jeu : la caméra revient face au nord.
  bool reset_yaw = false;
};

[[nodiscard]] bool IsConfirmKey(std::uint32_t scancode) noexcept;

// LES TOUCHES QUE LA PARTIE LIT : la marche, Échap et les chiffres des
// cartes. Aucune liaison du jeu ne les lui reprend ; une autre touche peut
// garder une action du jeu qui ne touche pas au personnage (l'enregistreur).
[[nodiscard]] bool SurvivorsReadsKey(std::uint32_t scancode) noexcept;

// À QUI VA UNE FRAPPE PENDANT LA PARTIE, maintenant que la conversation et la
// voix restent par-dessus l'arène.
enum class SurvivorsKeyTurn : std::uint8_t {
  // À la route de l'arène, comme avant.
  kArena,
  // Un champ de texte écrit : la frappe est à lui, et le héros lâche les
  // touches qu'il tenait — leur relâchement ira au champ.
  kTyping,
  // La touche « Parler (voix) » : la voix la lit sur l'état du clavier, et
  // l'arène ne la voit pas.
  kVoice,
  // Entrée hors du coffre : la conversation s'ouvre.
  kOpenChat,
  // Échap sur la barre que le battle mode a dépliée : elle se replie, et la
  // partie ne se met pas en pause.
  kFoldChat,
};

// Ce que l'interface et l'écran disent au moment de la frappe.
struct SurvivorsKeyFacts {
  // Un champ de texte a le clavier : toute frappe est à lui.
  bool typing = false;
  // La barre que le battle mode a dépliée réclame les frappes sans avoir le
  // clavier : tout lui va, sauf Entrée et Échap.
  bool chat_claims = false;
  // Le coffre est à l'écran : Entrée y vaut « Continuer ».
  bool chest_shown = false;
  // La barre de conversation se replie sur Échap.
  bool chat_folds_on_escape = false;
  // La frappe est celle de la touche « Parler (voix) ».
  bool talk_key = false;
};

[[nodiscard]] SurvivorsKeyTurn SurvivorsKeyTurnOf(const SurvivorsEvent& event,
                                                  const SurvivorsKeyFacts& facts) noexcept;

class SurvivorsRoute {
 public:
  // Vrai quand l'événement est pris.
  [[nodiscard]] bool Take(const SurvivorsEvent& event);
  // Les gestes de l'image, remis à zéro.
  [[nodiscard]] SurvivorsGestures TakeGestures() noexcept;
  // Une partie neuve, ou le retour au jeu : rien n'est tenu.
  void Reset() noexcept;
  // Le clavier part à un champ de texte : les touches et le bouton tenus se
  // lâchent, sans toucher aux gestes de l'image.
  void ReleaseHeld() noexcept;

  [[nodiscard]] const MoveKeys& keys() const noexcept { return keys_; }
  [[nodiscard]] bool mouse_held() const noexcept { return mouse_held_; }
  [[nodiscard]] float mouse_x() const noexcept { return mouse_x_; }
  [[nodiscard]] float mouse_y() const noexcept { return mouse_y_; }

 private:
  MoveKeys keys_;
  SurvivorsGestures gestures_;
  bool mouse_held_ = false;
  float mouse_x_ = 0.0F;
  float mouse_y_ = 0.0F;
  // Le bouton droit tenu hors des fenêtres : ses mouvements tournent la caméra.
  bool orbiting_ = false;
};

}  // namespace moonclient
