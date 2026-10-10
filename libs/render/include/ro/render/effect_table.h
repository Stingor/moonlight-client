#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ro::render {

// Le dossier où le client range ses effets animés. Les chemins de la table lui
// sont relatifs.
inline constexpr std::string_view kEffectFolder = "data\\texture\\effect\\";

// Une entrée de la table des effets du client.
struct EffectEntry {
  std::uint16_t id = 0;
  // Le chemin, relatif à kEffectFolder. Il porte un « %d » quand `variants`
  // n'est pas nul.
  std::string_view path;
  // Combien de fichiers différents cet effet possède, ou ZÉRO pour un chemin
  // fixe. Le client en tire un au sort à chaque lancement : Pneuma et Meteor
  // Storm en ont trois chacun, et jouer toujours le premier se verrait.
  std::uint8_t variants = 0;
};

// La table entière, triée par identifiant.
//
// C'est celle à laquelle le client s'adresse quand il fait naître un effet PAR
// SON GESTIONNAIRE — un effet que le serveur annonce, une émanation d'objet.
[[nodiscard]] std::span<const EffectEntry> EffectTable();

// La SECONDE table du client, triée de même.
//
// ⚠ IL Y EN A DEUX, et ce n'est pas une redite. Le client résout un identifiant
// d'effet par deux fonctions distinctes, selon le chemin qui le fait naître :
// son gestionnaire d'effets passe par la première, la naissance d'un effet
// primitif par son numéro passe par celle-ci — et c'est par celle-ci que
// passent les effets de COMPÉTENCE. La
// première ne nomme que trois identifiants sous deux cents, celle-ci en nomme
// quatre-vingts : ne lire que la première, c'est conclure à tort que les
// compétences classiques n'ont que des effets procéduraux.
//
// Là où les deux nomment le même identifiant, elles le nomment PAREIL, et un
// essai le vérifie.
[[nodiscard]] std::span<const EffectEntry> EffectPrimitiveTable();

// Le fichier .str d'un identifiant d'effet, relatif à kEffectFolder, ou une
// chaîne VIDE pour un identifiant qu'AUCUNE des deux tables ne nomme — il est
// alors procédural, inemployé, ou résolu AILLEURS.
//
// ⚠ CES DEUX TABLES S'ARRÊTENT À 2442. Au-delà, le client ne compare plus
// rien : il appelle un global Lua et laisse une table de DONNÉES de
// l'installation répondre. Aucun relevé du
// switch ne pouvait rendre cette branche, faute de cas à relever. Voir
// `ro/render/hat_effect_table.h` et `kHatEffectIdBase`.
//
// Les deux sont consultées, la première d'abord. Nous ne cherchons pas à
// reproduire quel chemin du client fait naître quel effet : nous cherchons à
// DESSINER, et un identifiant nommé d'un côté ou de l'autre se dessine pareil.
//
// `draw` choisit parmi les variantes d'un effet qui en a plusieurs ; il est
// pris modulo leur nombre. Le passer explicitement plutôt que de tirer ici rend
// une scène rejouable à l'identique.
[[nodiscard]] std::string EffectFile(std::uint32_t effect_id, std::uint32_t draw = 0);

}  // namespace ro::render
