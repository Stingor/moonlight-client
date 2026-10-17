#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/card_album_link.h"

namespace ro::ui {

// Ce qu'on sait d'une adresse web montrée : voir `chat_rich_art.h`.
struct ChatUrlFacts;

// ── UN LIEN D'OBJET, ET LES GESTES QUI VONT AVEC ─────────────────────────────
//
// 🔴 LES MÊMES PARTOUT OÙ UN LIEN SE MONTRE, et c'est le fond du sujet. Le fil,
// la barre de saisie et la table des drops passent tous par ici : une
// convention recopiée en trois exemplaires finit par diverger — le clic droit
// ouvrant une fiche ici et un menu là, le Maj+clic n'existant qu'à un endroit.
//
// Les surfaces disent donc ce qu'elles MONTRENT, et ce module dit ce que ça
// fait :
//
//   clic GAUCHE  → la description
//   MAJ + clic   → le lien reposé dans la saisie du chat
//   clic DROIT   → le menu, le même partout

// L'ORANGE DES LIENS, celui du client. ⚠ PAS DE SOULIGNEMENT : le lien se
// reconnaît à sa couleur et à ses chevrons, le trait salirait une ligne dense,
// et le client n'en met pas non plus. Au survol, la main suffit.
inline constexpr ImU32 kChatLinkColor = IM_COL32(0xF4, 0x93, 0x4A, 255);
// L'ombre rouge d'un objet CASSÉ, décalée d'un pixel sous le texte — le même
// rendu que dans la description et sur les cases.
inline constexpr ImU32 kChatDamagedShadow = IM_COL32(0xFA, 0x50, 0x50, 255);
inline constexpr float kChatDamagedShadowOffset = 1.0F;
// L'ORANGE ÉTEINT D'UN RESPAWN DE MVP ÉCHU : le lien reste un lien, mais sa
// fenêtre est passée, et la couleur le dit avant qu'on ne le lise.
inline constexpr ImU32 kChatLinkSpentColor = IM_COL32(0x96, 0x70, 0x52, 255);

// CE QU'UN MORCEAU DÉSIGNE.
//
// 🔴 DEUX ESPÈCES, ET LA SECONDE NE PARLE PAS DU MONDE. Un lien d'OBJET ne
// transporte qu'une DÉSIGNATION : l'objet existe chez le lecteur, sa table le
// nomme. Un STYLE n'existe nulle part hors de son porteur — la recette d'un
// joueur en zone disparaît avec lui —, si bien que sa balise transporte la
// RECETTE ENTIÈRE. Les deux ne s'ouvrent donc pas sur la même chose, et leur
// menu ne propose pas les mêmes gestes.
//
// ⚠ UN MONSTRE EST UNE DÉSIGNATION, comme un objet — mais le client ne sait pas
// le NOMMER : sa balise `<MOBL>` transporte le nom avec l'identifiant et le rang,
// et son clic ouvre la fiche que le serveur remplit.
//
// ⚠ UNE RECETTE DÉSIGNE UN ITEM, mais pas pour le décrire : son clic ouvre
// l'Atlas des recettes sur lui, et son aperçu montre le skill et les
// composants. Sa balise `<CRAF>` transporte le nom avec l'identifiant.
//
// ⚠ UN LIEU EST UNE DÉSIGNATION SANS IDENTIFIANT : sa balise `<NAVIL>` porte le
// nom interne d'une carte et deux coordonnées, et son clic lance le guidage.
//
// ⚠ UN JOUEUR N'A NI BALISE NI IDENTIFIANT : c'est le PSEUDO en tête d'un
// chuchotement, que le client place toujours au même endroit. Son clic
// PRÉPARE le chuchotement — le nom dans la case du destinataire, le clavier
// dans la saisie —, et n'ouvre aucune fenêtre : un clic sur un pseudo du fil
// arrive bien trop facilement pour ça. La conversation à part reste au menu.
//
// ⚠ UN RESPAWN DE MVP EST UN FAIT DATÉ, pas une désignation : sa balise `<MVPL>`
// porte le monstre, la carte, l'heure de mort ou l'instant du retour, et la
// tombe. Son clic ouvre le carnet de chasse sur le créneau, sans rien importer.
//
// ⚠ UNE RECHERCHE DE NAVIGATION EST UNE QUESTION, pas un lieu : sa balise
// `<NAVS>` porte une famille (carte, PNJ, monstre), un terme et parfois la
// carte où chercher. Son clic ouvre la navigation sur cette recherche.
//
// ⚠ UNE ADRESSE WEB EST UN TEXTE, pas une désignation : elle se lit dans la
// ligne même, et son clic passe par une confirmation avant le navigateur.
//
// ⚠ UN PSEUDO DISCORD N'EST PAS UN JOUEUR : il n'existe pas dans le jeu, et son
// menu ne propose que de le mentionner ou de copier son nom.
//
// ⚠ UN RÉGLAGE NE PARLE PAS DU MONDE MAIS DU CLIENT : sa balise `<SETL>` porte
// une CLÉ — une page du panneau, un onglet d'une fenêtre —, et son clic ouvre
// ce qu'elle désigne. Il n'a pas d'identifiant : c'est le JETON qui le retrouve.
enum class ChatLinkKind : std::uint8_t {
  kItem,
  kStyle,
  kMonster,
  kRecipe,
  kPlace,
  kPlayer,
  kMvp,
  kNaviSearch,
  kUrl,
  kDiscordName,
  kSetting,
};

// CE QUE LE MENU D'UN JOUEUR REFUSE D'AVANCE, et pourquoi : chaque raison, déjà
// traduite, grise son entrée et se lit dans son infobulle. Nulle, l'entrée est
// offerte.
//
// ⚠ COMPOSÉ PAR `app`, qui seul sait qui est de notre groupe, de notre guilde
// et de nos amis. Les raisons sont celles que le SERVEUR opposerait — le plus
// souvent sans un mot.
struct PlayerLinkRefusals {
  const char* party = nullptr;
  const char* guild = nullptr;
  const char* befriend = nullptr;
  // « Inviter dans mon carnet MVP » : la raison qui la grise, nulle si offerte.
  const char* mvp = nullptr;
  // Le carnet est éteint : l'entrée n'est pas même montrée.
  bool mvp_withdrawn = false;
};

// CE QUE LE MENU D'UN LIEN DE MVP SAIT D'AVANCE, composé par `app` qui seul
// tient le carnet. Il vit le temps d'une image, comme `PlayerLinkRefusals`.
//
// ⚠ IL SERT À TROIS MENUS : celui d'un `<MVPL>` (genre `kMvp`), celui du NOM
// d'un MVP du carnet (genre `kMonster`, faits non nuls) et celui d'un monstre
// de la navigation, qui ne connaît que sa classe de sprite. Un lien de monstre
// du chat, lui, n'en a pas : ni respawn ni guidage, et sa recherche part dès
// que son libellé n'est pas vide.
struct MvpLinkMenuFacts {
  // `kMvp` : la raison, déjà traduite, qui grise « Ajouter à mon carnet » ;
  // nulle, l'entrée est offerte.
  const char* import_refusal = nullptr;
  // `kMvp` : la raison, déjà traduite, qui grise « Ouvrir le carnet » — un
  // carnet éteint ; nulle, l'entrée est offerte.
  const char* open_refusal = nullptr;
  // La tombe est connue ET la carte nommée : « Guider vers la tombe ».
  bool tomb_known = false;
  // `kMonster` du carnet : la ligne porte une heure (mort ou instant exact),
  // donc « Partager le respawn dans le chat » a quelque chose à partager.
  bool carries_respawn = false;
  // `kMonster` du carnet : le nom n'est pas vide, donc « Où le trouver » a
  // quelque chose à chercher.
  bool named = false;
  // La carte est nommée : « Y aller » quand la tombe est inconnue.
  bool map_known = false;
  // `kMonster` : l'identifiant n'est qu'une CLASSE DE SPRITE. La fiche sait en
  // remonter ; le bestiaire du site et les deux commandes, qui ne lisent que
  // l'identifiant de la base, ne sont pas offerts.
  bool sprite_class_only = false;
};

// CE QUE LE MENU D'UNE RECHERCHE DE NAVIGATION SAIT D'AVANCE, composé par `app`
// qui seul lit la balise. Une valeur, et non un pointeur : deux champs.
struct NaviSearchMenuFacts {
  // La famille du terme : 0 carte, 1 PNJ, 2 monstre — les valeurs de la balise.
  std::uint8_t family = 0;
  // La recherche est bornée à une carte : « Y aller » et « Voir ce qu'il y a
  // là-bas » ont un lieu.
  bool bounded = false;
};

// UN MORCEAU DE LIGNE QUI DÉSIGNE QUELQUE CHOSE.
//
// 🔴 `ui` NE SAIT PAS LIRE UNE BALISE. Ce qui arrive du serveur porte un
// `<ITEML>…</ITEML>` que seule la table de l'installation sait traduire en nom :
// `app` a donc déjà remplacé la balise par le nom composé, et ne laisse ici que
// les bornes du morceau et ce que son clic doit ouvrir.
struct ChatLinkSpan {
  // Où le nom commence dans le `text` de la ligne, et sa longueur en octets.
  std::uint32_t offset = 0;
  std::uint32_t length = 0;
  ChatLinkKind kind = ChatLinkKind::kItem;
  // Ce que le clic décrit. ⚠ ZÉRO POUR UN STYLE : il n'a pas d'identifiant, et
  // c'est le JETON qui le retrouve.
  std::uint32_t item_id = 0;
  // Un équipement cassé prend son ombre rouge, ici comme ailleurs.
  bool damaged = false;
  // 🔴 CE QUE LE LIEN PORTE VRAIMENT, et que `ui` ne saura jamais lire : un
  // raffinement, des cartes, un grade, un forgeron. Reposer le lien doit rendre
  // LE MÊME objet, pas l'entrée de catalogue qui porte son nom. `ui` reçoit donc
  // un jeton opaque et le rend tel quel ; `app` seul sait ce qu'il désigne.
  std::uint32_t token = 0;
  // UN JOUEUR : ce que son menu refuse d'avance. Nul pour tout le reste. ⚠ IL
  // NE VIT QUE LE TEMPS DE L'IMAGE, comme le texte de la ligne.
  const PlayerLinkRefusals* refusals = nullptr;
  // UN RESPAWN DE MVP DONT LA FENÊTRE EST ÉCHUE : il se peint en orange éteint.
  bool spent = false;
  // La marque « · passé » d'un respawn échu, insérée DANS le morceau : son
  // début en octets depuis celui du morceau, et sa longueur (zéro : aucune
  // marque). Elle se peint, mais la tête du menu la retire : le menu nomme le
  // lien, pas son état.
  std::uint32_t mark_at = 0;
  std::uint32_t mark_length = 0;
  // `kMvp` : ce que son menu sait d'avance. Nul pour tout le reste. ⚠ IL NE VIT
  // QUE LE TEMPS DE L'IMAGE, comme `refusals`.
  const MvpLinkMenuFacts* mvp = nullptr;
  // `kNaviSearch` : ce que son menu sait d'avance. Ignoré pour tout le reste.
  NaviSearchMenuFacts navi;
  // `kUrl` : ce qu'on sait de l'adresse — son hôte, son aperçu. Nul pour tout
  // le reste. ⚠ IL NE VIT QUE LE TEMPS DE L'IMAGE, comme `mvp`.
  const ChatUrlFacts* url = nullptr;
};

// CE QU'UN GESTE SUR UN LIEN DEMANDE.
//
// ⚠ `ui` N'EN FAIT RIEN : ouvrir une adresse lance un programme du système,
// envoyer une commande parle au serveur. Les deux se décident chez `app`.
enum class ChatLinkGesture : std::uint8_t {
  kNothing,
  // Le clic gauche : la fenêtre de description. ⚠ POUR UN STYLE, C'EST
  // L'APERÇU — un pantin, le nôtre, habillé de ce style. Même geste, même
  // intention : « montre-moi ce que c'est ». Pour une RECETTE, c'est l'Atlas.
  kDescribe,
  // Maj + clic : le lien reposé dans la saisie, pour le repasser à quelqu'un.
  kRepost,
  // Le clic droit : le menu s'ouvre. ⚠ L'APPELANT FAIT L'`OpenPopup` LUI-MÊME —
  // l'identifiant d'un popup se hache avec la pile d'ids de SA fenêtre, et le
  // clic s'est produit sous celle d'une table ou d'un `PushID`.
  kMenu,
  // Les entrées du menu d'un OBJET.
  kOpenDatabase,
  kItemInfo,
  kWhoDrops,
  // Le Marché ouvert sur l'identifiant de l'item : qui le vend, qui l'achète.
  kMarketSearch,
  // La fiche de l'item dans la base de données du jeu (ADR 0116).
  kGameDatabase,

