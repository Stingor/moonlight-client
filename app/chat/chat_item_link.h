#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/style_code.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_log.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_monster_link.h"
#include "chat/chat_mvp_link.h"
#include "chat/chat_navi_search_link.h"
#include "chat/chat_place_link.h"
#include "chat/chat_recipe_link.h"
#include "items/creator_names.h"
#include "options/setting_links.h"

namespace moonclient {

// CE QU'UN LIEN DE LA CONVERSATION DÉSIGNE VRAIMENT.
//
// 🔴 UNE SEULE TABLE POUR LES DEUX ESPÈCES, et surtout pas une par genre. Le
// jeton que `ui` rend est un RANG : deux numérotations parallèles se
// confondraient au premier geste, et un clic sur un lien de style ouvrirait la
// description de l'objet portant le même rang — sans que rien ne le signale.
struct ChatLinkTarget {
  ro::ui::ChatLinkKind kind = ro::ui::ChatLinkKind::kItem;
  // OBJET : la balise RELUE, et non l'identifiant seul. Elle porte le
  // raffinement, les cartes, le grade, le forgeron — reposer le lien doit
  // rendre LE MÊME objet, et cela ne se reconstruit pas depuis un nom.
  ro::vfs::ItemLink item;
  // STYLE : le code TEL QU'IL A VOYAGÉ, et l'étiquette qui l'accompagnait.
  //
  // ⚠ JAMAIS RÉ-ENCODÉ : un aller-retour perdrait ce qu'une version future y
  // ajouterait, et ce qu'on relaie ne serait plus ce qu'on a reçu.
  std::string code;
  std::string owner;
  // MONSTRE : la balise relue — identifiant, rang et nom, qu'on relaie tels
  // qu'ils ont voyagé.
  MonsterLink monster;
  // RECETTE : le produit et son nom, de même.
  RecipeLink recipe;
  // LIEU : la carte et la case, telles qu'elles ont voyagé.
  PlaceLink place;
  // JOUEUR : son nom, en UTF-8 — c'est par lui que tous ses gestes passent.
  std::string player;
  // RESPAWN DE MVP : la balise relue, telle qu'elle a voyagé.
  MvpLink mvp;
  // RESPAWN DE MVP seulement : l'expéditeur de la ligne, en UTF-8, que l'import
  // nomme au carnet du groupe ; vide quand la ligne ne le dit pas.
  std::string sender;
  // RECHERCHE DE NAVIGATION : la balise relue, telle qu'elle a voyagé.
  NaviSearchLink navi_search;
  // ADRESSE WEB : telle qu'elle était écrite dans la ligne.
  std::string url;
  // PSEUDO DISCORD : tel que le relai l'a écrit.
  std::string discord_name;
  // RÉGLAGE : la clé, telle qu'elle a voyagé. ⚠ RECONNUE ET OFFERTE À
  // L'ANALYSE : sans quoi il n'y aurait pas de lien.
  std::string setting_key;
};

// L'HEURE DU SERVEUR, pour décider qu'un respawn de MVP est échu. Sans elle
// (pointeur nul), aucun lien n'est jamais échu : c'est le cas des bulles.
struct MvpLinkClock {
  std::int64_t server_now_s = 0;
  // Le catalogue du lecteur, qui prête sa loi à un lien « mort à » qui n'en
  // porte pas. Nul : aucun catalogue.
  const ro::sim::MvpTrackerBook* book = nullptr;
};

// UNE LIGNE DE CONVERSATION DONT LES BALISES SONT DEVENUES DES NOMS.
//
// 🔴 CE TRAVAIL NE PEUT PAS SE FAIRE DANS `ui`. Ce qui arrive du serveur porte
// un `<ITEML>…</ITEML>` — un identifiant, un raffinement, des cartes —, et le
// traduire en « +7 Knife [3] » demande la table de l'installation et les mots du
// client. `ui` n'a ni l'une ni les autres, et ne doit pas les avoir : elle reçoit
// le texte déjà lisible et les bornes de ce qui se clique.
//
// ⚠ SANS CELA, LE JOUEUR LIT LA BALISE. Ce n'est pas un ornement manquant :
// n'importe quel joueur du serveur qui lie un objet remplit notre fil de
// « <ITEML>000021jn%07&00'00)00</ITEML> ».
struct DecoratedChatLine {
  std::string text;
  std::vector<ro::ui::ChatLinkSpan> links;
  // Ce que chaque lien porte, en regard de `links`.
  std::vector<ChatLinkTarget> carried;
  // LE TEXTE RICHE : ses styles, ses images, et l'adresse de chaque vignette en
  // regard de sa clé.
  std::vector<ro::ui::ChatStyleRun> styles;
  std::vector<ro::ui::ChatInlineImage> images;
  std::vector<std::string> picture_urls;
  // Où commence ce que la ligne DIT, après son auteur.
  std::uint32_t body_offset = 0;
};

// Les chevrons qui encadrent le nom, comme au client. ⚠ CE SONT EUX QUI DISENT
// QUE C'EST UN LIEN, avec la couleur : il n'y a pas de soulignement.
inline constexpr char kChatLinkOpen = '<';
inline constexpr char kChatLinkClose = '>';

// Remplace chaque balise par son nom composé et note ses bornes.
//
// Rend `false` — sans rien écrire — quand la ligne n'en porte aucune, ce qui est
// le cas de la quasi-totalité d'entre elles : l'appelant garde alors la vue sur
// le texte du monde, sans rien recopier.
//
// ⚠ UNE BALISE QU'ON NE SAIT PAS LIRE PASSE TELLE QUELLE — tronquée par le
// serveur, elle reste à l'écran plutôt que de disparaître : une ligne amputée en
// silence est pire qu'une ligne laide.
//
// ⚠ UN OBJET QUE LA TABLE IGNORE, LUI, N'EST PAS CE CAS-LÀ : la composition des
// noms a déjà son repli, « #12345 », et le lien reste donc lisible et cliquable.
// Le client, lui, écrit une espace.
//
// `creators`, facultatif, nomme le forgeron d'un item forgé lié. `maps`,
// facultatif, donne le nom affiché d'une carte : sans lui, un lieu montre le nom
// interne. `mvp_clock`, facultatif, marque « · passé » un respawn de MVP échu.
// `settings`, facultatif, dit ce que le lecteur a : un lien de réglage vers
// une destination absente — ou, sans lui, vers une destination conditionnelle —
// reste en texte inerte.
[[nodiscard]] bool DecorateChatLine(std::string_view raw, const ro::vfs::ItemTable& items,
                                    const ro::vfs::ItemWords& words, DecoratedChatLine* out,
                                    const CreatorNames* creators = nullptr,
                                    const ro::vfs::MapDisplayNames* maps = nullptr,
                                    const MvpLinkClock* mvp_clock = nullptr,
                                    const SettingReach* settings = nullptr);

// UNE LIGNE PEUT-ELLE PORTER UNE BALISE ? Les deux espèces s'ouvrent sur « < » :
// sans lui, il n'y a rien à décorer.
//
// 🔴 C'EST LA QUESTION QUE LE TRI DU FIL POSE À CHAQUE LIGNE, À CHAQUE IMAGE, et
// presque toutes répondent non. Elle se pose donc AVANT de bâtir la ligne
// décorée qu'attend `DecorateChatLine` : ses trois conteneurs, bâtis puis jetés
// pour chaque ligne, coûteraient plus que tout le reste du tri.
[[nodiscard]] bool MayCarryChatTags(std::string_view raw);

// ── LE LIEN D'UN STYLE ──────────────────────────────────────────────────────
//
// Le libellé VISIBLE d'un style partagé : « [Style: Pseudo] ».
//
// 🔴 COMPOSÉ CHEZ LE LECTEUR, jamais transporté tout fait : chacun le lit dans
// SA langue, et le « [Style: ] » d'un expéditeur anglophone n'a rien à imposer
// à son lecteur. C'est la balise qui voyage, pas sa décoration.
//
// ⚠ DES CROCHETS, ET NON LES CHEVRONS D'UN OBJET : deux espèces de lien qui se
// ressembleraient promettraient le même menu. Les joueurs du serveur les lisent
// déjà.
[[nodiscard]] std::string StyleLinkLabel(std::string_view owner);

// ── POSER UN LIEN DANS LA SAISIE ────────────────────────────────────────────
//
// 🔴 CE QUE LE JOUEUR TAPE N'EST PAS CE QUI PART. La balise fait une cinquantaine
// d'octets de charabia ; l'écrire dans la saisie rendrait la ligne illisible
// pendant qu'on la compose. On y pose donc le NOM entre chevrons — exactement ce
// que tout le monde lira une fois la ligne partie — et la balise attend de côté
// jusqu'à l'envoi.
//
// ⚠ LE LIBELLÉ POSÉ RESTE EN TEXTE ORDINAIRE : lui donner la couleur des liens
// demanderait de repeindre par-dessus le champ de saisie.
struct PendingChatLink {
  // Le libellé posé dans la saisie, chevrons compris.
  std::string shown;
  // La balise qui le remplacera à l'envoi.
  std::string wire;
  ro::ui::ChatLinkKind kind = ro::ui::ChatLinkKind::kItem;
  // Ce que sa pastille décrit au clic. Zéro pour un style, qui n'a pas
  // d'identifiant.
  std::uint32_t item_id = 0;
  // STYLE : de quoi répondre à un geste sur la pastille avant l'envoi — le
  // code et son étiquette, les mêmes que ceux de `ChatLinkTarget`.
  std::string code;
  std::string owner;
};

// COMBIEN DE LIENS UNE LIGNE PEUT PORTER.
//
// ⚠ C'EST LE NOMBRE DU CLIENT, et il a une raison mesurable : le serveur borne
// une ligne de conversation à 255 octets (`CHAT_SIZE_MAX`), et une balise en fait
// une cinquantaine. Trois en occupent déjà plus de la moitié.
inline constexpr std::size_t kMostChatLinks = 3;

// Le libellé d'un nom composé : le nom entre chevrons, comme au client.
[[nodiscard]] std::string ChatLinkLabel(std::string_view name);

// OUBLIE LES LIENS DONT LE LIBELLÉ N'EST PLUS DANS LA SAISIE.
//
// ⚠ SANS CELA LE PLAFOND SE BOUCHE : le joueur qui efface un lien doit pouvoir en
// reposer un autre. Le balayage va de GAUCHE à DROITE en consommant les liens
// dans l'ordre de pose — deux exemplaires du même objet donnent deux entrées
// distinctes, et celle que le joueur a effacée est simplement sautée.
void PruneChatLinks(std::string_view typed, std::vector<PendingChatLink>* pending);

// Rend la ligne À ENVOYER : chaque libellé posé reprend sa forme `<ITEML>`.
//
// ⚠ UNE FOIS CHACUN, ET DANS L'ORDRE, pour la même raison : deux liens sur le
// même objet portent le même libellé, et les remplacer tous d'un coup enverrait
// deux fois la première balise.
[[nodiscard]] std::string ResolveChatLinks(std::string_view typed,
                                           const std::vector<PendingChatLink>& pending);

// POSE LE LIBELLÉ D'UN LIEN AU BOUT DE CE QUE LA SAISIE PORTERA, séparé de ce
// qui précède. Faux, et rien ne change, quand la ligne porte déjà
// `kMostChatLinks` liens ou que le libellé n'y tiendrait pas.
//
// 🔴 CE QUE LA SAISIE PORTERA, ET NON CE QU'ELLE MONTRE : une ligne retenue
// pour sa longueur revient par le dépôt (`pending_insert`), avec ses liens.
// Les liens s'élaguent donc contre le texte ET le dépôt — contre le texte seul,
// ceux de la ligne retenue disparaîtraient et partiraient en texte brut. Et
// quand le dépôt attend, le libellé s'y AJOUTE : la ligne retenue d'abord, le
// lien posé ensuite, dans l'ordre où ils sont venus.
//
// L'appelant ajoute le lien à `pending` quand la pose réussit.
[[nodiscard]] bool PlaceChatLinkLabel(std::string_view label, ro::ui::ChatInputState& input,
                                      std::vector<PendingChatLink>* pending);

}  // namespace moonclient
