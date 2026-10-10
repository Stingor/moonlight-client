#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace moonclient {

// « /noeud » sert à REGARDER une troupe sans attendre que le jeu la joue, comme
// « /effet » sert à regarder un effet en fichier.
//
// Le sprite qu'une ligne tapée demande de semer, ou rien. Un nom seul suffit :
// il se cherche dans le dossier des sprites d'effets, extensions sous-entendues.
[[nodiscard]] std::optional<std::string> EffectNodeCommand(std::string_view line);

}  // namespace moonclient