  // ── CE QU'UN LIEN DE STYLE PROPOSE EN PLUS ────────────────────────────────
  //
  // ⚠ ESSAYER N'APPLIQUE RIEN : le code se charge dans l'éditeur, et il faudra
  // valider. Un lien de chat vient d'un autre joueur — il ne doit pas pouvoir
  // changer une apparence d'un simple clic.
  kTryStyle,
  // Le code NU dans le presse-papiers, pour le coller ailleurs : un forum, un
  // ami hors du jeu.
  kCopyCode,

  // ── CE QU'UN LIEN DE MONSTRE PROPOSE EN PLUS ──────────────────────────────
  //
  // Le bestiaire du site, à la page du monstre. Son clic gauche, lui, ouvre la
  // fiche en jeu : c'est `kDescribe`.
  kOpenBestiary,

  // ── CE QU'UN LIEN DE LIEU PROPOSE EN PLUS ─────────────────────────────────
  //
  // La fenêtre de navigation ouverte sur la carte, pour VOIR l'itinéraire
  // plutôt que le suivre à l'aveugle — le guidage part aussi. Le nom de la
  // carte se copie par `kCopyCode` : c'est le nom interne, celui d'une commande.
  kOpenInNavigation,
  // Ce qu'il y a SUR la carte, par opposition à « y aller ».
  kPlaceContents,
  // Le staff : `@warp` sur le lieu. Offert au menu selon ses droits seulement.
  kStaffWarp,

