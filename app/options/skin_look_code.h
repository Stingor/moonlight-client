#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ro/ui/skin_look.h"

namespace moonclient {

// LE CODE D'UN THÈME, À PARTAGER : une ligne de texte qu'un joueur copie depuis
// sa page Skin et qu'un autre colle dans la sienne.
//
// `moonlook5:` — la version de l'habillage —, puis en hexadécimal : le thème de
// référence (deux chiffres), la luminosité des images et l'opacité (deux
// chiffres chacune, sur 255), et chaque couleur en RRGGBBAA. Les couleurs vont
// par version, puis par clé : un ordre qui ne bouge pas quand la page se range
// autrement, et où une couleur à venir s'ajoute à la fin.
//
// ⚠ L'ARRONDI N'EN FAIT PAS PARTIE : comme un thème posé, un code collé garde
// celui du joueur.
[[nodiscard]] std::string SkinLookCode(const ro::ui::SkinLook& look, int theme_rank);

// Ce qu'un code porte : l'habillage, l'arrondi à son défaut, et le thème de
// référence.
struct SharedSkinLook {
  ro::ui::SkinLook look;
  int theme_rank = 0;
};

// Le thème d'un code, ou rien quand la ligne n'en est pas un. Un code plus
// ancien n'a pas les couleurs venues depuis : elles prennent celles de son
// thème. Les blancs qui l'entourent ne comptent pas.
[[nodiscard]] std::optional<SharedSkinLook> ReadSkinLookCode(std::string_view code);

}  // namespace moonclient
