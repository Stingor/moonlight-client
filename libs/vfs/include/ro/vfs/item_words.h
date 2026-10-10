#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/achievement_words.h"
#include "ro/vfs/chat_room_words.h"
#include "ro/vfs/trade_words.h"
#include "ro/vfs/vfs.h"
#include "ro/vfs/whisper_words.h"

namespace ro::vfs {

// LES MOTS DONT LE NOM D'UN OBJET EST FAIT.
//
// Le fil ne porte pas de nom d'objet : il porte un identifiant, un raffinement,
// un grade et quatre cartes. « +7 [A] Double Lucky Sword [2] » se COMPOSE, et
// chacun de ses mots vient d'un fichier de l'installation — les mots de cartes —
// ou de l'interface — les ornements, posés par `FillMessageWords`. Voir les
// ADR 0035 et 0090.

// Quatre cartes sur le fil, et le client n'en lit jamais une cinquième.
inline constexpr std::size_t kItemNameCardSlots = 4;

// Les cinq éléments d'une arme forgée : neutre, glace, terre, feu, vent — dans
// l'ordre où l'octet bas de `card[1]` les numérote.
inline constexpr std::size_t kItemElementCount = 5;

// Le nombre d'étoiles d'une forge tient sur un quartet ; seuls 5, 10 et 15
// donnent un mot.
inline constexpr std::size_t kItemForgeStarsMax = 15;

// Les deux anneaux de mariage remplacent le possessif par un mot à eux.
inline constexpr std::uint32_t kWeddingRingItemIdFirst = 2634;
inline constexpr std::uint32_t kWeddingRingItemIdSecond = 2635;

// Le mot d'une carte, et de quel côté du nom il se pose.
struct CardWord {
  std::string text;

  // 🔴 C'EST UNE LISTE D'IDENTIFIANTS QUI LE DIT, PAS UNE TABLE DE MOTS.
  // `cardpostfixnametable.txt` n'apporte aucun texte — son second champ est
  // toujours vide. Il NOMME les cartes dont le mot déménage de l'avant du nom
  // vers l'arrière, et le chargeur du client fait littéralement ce déplacement.
  // S'en tenir à la lecture naïve du fichier mettrait 509 mots du mauvais côté.
  bool after_name = false;
};

// CE QUE LE CLIENT ÉCRIT AU SUJET D'UN RAMASSAGE — en rouge quand il échoue,
// en jaune quand il réussit.
//
// ⚠ CE NE SONT PAS DES MOTS DE NOM, et ils sont pourtant ici : ils sortent du
// MÊME fichier, lu une seule fois. Les ranger ailleurs demanderait une seconde
// lecture de la même table pour six chaînes.
//
// ⚠ AUCUN CODE DE REFUS N'EST NOMMÉ ICI : le nom dit le SENS du message, pas le
// numéro qui le déclenche. Le numéro appartient au protocole, et c'est la
// couche qui le décode qui fait la correspondance.
//
// ⚠ UN CHAMP VIDE EST UNE CLÉ QUE L'INSTALLATION NE PORTE PAS : la table de
// messages est la seule source, aucun libellé de repli ne prend sa place, et
// l'appelant n'écrit pas de ligne vide.
struct ItemPickupWords {
  // Le refus qui ne dit pas pourquoi.
  std::string cannot_get;

  // Le sac est trop lourd.
  std::string too_heavy;

  // La limite de poids serait dépassée. ⚠ LE CLIENT RANGE CE TEXTE SOUS UNE
  // CLÉ QUI PARLE DE NOMBRE : on suit son texte, pas le nom de sa clé.
  std::string over_weight_limit;

  // 30 000 d'un même objet, pas plus.
  std::string one_kind_limit;

  // Le plafond du même objet est atteint.
  std::string same_item_limit;

  // « Vous avez obtenu %s (%d). », la ligne JAUNE du succès, que le
  // client écrit EN PLUS du bandeau. Se compose par `GotItemText`.
  std::string got_item;

  // « Vous avez jeté %s (%d). », la ligne d'un objet jeté au sol. Se compose
  // par `DroppedItemText`.
  std::string dropped_item;

