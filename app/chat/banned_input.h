#pragma once

// LES MOTS INTERDITS, DU CÔTÉ DE CE QU'ON ÉCRIT : la liste de l'installation,
// relue, et le refus qu'une saisie reçoit quand elle en porte un.
//
// Ce qui ARRIVE se masque dans `net` à mesure que les trames se lisent, ou à
// l'affichage là où le client d'origine le fait : l'étiquette d'un acteur,
// une enseigne, le titre du salon, la conversation 1:1. Ce qui PART suit deux
// règles :
//
//   · une PAROLE — publique, groupe, guilde, chuchotement, clan, champ de
//     bataille — part MASQUÉE : le joueur n'est pas arrêté, ses mots si ;
//   · un NOM, un TITRE ou un TEXTE — personnage, salon, échoppe, groupe,
//     guilde et son avis, compagnon, courrier, réponse à un PNJ, Talkie Box —
//     ne part PAS : la saisie est refusée, avec le message que le client
//     d'origine montre pour CETTE saisie, et rien n'est émis.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/banned_words.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LA LISTE DE L'INSTALLATION. Vide sans `vfs` ou sans fichier : rien ne se
// masque alors, comme chez le client d'origine.
[[nodiscard]] ro::sim::BannedWords LoadBannedWords(ro::vfs::Vfs* vfs);

// LES SAISIES QUE LE CLIENT ÉPROUVE, rangées par le refus qu'elles reçoivent.
enum class BannedInput : std::uint8_t {
  // Le nom d'un personnage à sa création.
  kCharacterName,
  // Le titre d'un salon ou d'une échoppe, le nom d'un groupe à sa création,
  // l'avis de la guilde, la réponse écrite à un PNJ.
  kSentence,
  // Le nom d'une guilde à sa création.
  kGuildName,
  // L'objet ou le corps d'un courrier.
  kMail,
  // Le nouveau nom d'un homoncule ou d'un familier.
  kCompanionName,
  // Le texte d'une Talkie Box ou d'un Graffiti.
  kTalkbox,
};

// LE REFUS DE CETTE SAISIE, traduit — rien quand elle ne porte aucun mot
// interdit, ou que la liste est vide.
[[nodiscard]] std::optional<std::string> BannedInputRefusal(const ro::sim::BannedWords& banned,
                                                            std::string_view utf8_text,
                                                            BannedInput input);

// LES BALISES QU'UN NOM DE COMPAGNON NE PORTE PAS : celles d'un lien d'objet,
// cherchées telles quelles, casse comprise, n'importe où dans le nom. Un nom
// qui en contient une se lirait comme un lien partout où il s'affiche.
inline constexpr std::string_view kItemLinkOpeningTag = "<ITEML>";
inline constexpr std::string_view kItemLinkClosingTag = "</ITEML>";

// LE REFUS DU NOUVEAU NOM D'UN HOMONCULE OU D'UN FAMILIER, traduit, une fois
// la borne de longueur passée : une balise de lien d'objet d'abord, puis un
// mot interdit. Rien quand le nom passe ; sans liste, seules les balises
// s'éprouvent.
[[nodiscard]] std::optional<std::string> CompanionNameRefusal(const ro::sim::BannedWords* banned,
                                                              std::string_view utf8_name);

// LA COULEUR DE CE REFUS quand il s'écrit dans la conversation, en 0xRRGGBB.
[[nodiscard]] std::uint32_t BannedInputColor(BannedInput input);

}  // namespace moonclient
