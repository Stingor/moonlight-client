#pragma once

#include <cstdint>
#include <optional>

#include <SDL3/SDL_events.h>

#include "ro/ui/character_window.h"

#include "character/shortcut_view.h"
#include "input/hotkeys.h"

namespace moonclient {

// Combien de temps la touche doit être tenue avant de poser la compétence.
//
// ⚠ UN MAINTIEN ET NON UN APPUI : un appui bref sur F1 au-dessus du grimoire
// est souvent un réflexe de combat, et il ne doit pas réécrire la barre.
inline constexpr std::uint32_t kGrimoireKeyHoldMs = 500;

// LE MODE CLAVIER DU GRIMOIRE : la souris sur une compétence, une touche tenue,
// et la compétence se pose dans la case de barre liée à cette touche.
//
// 🔴 LA TOUCHE EST PRISE DÈS L'APPUI, avant qu'elle n'agisse. Sinon F1 lancerait
// le sort de sa case au moment même où l'on veut la remplacer — et ses
// répétitions le relanceraient tant qu'elle est tenue.
//
// ⚠ SEULES LES TOUCHES DE BARRE ET LES TOUCHES LIBRES SONT PRISES. Une touche
// liée à une autre action — ouvrir le sac, le menu — garde son rôle : le mode
// clavier ne doit pas rendre le reste du jeu sourd.
class GrimoireKeys {
 public:
  // Une touche enfoncée, avant son action. Rend vrai quand elle est prise :
  // elle n'agit alors pas, ni ses répétitions.
  [[nodiscard]] bool TakeKeyDown(const SDL_KeyboardEvent& key,
                                 const ro::ui::CharacterWindowState& sheet, const Hotkeys& bindings,
                                 std::uint32_t now_ms);

  // À chaque image : une fois la touche tenue assez longtemps, pose la
  // compétence, ou dit que la touche n'a pas de case.
  void Advance(ro::ui::CharacterWindowState& sheet, ShortcutBarView& bar, const Hotkeys& bindings,
               std::uint32_t now_ms);

  // La compétence dont la barre éclaire les cases : celle qu'on survole, mode
  // clavier allumé. Zéro sinon.
  [[nodiscard]] static std::uint16_t Highlighted(const ro::ui::CharacterWindowState& sheet);

 private:
  struct Held {
    Binding binding;
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    std::uint16_t skill_id = 0;
    std::uint16_t level = 0;
    std::uint32_t since_ms = 0;
    // Posée, ou abandonnée : la touche reste prise jusqu'au relâchement.
    bool settled = false;
  };
  std::optional<Held> held_;
};

}  // namespace moonclient