  // Ce qu'un journal ajoute au nom d'un objet cassé : le nom seul ne le dit pas,
  // et c'est pourtant la première chose qu'on veut savoir d'un objet reçu.
  std::string damaged_mark;
};

// CE QUE LE CLIENT ÉCRIT D'UN COUP DANS LA CONVERSATION. Se composent par
// `BattleLineText` (`battle_words.h`).
struct BattleWords {
  // Le nom de qui n'en a pas.
  std::string someone;
  // Ce que je reçois, ce que reçoit mon groupe, ce que
  // je porte, ce que porte mon groupe.
  std::string i_received;
  std::string party_received;
  std::string i_gave;
  std::string party_gave;
};

// Les dix races, les dix éléments et les trois tailles d'un monstre.
inline constexpr std::size_t kMonsterRaceCount = 10;
inline constexpr std::size_t kMonsterElementCount = 10;
inline constexpr std::size_t kMonsterSizeCount = 3;

// CE QUE LA FICHE D'UN MONSTRE ÉCRIT :
// les valeurs de race, d'élément et de taille, et les libellés des lignes. Se
// lisent par les accesseurs de `monster_words.h`, qui rendent vide une clé absente.
struct MonsterWords {
  // Indexé par la race du serveur.
  std::array<std::string, kMonsterRaceCount> race;
  // Indexé par l'élément du serveur — l'ordre
  // des constantes `ELE_*`, que les résistances suivent aussi.
  std::array<std::string, kMonsterElementCount> element;
  // Indexé par la taille du serveur.
  std::array<std::string, kMonsterSizeCount> size;
  // Les libellés des lignes : niveau, race, élément, taille, DEF, MDEF.
  std::string level_label;
  std::string race_label;
  std::string element_label;
  std::string size_label;
  std::string def_label;
  std::string mdef_label;
};

// CE QU'ÉCRIT LA FENÊTRE DES DESTINATIONS d'une compétence de déplacement. Se
// lisent par les accesseurs de `warp_list_words.h`, qui rendent vide une clé absente.
struct WarpListWords {
  // La ligne au-dessus de la liste.
  std::string title;
  // La ligne d'un départ au hasard.
  std::string random_position;
  // La dernière ligne, que le client ajoute lui-même.
  std::string cancel;
  // Ce qu'il écrit pour une carte que sa table ne connaît pas.
  std::string nowhere;
};

// Combien de phrases la mémorisation d'un point de Warp connaît : voir
// `WarpMemoMessage`, dont c'est le compte.
inline constexpr std::size_t kWarpMemoMessageCount = 8;

// CE QU'ÉCRIT LA MÉMORISATION D'UN POINT DE WARP, une phrase par sens. Se lisent
// par `WarpMemoText`, qui rend vide une clé absente.
struct WarpMemoWords {
  std::array<std::string, kWarpMemoMessageCount> texts;
};

// CE QU'ÉCRIT LA FENÊTRE DE SAISIE DE TALKIE BOX. Se lisent par les accesseurs
// de `talkbox_words.h`, qui rendent vide une clé absente.
struct TalkboxWords {
  // Le titre.
  std::string title;
  // La ligne qui demande le message.
  std::string prompt;
};

// CE QU'ÉCRIT LA BOUTIQUE DE VOTE. Se lisent par les accesseurs de
// `cash_shop_words.h`, qui rendent vide une clé absente.
//
// ⚠ CE SERVEUR A REBAPTISÉ TOUTE LA FAMILLE DANS SA TABLE : la monnaie s'y
// appelle « Vote » et la boutique « Vote Shop ». Les lire ici, c'est suivre
// d'office une prochaine retouche de la table.
struct CashShopWords {
  // Le nom de la boutique, titre de sa fenêtre.
  std::string shop_name;
  // La monnaie principale.
  std::string currency;
  // La seconde
  // monnaie, et la case qui la fait dépenser d'abord.
  std::string event_points;
  std::string use_event_points;
  // LES REFUS D'ACHAT, dans l'ordre de `CashShopRefusal`.
  std::string deal_failed;
  std::string some_failed;
  std::string rune_overcount;
  std::string each_item_overcount;
  std::string unknown_error;
  std::string busy;
  std::string billing_expired;
  std::string billing_error;
};

// CE QU'ÉCRIVENT LES ATELIERS — fabrication et affinage d'arme. Se lisent par
// les accesseurs de `craft_words.h`, qui rendent vide une clé absente.
struct CraftWords {
  // La liste de fabrication est vide.
  std::string cant_make;
  // Le titre de la liste de forge et de pharmacie.
  std::string make_list;
  // « : matériaux requis », après le nom.
  std::string required_for;
  // Réussite et échec d'une fabrication — un `%s`.
  std::string make_succeeded;
  std::string make_failed;
  // Les quatre réponses d'un affinage, dans l'ordre du serveur — un `%s`
  // chacune. ⚠ LA RÉUSSITE ET L'ÉCHEC PORTENT LE MÊME TEXTE.
  std::string refine_succeeded;
  std::string refine_failed;
  std::string refine_skill_too_low;
  std::string refine_missing_ore;
};

struct ItemWords {
  // Le mot de multiplicité, indexé par le nombre de cartes IDENTIQUES. Les
  // rangs 0 et 1 restent vides : une carte seule ne se multiplie pas.
  std::array<std::string, kItemNameCardSlots + 1> repeated;

