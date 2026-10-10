#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace moonclient {

// ── LE LIEN D'UNE RECETTE : `<CRAF>id:nom</CRAF>` ───────────────────────────
//
// Une balise À NOUS : le client d'origine ne la connaît pas,
// et la laisse à l'écran telle quelle.
//
// 🔴 CE N'EST PAS UN LIEN D'ITEM : il ne désigne pas l'item mais la FAÇON DE LE
// FAIRE. D'où un libellé qui l'annonce — « [Recette : Acid Bottle] » —, un
// aperçu qui montre le skill et les composants plutôt que les stats, et un clic
// qui ouvre l'Atlas des recettes plutôt que la description. C'est ce qu'on
// poste quand on explique une fabrication ; un lien d'item n'en dit rien.
//
// ⚠ LE NOM VOYAGE, comme dans la balise d'un monstre : un client qui ne la
// connaît pas voit la balise brute, et elle reste lisible. Champs séparés par
// ':', le nom EN DERNIER — il est libre d'en porter.

inline constexpr std::string_view kRecipeLinkOpen = "<CRAF>";
inline constexpr std::string_view kRecipeLinkClose = "</CRAF>";

struct RecipeLink {
  // Le PRODUIT de la recette.
  std::uint32_t item_id = 0;
  // En UTF-8.
  std::string name;
};

// La balise. VIDE sans identifiant ni nom, ou quand le nom porte la fermante :
// il couperait la balise en deux à la relecture.
[[nodiscard]] std::string BuildRecipeLink(const RecipeLink& link);

// Relit une balise qui COMMENCE à `text[0]`. Faux si ce n'en est pas une, sans
// identifiant ou sans nom.
//
// ⚠ `after` REÇOIT CE QUI SUIT LA FERMANTE, et la fin du texte quand elle
// manque : une ligne coupée par le serveur ne doit pas faire lire au-delà.
[[nodiscard]] bool ParseRecipeLink(std::string_view text, RecipeLink* out, std::size_t* after);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindRecipeLink(std::string_view text, std::size_t from);

// Ce que le lecteur voit : « [Recette : Acid Bottle] ».
//
// 🔴 COMPOSÉ CHEZ LE LECTEUR, dans SA langue, et le même que dans un dialogue
// de PNJ. ⚠ DES CROCHETS ET NON LES CHEVRONS D'UN ITEM : deux espèces de lien
// qui se ressembleraient promettraient le même clic. Ils font partie du
// morceau cliquable, comme ceux d'un style — ils FONT le libellé.
[[nodiscard]] std::string RecipeLinkLabel(std::string_view name);

}  // namespace moonclient
