#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/model_pose.h"

namespace ro::ui {

// UN MODÈLE AU PRESSE-PAPIERS, pour le recopier dans une autre carte : son
// fichier, relatif à data\model\ et en UTF-8, et ce qui ne dépend pas de la
// carte — rotation, échelle, opacité, façon de jouer et vitesse, comme un .rsw
// les écrit. La position, elle, se prend là où l'on colle.
struct ClipboardModel {
  std::string model_file;
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  std::optional<float> opacity;
  std::int32_t animation_type = render::kModelPlayLooped;
  float animation_speed = 1.0F;

  friend bool operator==(const ClipboardModel&, const ClipboardModel&) = default;
};

// Une ligne de texte, lisible et collable ailleurs :
// « moonclient-modèle|fichier|rx ry rz|sx sy sz|opacité|jeu vitesse » —
// l'opacité vide pour un modèle plein. Une ligne sans le dernier champ joue en
// boucle à vitesse 1. Le fichier ne porte jamais « | ».
[[nodiscard]] std::string ModelClipboardText(const ClipboardModel& model);

// Le modèle que porte `text`, ou rien : une autre forme, un nombre illisible,
// un fichier vide, une opacité hors de [0, 1] ou une vitesse hors de ]0, 100].
[[nodiscard]] std::optional<ClipboardModel> ParseModelClipboard(std::string_view text);

}  // namespace ro::ui