  // Le possessif qui relie le nom du créateur à l'objet — « 's Fire » —, indexé
  // par l'élément. Ce n'est pas un ornement : c'est lui qui porte l'apostrophe.
  std::array<std::string, kItemElementCount> possessive;

  // Le possessif des anneaux de mariage, qui n'ont pas d'élément mais un mot.
  std::string wedding_possessive;

  // La force de forge, indexée par le nombre d'étoiles. Hors de 5, 10 et 15 le
  // client n'écrit RIEN — pas même un espace.
  std::array<std::string, kItemForgeStarsMax + 1> forge_strength;

  // Ce qui s'affiche tant que le serveur n'a pas dit qui a forgé l'objet.
  std::string nameless_creator;

  // Le préfixe d'un œuf de familier qu'on a renommé.
  std::string beloved_pet;

  // LES CINQ RANGS D'INTIMITÉ D'UN FAMILIER, du plus distant au plus fidèle.
  // Ce sont les libellés de sa fiche : l'œuf dit la même
  // chose que la fiche du familier qui en sort.
  std::array<std::string, 5> pet_intimacy;

  // LE SUFFIXE DU BANDEAU D'OBTENTION.
  // 🔴 SON ESPACE DE TÊTE LUI APPARTIENT : « ` - %d obtained.` ». Celui qui
  // concatène n'en ajoute pas une.
  std::string obtained_suffix;

  // Ce que le client dit du ramassage dans le fil de conversation.
  ItemPickupWords pickup;

  // CE QU'IL DIT D'UN SERTISSAGE, réussi ou refusé. Se lisent par
  // `CardInsertText`.
  std::string card_inserted;
  std::string card_refused;

  // CE QU'IL DIT QUAND LE SERVEUR ANNONCE SI NOTRE ÉQUIPEMENT EST MONTRÉ. Se
  // lisent par `EquipShownText`.
  std::string equip_hidden;
  std::string equip_shown;

  // CE QUE LA BANNIÈRE DES COMPÉTENCES DE COUPLE AJOUTE au nom du partenaire.
  // Se lit par `LoveSkillText`.
  std::string love_skill;

  // Le mot de chaque carte connue.
  //
  // ⚠ UNE ENTRÉE PEUT PORTER UN MOT VIDE, et ce n'est pas une absence : le
  // client distingue les deux. « Présente dans la table » décide si l'objet
  // affiche des mots de cartes ; « porte un mot non vide » décide si CETTE
  // carte en écrit un. Confondre les deux ferait disparaître le mot des autres
  // cartes d'un objet qui en porte une muette.
  std::unordered_map<std::uint32_t, CardWord> cards;

  // Ce qu'il dit des coups : voir `BattleWords`.
  BattleWords battle;

  // Ce qu'il écrit sur la fiche d'un monstre : voir `MonsterWords`.
  MonsterWords monster;

  // Ce qu'écrit la fenêtre des destinations : voir `WarpListWords`.
  WarpListWords warp_list;

  // Ce qu'il écrit de la mémorisation d'un point de Warp : voir `WarpMemoWords`.
  WarpMemoWords warp_memo;

  // Ce qu'écrit la saisie de Talkie Box : voir `TalkboxWords`.
  TalkboxWords talkbox;

  // Ce qu'écrit la boutique de vote : voir `CashShopWords`.
  CashShopWords cash_shop;

  // Ce qu'écrivent les ateliers : voir `CraftWords`.
  CraftWords craft;

  // Ce qu'écrivent les commerces : voir `ro/vfs/trade_words.h`.
  TradeWords trade;

  // Ce qu'écrivent les salons : voir `ro/vfs/chat_room_words.h`.
  ChatRoomWords chat_room;

  // Ce que disent les refus d'un chuchotement : voir `ro/vfs/whisper_words.h`.
  WhisperWords whisper;

  // Ce qu'écrit la fenêtre des succès : voir `ro/vfs/achievement_words.h`.
  AchievementWords achievement;

  // LE TITRE DE LA LISTE DES CHARRETTES. Se lit par `CartListTitle`.
  std::string cart_list_title;

  [[nodiscard]] const CardWord* FindCard(std::uint32_t card_id) const;

  // Le mot de multiplicité pour N cartes identiques, ou rien si N n'en demande
  // pas.
  [[nodiscard]] std::string_view RepeatedWord(std::size_t identical) const;

  // Le possessif d'un objet forgé : celui de son élément, sauf pour les deux
  // anneaux de mariage.
  [[nodiscard]] std::string_view PossessiveWord(std::uint32_t item_id, std::size_t element) const;