  // ── CE QU'UN ITEM EN VENTE AU VOTE SHOP PROPOSE EN PLUS ──────────────────
  //
  // La boutique s'ouvre, l'item au panier.
  kVoteShop,

  // ── CE QU'UN PSEUDO PROPOSE EN PLUS ───────────────────────────────────────
  //
  // « Chuchoter… », le premier geste, est `kDescribe` — c'est aussi celui du
  // clic gauche. Le nom se copie par `kCopyCode`.
  kWhisperWindow,
  kInviteParty,
  kInviteGuild,
  kAddFriend,

  // ── CE QU'UN LIEN DE MVP, OU LE NOM D'UN MVP DU CARNET, PROPOSE EN PLUS ───
  //
  // ⚠ POUR UN `<MVPL>`, `kDescribe` OUVRE LE CARNET : la fiche du monstre a
  // donc son propre geste.
  kMonsterSheet,
  // Le respawn posé dans la saisie, en `<MVPL>`.
  kShareRespawn,
  // Le guidage jusqu'à la tombe.
  kGuideToTomb,
  // La recherche de la fenêtre de navigation sur le nom, monstres seulement.
  kWhereToFind,
  // Les commandes `@mobinfo <id>` et `@whereis <id>`, parties comme une ligne
  // tapée.
  kMobInfo,
  kWhereIs,
  // L'heure de mort du lien versée au carnet du groupe, comme une saisie.
  kAddToMvpLog,

