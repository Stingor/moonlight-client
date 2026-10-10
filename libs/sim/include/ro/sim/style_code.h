#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/style.h"

namespace ro::sim {

// UN STYLE ÉCRIT EN CLAIR : ce qu'un joueur copie, colle, range et partage.
//
// La forme est `<version>:<corps>:<teinte>:<cheveux>:<coiffure>:` suivie de
// quatre-vingts chiffres hexadécimaux — cinq octets par rampe, huit rampes.
//
// 🔴 C'EST EXACTEMENT CELLE DU SERVEUR ET DE SES AUTRES CLIENTS, à l'octet
// près. Ce n'est pas une convergence heureuse : les joueurs de ce serveur
// échangent leurs codes d'un client à l'autre, et une base de données en garde. Un
// format « équivalent » rendrait la moitié des codes illisibles chez l'autre
// moitié des joueurs, sans que rien ne dise pourquoi.
//
// 🔴 LA VERSION SE COMPARE, ELLE NE SE MIGRE PAS. Un code d'une autre version
// est REFUSÉ — voir `kStyleWireVersion` : réinterpréter des rangs de rampes
// qui ne désignent plus les mêmes pièces repeindrait les bottes en couleur de
// cape, avec l'aplomb d'un format valide. Un cache se refait, une confiance
// non.
//
// ⚠ EN CLAIR, ET C'EST UN CHOIX : le code se lit dans une valeur SQL et dans
// un presse-papiers. Seul le corps est en hexadécimal sur huit chiffres, parce
// que c'est un condensé — pas un nombre qu'on lit — et que sa largeur fixe le
// rend repérable d'un coup d'œil.

// La longueur de la queue hexadécimale : cinq octets par rampe, deux chiffres
// par octet.
inline constexpr std::size_t kStyleCodeRampBytes = 5;
inline constexpr std::size_t kStyleCodeHexChars = kStyleRampCount * kStyleCodeRampBytes * 2;

// Ce qu'un code porte : une recette, et le corps sur lequel elle a été faite.
struct StyleCode {
  StyleRecipe recipe;
  // Zéro quand le code ne désigne aucun corps — ce qui est le cas de tous les
  // codes PARTAGÉS et de tous les préréglages.
  //
  // ⚠ UN CODE SE COLLE SUR LE PERSONNAGE QU'ON VEUT : y graver un corps le
  // rendrait inapplicable ailleurs, alors qu'une recette se ré-applique par
  // RANG de rampe. Le résultat reste plausible — les pièces sont triées par
  // surface — sans être identique, et c'est le compromis assumé partout.
  std::uint32_t body_key = 0;
};

[[nodiscard]] std::string EncodeStyleCode(const StyleRecipe& recipe, std::uint32_t body_key = 0);

// Lit un code, ou rien s'il est illisible ou d'une autre version.
//
// ⚠ LES ESPACES AUTOUR SONT TOLÉRÉS : un code collé depuis un chat ou un forum
// en traîne presque toujours, et refuser pour cela serait gratuitement pénible.
[[nodiscard]] std::optional<StyleCode> DecodeStyleCode(std::string_view text);

// ── LA BALISE QUI PORTE UN STYLE DANS UNE LIGNE DE CONVERSATION ─────────────
//
// `<STYL>étiquette:code</STYL>`, la balise que les autres clients du serveur
// emploient, à l'octet près — et elle vit ICI, contre le code qu'elle transporte, plutôt que dans
// le chat.
//
// 🔴 C'EST UN CONTRAT ENTRE DEUX CLIENTS, et le laisser loin de l'encodage
// laisserait un jour l'un changer sans l'autre : la balise porte une chaîne
// dont la VERSION décide, et ce qui décide de la version est juste au-dessus.
//
// 🔴 CE QUI VOYAGE EST LE CODE EN ENTIER, et c'est ce qui distingue ce lien de
// tous les autres. Un objet, un monstre, un réglage EXISTENT chez le lecteur :
// il suffit de les désigner. Un style n'existe NULLE PART hors de son
// porteur — la recette d'un joueur en zone disparaît avec lui —, donc un lien
// qui ne transporterait qu'un pseudo mourrait au premier changement de carte.
//
// ⚠ LA LIGNE EST DONC LONGUE CHEZ QUI NE REND PAS LA BALISE, et l'étiquette
// vient en TÊTE pour que le début reste lisible.

// L'étiquette, nettoyée de ce qui couperait la balise en deux.
//
// 🔴 ON REMPLACE AU LIEU DE REFUSER. Depuis qu'une étiquette peut être un nom
// de préréglage, elle est TAPÉE PAR LE JOUEUR : « Rouge : v2 » est un nom
// parfaitement raisonnable, et refuser donnerait un geste qui ne produit rien
// et n'explique rien — la pire des réponses. Un pseudo n'en porte jamais.
[[nodiscard]] std::string StyleTagLabel(std::string_view label);

// La balise complète. Vide si le code l'est.
[[nodiscard]] std::string BuildStyleTag(std::string_view label, std::string_view code);

// Où commence la prochaine balise, ou `npos`.
[[nodiscard]] std::size_t FindStyleTag(std::string_view text, std::size_t from);

// Ce qu'une balise porte.
struct StyleTag {
  // L'étiquette, telle qu'elle a voyagé. Peut être vide.
  std::string label;
  // Le code, tel qu'il a voyagé — c'est LUI qu'on relaie, jamais une version
  // ré-encodée : un aller-retour perdrait ce qu'une version future ajouterait.
  std::string code;
  StyleCode read;
};

// CE QU'UNE BALISE A DONNÉ — trois issues, et les confondre coûterait.
enum class StyleTagRead : std::uint8_t {
  // Pas une balise, ou sa fermante manque : rien à sauter, et ce qui suit doit
  // rester à l'écran tel quel.
  kNotATag,
  // La balise est entière, mais son code est illisible ou d'une autre version.
  // 🔴 L'ÉTIQUETTE EST QUAND MÊME RENDUE : c'est tout ce qu'on peut encore
  // montrer, et elle dit de quoi on parle. Cent dix caractères d'hexadécimal
  // n'apprendraient rien à personne.
  kUnreadable,
  kOk,
};

// Lit la balise qui commence à `text[0]`, et dit combien d'octets elle occupe.
//
// 🔴 LE CODE EST VALIDÉ ICI, À LA LECTURE, ET PAS AU CLIC. Un lien qui se
// révèle illisible au moment où on l'ouvre est une promesse trahie ; refusé à
// l'analyse, il retombe en texte ordinaire, ce qui se comprend tout seul.
//
// ⚠ `after` VAUT CE QU'IL FAUT SAUTER DÈS QUE LA FERMANTE EST LÀ, même sur un
// code illisible : sans cela l'appelant repartirait en arrière sur la même
// balise. Zéro veut dire « rien à sauter ».
[[nodiscard]] StyleTagRead ParseStyleTag(std::string_view text, StyleTag* out, std::size_t* after);

}  // namespace ro::sim
