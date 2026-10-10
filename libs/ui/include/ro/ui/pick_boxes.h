#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace ro::ui {

// LES ZONES CLIQUABLES, telles que l'établi du staff les montre.
//
// Les rectangles d'ÉCRAN dont la souris se sert pour savoir sur QUOI elle
// pointe — pas les positions au sol. Déjà projetés, en pixels de fenêtre : `ui`
// ne connaît ni la caméra ni le monde, il dessine ce qu'on lui donne. C'est
// `app` qui les calcule, par la règle même qui désigne.

// LA FAMILLE D'UNE ZONE, par le numéro que le client lui donne.
enum class PickCategory : std::uint8_t {
  kActor = 0,
  kGroundItem = 1,
  kSkillUnit = 2,
  kPet = 3,
  kCompanion = 4,
};

// Une zone cliquable.
struct PickBoxView {
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};
  // L'identifiant de ce qui la dépose, et la classe qu'elle affiche.
  std::uint32_t id = 0;
  std::uint32_t job = 0;
  PickCategory category = PickCategory::kActor;
  // La zone du personnage joué.
  bool own = false;
};

// Tout ce que l'établi dessine à cette image.
struct PickBoxesView {
  std::vector<PickBoxView> boxes;
  // Le rang de la zone qu'un clic prendrait maintenant, s'il y en a une.
  std::optional<std::size_t> hovered;
  glm::vec2 mouse{0.0F, 0.0F};
};

// La couleur du contour d'une zone : jaune pour celle qu'un clic prendrait,
// sinon celle de sa famille — vert pour un acteur, bleu pour un objet au sol,
// orange pour une unité de compétence, gris pour le reste.
[[nodiscard]] std::array<std::uint8_t, 4> PickBoxColor(const PickBoxesView& view, std::size_t rank);

// L'étiquette de la zone désignée : « AID … job … cat … ».
[[nodiscard]] std::string PickBoxTag(const PickBoxView& box);

// Le compte près du curseur : combien de zones, et si l'une est sous lui.
[[nodiscard]] std::string PickBoxCount(const PickBoxesView& view);

// Dessine les zones au PREMIER PLAN, par-dessus les fenêtres : le panneau qu'on
// garde ouvert pour régler ne doit pas masquer la zone qu'on observe.
void DrawPickBoxes(const PickBoxesView& view);

}  // namespace ro::ui
