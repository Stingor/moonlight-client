#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ro::ui {

// UN SPRITE DE DÉCOR DU DOCUMENT, tel que le panneau le montre et le rend :
// une créature seulement peinte, voir `ro::mapedit::MapSprite`.
struct MapEditorSprite {
  // Son nom dans la table des classes, ou le numéro de sa classe.
  std::string sprite;
  // Ses pieds, dans le repère du RSW.
  std::array<float, 3> position{};
  // Le rang d'animation du .act.
  std::uint32_t action = 0;
  // Figée sur `held_frame`, ou en boucle à `frame_speed` fois la cadence.
  bool frozen = false;
  std::uint32_t held_frame = 0;
  float frame_speed = 1.0F;
  // La direction du .act, de 0 (de face) à 7, et la rotation, en degrés par
  // seconde.
  std::uint32_t direction = 0;
  float turn_degrees_per_second = 0.0F;
  // Le roulis, en degrés, et sa rotation, en degrés par seconde.
  float roll_degrees = 0.0F;
  float roll_degrees_per_second = 0.0F;
  float scale = 1.0F;
  // De 0 (invisible) à 1 (plein).
  float opacity = 1.0F;
  bool hidden = false;
  std::string why;
  // CE QUE SON .act PORTE, posé par l'appelant : ses rangs d'animation, et les
  // images de l'action choisie ; zéro quand le sprite ne se lit pas.
  std::size_t action_count = 0;
  std::size_t frame_count = 0;
};

namespace map_editor {
// Ajouter le sprite `sprite` — un nom ou un numéro de classe —, là où
// l'appelant pose ce qu'on ajoute.
struct AddSprite {
  std::string sprite;
};
// Régler le sprite de rang `slot`, ou plus aucun.
struct SelectSprite {
  std::optional<std::size_t> slot;
};
// Le sprite de rang `slot` devient `sprite`. `released` : le geste est fini, et
// il devient une entrée de l'historique.
struct SetSprite {
  std::size_t slot = 0;
  MapEditorSprite sprite;
  bool released = true;
};
struct RemoveSprite {
  std::size_t slot = 0;
};
// Déplacer le sprite de rang `slot` en `native`, dans le repère du RSW — à la
// flèche, comme une pose. `released`, comme pour `SetSprite`.
struct MoveSprite {
  std::size_t slot = 0;
  std::array<float, 3> native{};
  bool released = true;
};
}  // namespace map_editor

}  // namespace ro::ui
