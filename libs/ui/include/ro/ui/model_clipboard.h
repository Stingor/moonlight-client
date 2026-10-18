#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace ro::ui {

// UN MODÈLE AU PRESSE-PAPIERS, pour le recopier dans une autre carte : son
// fichier, relatif à data\model\ et en UTF-8, et ce qui ne dépend pas de la
// carte — rotation, échelle, opacité. La position, elle, se prend là où l'on
// colle.
struct ClipboardModel {
  std::string model_file;
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  std::optional<float> opacity;

  friend bool operator==(const ClipboardModel&, const ClipboardModel&) = default;
};

// Une ligne de texte, lisible et collable ailleurs :
// « moonclient-modèle|fichier|rx ry rz|sx sy sz|opacité » — l'opacité vide
// pour un modèle plein. Le fichier ne porte jamais « | ».
[[nodiscard]] std::string ModelClipboardText(const ClipboardModel& model);

// Le modèle que porte `text`, ou rien : une autre forme, un nombre illisible,
// un fichier vide ou une opacité hors de [0, 1].
[[nodiscard]] std::optional<ClipboardModel> ParseModelClipboard(std::string_view text);

}  // namespace ro::ui
