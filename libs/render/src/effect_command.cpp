#include "ro/render/effect_command.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <system_error>

#include "ro/render/effect_table.h"

namespace ro::render {
namespace {

constexpr std::string_view kEffectPrefix = "/effet";
constexpr std::string_view kEffectExtension = ".str";
constexpr std::string_view kBlanks = " \t";

// Vrai si le nom porte déjà son extension, quelle que soit la casse.
bool EndsWithExtension(std::string_view name) {
  if (name.size() < kEffectExtension.size()) {
    return false;
  }
  const std::string_view tail = name.substr(name.size() - kEffectExtension.size());
  for (std::size_t index = 0; index < kEffectExtension.size(); ++index) {
    const char letter = tail[index] >= 'A' && tail[index] <= 'Z'
                            ? static_cast<char>(tail[index] - 'A' + 'a')
                            : tail[index];
    if (letter != kEffectExtension[index]) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<std::string> EffectCommand(std::string_view line, std::uint32_t draw) {
  if (!line.starts_with(kEffectPrefix)) {
    return std::nullopt;
  }
  std::string_view rest = line.substr(kEffectPrefix.size());
  // Une blanche est EXIGÉE après la commande : sans elle, tout mot plus long
  // commençant par les mêmes lettres nous reviendrait. C'est ce qui protège
  // aussi de la forme à signe des commandes du jeu, où « /str+ » se lirait
  // sinon comme « /str ».
  const std::size_t first = rest.find_first_not_of(kBlanks);
  if (first == 0 || first == std::string_view::npos) {
    return std::nullopt;
  }
  rest = rest.substr(first, rest.find_last_not_of(kBlanks) + 1 - first);

  // Tout en chiffres : un IDENTIFIANT, que la table du client traduit. Le
  // tirage sert aux effets à plusieurs fichiers — Pneuma en a trois.
  if (!rest.empty() &&
      std::ranges::all_of(rest, [](char letter) { return letter >= '0' && letter <= '9'; })) {
    std::uint32_t identifier = 0;
    const auto [unused, error] =
        std::from_chars(rest.data(), rest.data() + rest.size(), identifier);
    if (error != std::errc{}) {
      return std::nullopt;
    }
    const std::string file = EffectFile(identifier, draw);
    return file.empty() ? std::string() : std::string(kEffectFolder) + file;
  }

  std::string path;
  // Un nom sans dossier est cherché parmi les effets : c'est le cas ordinaire,
  // et taper le chemin entier à la main ne prouverait rien de plus.
  if (rest.find_first_of("\\/") == std::string_view::npos) {
    path.append(kEffectFolder);
  }
  path.append(rest);
  if (!EndsWithExtension(path)) {
    path.append(kEffectExtension);
  }
  return path;
}

}  // namespace ro::render
