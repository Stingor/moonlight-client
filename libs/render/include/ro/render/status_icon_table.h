#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ro::render {

// LA SECONDE SOURCE D'IMAGES D'ÉTAT : celle que le CLIENT porte dans son code.
//
// 🔴 LE CLIENT EN A DEUX, ET NE LIRE QUE LA PREMIÈRE FAIT CONCLURE À TORT. Sa
// résolution appelle d'abord la fonction Lua `GetEFSTImgFileName` — un simple
// `StateIconImgList[priorité][efst]` que l'installation fournit — puis, si elle
// ne rend rien, retombe sur un `switch` écrit dans son propre code.
//
// ⚠ C'EST LE MÊME PIÈGE QUE LES DEUX RÉSOLVEURS D'EFFETS, payé une seconde fois :
// la table Lua nomme 533 icônes, ce qui suffit à croire qu'on a tout vu. Les 87
// que ce switch ajoute ne sont nulle part ailleurs, et sans elles la barre
// d'états montre des lettres là où le client montre une image.
//
// Le chemin est RELATIF au dossier des textures — « effect\\STEELBODY.tga » —,
// comme le client l'écrit. Vide quand le client ne nomme pas cet état.
//
// 🔴 LE MÉTIER EN FAIT PARTIE, et ce n'est pas une commodité : six états — Aspersio,
// Enchant weapon et les quatre armes élémentaires — ont une image À EUX pour la
// lignée Taekwon, et l'image de tout le monde ailleurs. Le client tranche par
// la classe de l'acteur ; sans ce paramètre, une de ces deux
// réponses serait forcément fausse pour quelqu'un.
[[nodiscard]] std::string_view BuiltInStatusIconPath(std::uint32_t effect_id, std::uint32_t job_id);

// Combien la table en porte. ⚠ POUR LE RELEVÉ, et pour lui seul : c'est le
// chiffre qu'un essai compare, et qu'un générateur cassé ferait tomber à zéro
// sans que rien d'autre ne le dise.
[[nodiscard]] std::size_t BuiltInStatusIconCount();

}  // namespace ro::render
