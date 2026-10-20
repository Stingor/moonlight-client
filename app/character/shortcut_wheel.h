#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>

#include <glm/glm.hpp>

#include "ro/ui/shortcut_bar.h"

#include "input/hotkeys.h"

namespace moonclient {

// CE QUI DÉCIDE SI LE GESTE PEUT OUVRIR LA ROUE.
struct WheelGate {
  // La souris est sur une fenêtre : le clic milieu y appartient à l'interface
  // — il y vide une case de barre —, et la roue ne s'ouvre que sur le monde.
  bool mouse_over_interface = false;
  // La fenêtre des raccourcis attend un geste : il est pour elle.
  bool capturing_hotkey = false;
};

// LA ROUE D'UTILISATION, VUE DE LA PARTIE : ouverte ou non, où, et la case
// choisie au relâcher.
//
// ⚠ ELLE NE LANCE RIEN ELLE-MÊME. La case choisie repart par l'intention d'un
// clic sur la case de la barre (`ShortcutBarsAction::cast`), donc par le même
// chemin : un skill à cible ou au sol arme la visée ordinaire.
//
// 🔴 TANT QU'ELLE EST OUVERTE, LE MONDE NE REÇOIT NI CLIC, NI GLISSÉ, NI
// MOLETTE : la souris sert à viser une case, et la même souris ne doit pas
// tourner la caméra ou faire marcher le personnage en même temps.
class ShortcutWheel {
 public:
  // Retient la barre choisie dans les options et rend la vue à dessiner. À
  // chaque image, avant le dessin. ⚠ LA ROUE SE REFERME SANS RIEN UTILISER si
  // la barre vient d'être retirée.
  [[nodiscard]] ro::ui::ShortcutWheelView Compose(const ro::ui::ShortcutBarsLayout& layout);

  // UN ÉVÉNEMENT, AVANT LE MONDE. Rend vrai s'il est pris : il ne doit alors
  // aller nulle part ailleurs. Ouvre la roue au bouton du milieu, la referme
  // au relâcher de ce qui l'a ouverte, l'annule sur Échap ou à la perte du
  // focus, et avale la souris tant qu'elle est ouverte — `mouse`, la position
  // du curseur du jeu, suit tout de même.
  //
  // `held` : les modificateurs tenus, qu'un événement de souris ne porte pas.
  bool TakeEvent(const SDL_Event& event, const Hotkeys& bindings, WheelGate gate, SDL_Keymod held,
                 glm::vec2* mouse);

  // UNE TOUCHE QUI VA AU JEU. Rend vrai quand elle ouvre la roue, centrée sur
  // le curseur `mouse`. ⚠ JAMAIS SUR UNE RÉPÉTITION : la touche tenue est le
  // geste lui-même, et son auto-répétition ne doit rien rouvrir.
  bool TakeKeyDown(const SDL_KeyboardEvent& key, const Hotkeys& bindings, WheelGate gate,
                   glm::vec2 mouse);

  [[nodiscard]] bool open() const noexcept { return open_; }

  // LA DEMANDE DES BARRES, augmentée de la case choisie au relâcher. ⚠ UN CLIC
  // SUR UNE BARRE DANS LA MÊME IMAGE GARDE LA MAIN. La case choisie ne sert
  // qu'une fois.
  [[nodiscard]] ro::ui::ShortcutBarsAction WithChosen(const ro::ui::ShortcutBarsAction& asked);

 private:
  enum class Opener : std::uint8_t { kMiddleButton, kKey };

  void Open(glm::vec2 at, Opener opener, std::uint32_t key);
  // Referme la roue ; `use` retient la case visée, s'il y en a une.
  void Close(bool use);

  std::optional<std::size_t> bar_;
  bool open_ = false;
  Opener opener_ = Opener::kMiddleButton;
  // La touche qui l'a ouverte, quand c'en est une.
  std::uint32_t opener_key_ = 0;
  glm::vec2 center_{0.0F, 0.0F};
  glm::vec2 pointer_{0.0F, 0.0F};
  // L'index global de la case choisie, en attente de la demande des barres.
  std::optional<std::size_t> chosen_;
};

}  // namespace moonclient
