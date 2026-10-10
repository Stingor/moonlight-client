#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace ro::ui {

// LES ZONES DE SON D'AMBIANCE, telles que l'établi du staff les montre.
//
// Des tracés DÉJÀ PROJETÉS à l'écran, en pixels de fenêtre : `ui` ne connaît
// ni la carte ni la caméra, il dessine ce qu'on lui donne. C'est `app` qui
// passe chaque source du repère du client au nôtre, la pose sur le terrain et
// la projette.
struct SoundZoneView {
  // L'emprise de la source, un contour fermé. Vide quand un coin passe
  // derrière l'œil.
  std::vector<glm::vec2> footprint;
  // Le cercle d'où l'on entend le bruit, autour du point d'où il part.
  std::vector<glm::vec2> hearing;
  // Le point d'où le bruit part à cet instant, et le joueur qui l'entend : un
  // trait les relie.
  std::optional<glm::vec2> emitter;
  std::optional<glm::vec2> listener;
  // Où poser l'étiquette, et ce qu'elle dit : le fichier, la portée, le cycle.
  std::optional<glm::vec2> label_at;
  std::string label;
  // Vrai quand la source vient de sonner : son tracé s'allume.
  bool just_played = false;
};

// Dessine les zones sur le fond, par-dessus le monde et sous les fenêtres.
void DrawSoundZones(std::span<const SoundZoneView> zones);

}  // namespace ro::ui