  // ── CE QU'UN PSEUDO PROPOSE ENCORE ────────────────────────────────────────
  //
  // « Inviter dans mon carnet MVP ».
  kInviteMvp,

  // ── CE QU'UN NOM DE MONSTRE ET UNE RECHERCHE DE NAVIGATION PROPOSENT ──────
  //
  // Le lien `<NAVS>` « où le trouver » posé dans la saisie.
  kPostWhereToFind,
  // Le guidage vers la carte que désigne une recherche de navigation.
  kGoThere,

  // ── CE QU'UNE ADRESSE WEB PROPOSE EN PLUS ─────────────────────────────────
  //
  // « Ouvrir dans le navigateur » est `kDescribe`, « Copier l'adresse »
  // `kCopyCode`.
  //
  // L'aperçu de cette adresse-là, accordé pour cette fois.
  kShowImageOnce,
  // Son hôte admis pour de bon, après confirmation.
  kAllowImageHost,
  // L'image ouverte en grand dans la visionneuse.
  kViewImage,

  // ── CE QU'UNE CARTE PROPOSE EN PLUS ───────────────────────────────────────
  //
  // L'album de cartes ouvert sur sa pochette. Voir `card_album_link.h`.
  kSeeInAlbum,
};

// UN ITEM EN VENTE AU VOTE SHOP, et son prix.
//
// 🔴 LA LISTE VIENT DU SERVEUR, et elle peut ne pas être arrivée : une absence
// ne prouve RIEN. Le menu n'annonce donc que le POSITIF — jamais « pas au vote
// shop », qui affirmerait plus qu'on ne sait.
struct VoteShopOffer {
  std::uint32_t item_id = 0;
  std::int32_t price = 0;
};

// Le prix de l'item, rien s'il n'est pas dans la liste. ⚠ LA LISTE EST TRIÉE
// PAR IDENTIFIANT : c'est `app` qui la compose ainsi.
[[nodiscard]] std::optional<std::int32_t> VoteShopPriceOf(std::span<const VoteShopOffer> offers,
                                                          std::uint32_t item_id);

// CE QUE LE MENU D'UN OBJET SAIT D'AVANCE de la partie, composé par `app` une
// fois par image et prêté à toute surface qui porte des liens d'objet. Vide —
// une surface qui ne le reçoit pas —, il ne propose ni le panier du vote shop
// ni l'album.
struct ItemLinkFacts {
  // Le catalogue du vote shop.
  std::span<const VoteShopOffer> vote_shop;
  CardAlbumLinkFacts album;
};

// Ce qu'une surface portant des liens rapporte de son image.
struct ChatLinkAsk {
  ChatLinkGesture gesture = ChatLinkGesture::kNothing;
  ChatLinkKind kind = ChatLinkKind::kItem;
  // L'objet visé par le geste, ou zéro.
  std::uint32_t item_id = 0;
  // Celui que la souris SURVOLE, pour l'aperçu. ⚠ SÉPARÉ DU GESTE : on survole
  // bien plus souvent qu'on ne clique, et l'aperçu ne doit pas attendre un clic.
  std::uint32_t hovered_id = 0;
  // Ce que le lien survolé désigne : un monstre ne s'aperçoit pas comme un objet.
  ChatLinkKind hovered_kind = ChatLinkKind::kItem;
  // 🔴 ET SON JETON, avec sa surface : l'identifiant seul donnerait à l'aperçu
  // le nom de catalogue, sans le raffinement ni les cartes que le lien porte.
  // Zéro jeton est un jeton valable ; `hovered_id` nul dit qu'on ne survole rien.
  std::uint32_t hovered_token = 0;
  bool hovered_from_input = false;
  // Un lien est survolé. ⚠ C'EST LUI QUI LE DIT POUR UN LIEU : un lieu n'a pas
  // d'identifiant, et son `hovered_id` reste nul.
  bool hovering = false;
  // Le jeton du lien visé, rendu tel qu'il a été reçu.
  std::uint32_t token = 0;
  // ⚠ DE QUELLE SURFACE IL VIENT : les deux ont leur propre table de jetons, et
  // un index du fil lu dans celle de la saisie désignerait un autre objet.
  bool from_input = false;
  // LA SAISIE D'OÙ VIENT UN GESTE DE PASTILLE : vide pour la barre de la
  // conversation, le correspondant pour une saisie 1:1. Chacune a ses liens.
  std::string_view input_peer;
};

// LE GESTE LU SUR UNE ZONE SURVOLÉE, sans rien jouer. Pose le curseur « main ».
//
// ⚠ `hovered` EST FOURNI PAR L'APPELANT : aucune de ces surfaces n'est un élément
// ImGui. Le fil et la barre de saisie peignent leurs liens à la main, par-dessus
// une liste de dessin et par-dessus un champ de texte, et testent leurs
// rectangles eux-mêmes.
//
// 🔴 LE CLIC GAUCHE SE JOUE AU RELÂCHEMENT, comme les cases du sac : la fenêtre
// de description naît à l'image suivante, et partie de l'enfoncement elle
// s'ouvrirait derrière la fenêtre d'où le clic vient de partir.
[[nodiscard]] ChatLinkGesture HitChatLink(bool hovered);

// LE MENU D'UN LIEN, le même partout. Rend ce que le joueur y a choisi.
//
// ⚠ LE PANIER DU VOTE SHOP N'Y PARAÎT QUE POUR UN ITEM À SON CATALOGUE :
// `link_facts.vote_shop` est ce catalogue, et une surface qui ne le reçoit pas
// ne le propose pas. « Voir dans l'album » suit `link_facts.album` de même.
// Les deux commandes, elles, passent par le chat comme une ligne tapée.
// ⚠ LA LISTE DE RAMASSAGE AUTOMATIQUE N'Y EST PAS : elle se règle depuis la
// description.
//
// ⚠ LE GENRE DÉCIDE DES ENTRÉES, et il vient du morceau visé — jamais d'une
// devinette sur l'identifiant : un style n'en a pas, et « identifiant nul donc
// style » confondrait un lien de style avec un objet que la table ignore.
//
// `refusals` grise les entrées d'un JOUEUR ; nul, elles sont toutes offertes.
// `mvp` porte ce que le menu d'un MVP sait d'avance (voir `MvpLinkMenuFacts`),
// `navi` celui d'une recherche de navigation (voir `NaviSearchMenuFacts`),
// `url` celui d'une adresse web (voir `ChatUrlFacts`). `staff_warp` offre le
// `@warp` du staff sur un lieu.
[[nodiscard]] ChatLinkAsk DrawChatLinkMenu(
    const char* popup_id, ChatLinkKind kind, std::uint32_t item_id, std::uint32_t token,
    std::string_view label, ItemLinkFacts link_facts = {},
    const PlayerLinkRefusals* refusals = nullptr, const MvpLinkMenuFacts* mvp = nullptr,
    NaviSearchMenuFacts navi = {}, const ChatUrlFacts* url = nullptr, bool staff_warp = false);

// « INVITER DANS MON CARNET MVP », LA MÊME ENTRÉE pour le menu d'un pseudo et
// celui d'un membre de guilde ; le menu d'entité, qui a sa propre forme de
// ligne, en reprend le libellé et les infobulles. Grisée, sa raison se lit au
// survol ; offerte, son infobulle dit la règle. Rend vrai au clic.
[[nodiscard]] bool MvpInviteMenuItem(const char* refusal);

// L'INFOBULLE D'UN LIEN DE RÉGLAGE : le chemin de ce qu'il ouvre, déjà composé et
// traduit par `app`, puis les deux gestes. ⚠ ELLE NE TESTE PAS LE SURVOL : c'est
// l'appelant qui la pose à l'image où le lien est survolé.
void DrawSettingLinkTip(std::string_view path);

}  // namespace ro::ui