  // La force de forge pour ce nombre d'étoiles, ou rien.
  [[nodiscard]] std::string_view ForgeStrengthWord(std::size_t stars) const;
};

// Le gabarit ne porte-t-il QU'UNE conversion, et est-elle entière ? « %% » ne
// compte pas : c'est un pour-cent littéral.
[[nodiscard]] bool FormatTakesOneInt(std::string_view format);

// Le suffixe du bandeau, formaté pour cette quantité.
//
// ⚠ LE GABARIT VIENT D'UN CATALOGUE DE LANGUE, qu'un joueur peut retoucher : un
// « %s » qui s'y glisserait lirait un pointeur pris sur la pile. On n'accepte
// donc qu'une conversion entière, et on rend sinon un texte VIDE.
[[nodiscard]] std::string ObtainedText(const ItemWords& words, int amount);

// Le gabarit porte-t-il EXACTEMENT une conversion de chaîne PUIS une conversion
// entière, dans cet ordre ? « %% » ne compte pas, et toute autre conversion —
// « %n » en tête — fait refuser le gabarit entier.
//
// 🔴 L'ORDRE COMPTE AUTANT QUE LE COMPTE. Un gabarit « %d » puis « %s » passe un
// entier là où la libc attend un pointeur : elle le déréférence, et le client
// tombe. Un tel gabarit se refuse, il ne se réordonne pas.
[[nodiscard]] bool FormatTakesStringThenInt(std::string_view format);

// « Vous avez obtenu Red Potion (5). » — la ligne du succès.
//
// ⚠ LE NOM EST UN ARGUMENT, JAMAIS UN GABARIT : un « % » qu'il porterait n'est
// pas interprété. Ce qui vient d'un catalogue qu'on peut retoucher, c'est le
// GABARIT, et on rend un texte VIDE dès qu'il ne présente pas exactement « %s »
// puis « %d ».
[[nodiscard]] std::string GotItemText(const ItemWords& words, std::string_view item_name,
                                      int amount);

// « Vous avez jeté Red Potion (5). » — même contrat que `GotItemText`.
[[nodiscard]] std::string DroppedItemText(const ItemWords& words, std::string_view item_name,
                                          int amount);

// LE SENS D'UN REFUS DE RAMASSAGE, et rien de son numéro.
//
// 🔴 CE N'EST PAS UN CODE DU PROTOCOLE. Plusieurs codes partagent un même
// message, et la correspondance appartient à la couche qui décode le paquet :
// `vfs` ne sait que nommer les textes.
enum class ItemPickupRefusal {
  kCannotGet,
  kTooHeavy,
  kOverWeightLimit,
  kOneKindLimit,
  kSameItemLimit,
};

// Le texte d'un refus, ou vide quand rien ne l'a posé — l'appelant n'écrit
// alors aucune ligne.
//
// 🔴 PASSER PAR ICI PLUTÔT QUE DE LIRE LE CHAMP est ce qui garantit qu'aucun
// refus ne reste muet — le défaut même que ce chemin répare.
//
// La vue rendue vit aussi longtemps que `words` : elle ne pend pas.
[[nodiscard]] std::string_view PickupRefusalText(const ItemWords& words, ItemPickupRefusal refusal);

// LA LIGNE D'UN SERTISSAGE, réussi ou refusé, ou vide quand rien ne l'a posée.
[[nodiscard]] std::string_view CardInsertText(const ItemWords& words, bool inserted);

// LA PHRASE DES COMPÉTENCES DE COUPLE, même règle.
[[nodiscard]] std::string_view LoveSkillText(const ItemWords& words);

// L'ANNONCE DE NOTRE ÉQUIPEMENT, montré ou caché aux autres, même règle.
[[nodiscard]] std::string_view EquipShownText(const ItemWords& words, bool shown);

// LE TITRE DE LA LISTE DES CHARRETTES, même règle.
[[nodiscard]] std::string_view CartListTitle(const ItemWords& words);

// POSE LES MOTS DE L'INTERFACE DANS LA LANGUE COURANTE : tous ceux que le client
// d'origine tirait de sa table de messages, et que nous traduisons comme le
// reste de l'interface (ADR 0090). Les mots de cartes, eux, ne bougent pas.
//
// ⚠ À REJOUER À CHAQUE CHANGEMENT DE LANGUE : les mots sont posés, pas lus à la
// demande.
void FillMessageWords(ItemWords& words);

// Les deux fichiers d'où sortent les mots de cartes.
inline constexpr std::string_view kCardWordPath = "data\\cardprefixnametable.txt";
inline constexpr std::string_view kCardWordAfterPath = "data\\cardpostfixnametable.txt";

// Lit les mots de cartes, et pose ceux de l'interface dans la langue courante.
// Un fichier ABSENT rend les mots de cartes vides plutôt qu'une erreur : sans
// eux un nom reste lisible, il perd seulement ses ornements. Un fichier
// ILLISIBLE, lui, est une erreur.
[[nodiscard]] core::Result<ItemWords> ReadItemWords(Vfs& vfs);

}  // namespace ro::vfs
