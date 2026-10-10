#pragma once

#include <cstdint>

#include "ro/sim/item_news.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/creator_names.h"

namespace moonclient {

// CE QUE LE RAMASSAGE DIT DANS LA CONVERSATION.
//
// Deux faces d'un même paquet, et le client d'origine écrit les deux : le GAIN
// en cyan — « You got Red Potion (5). » —, le REFUS en rouge — « You cannot
// carry more items because you are overweight. ».
//
// 🔴 C'EST INDÉPENDANT DU BANDEAU, chez le client comme ici. Le bandeau
// s'efface au bout de cinq secondes ; le fil, lui, garde la trace, et c'est par
// lui qu'on relit ce qu'on a ramassé pendant une récolte. Couper l'un ne coupe
// pas l'autre.
//
// 🔴 ET C'EST ICI QUE LA PHRASE SE COMPOSE, parce que c'est le seul étage qui
// voit les deux moitiés : `net` a décodé le code mais ne connaît aucun texte de
// l'installation, `vfs` porte les textes mais aucun protocole. Le monde tient
// entre les deux les faits datés — ce qui est entré, ce qui a été refusé — et
// ce module les relève par leur RANG, comme le bandeau relève les siens.
class ItemPickupReport {
 public:
  // ⚠ LA TABLE DES OBJETS EST FACULTATIVE, et c'est ce qui sépare les deux
  // moitiés : le REFUS ne nomme aucun objet — le paquet d'un refus ne le porte
  // pas —, il parle donc sans elle. Seul le GAIN veut un nom, et se tait quand
  // l'installation n'en donne pas.
  ItemPickupReport(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords& words)
      : items_(items), words_(&words) {}

  // Écrit ce que les ramassages nouveaux ont à dire, depuis le dernier passage.
  //
  // ⚠ LE MONDE N'EST PAS CONSTANT : c'est lui qui porte la conversation, et
  // c'est dedans qu'on écrit.
  //
  // ⚠ À APPELER DANS LE POMPAGE RÉSEAU, pas dans le dessin : une image sautée
  // ne doit pas perdre une ligne.
  //
  // ⚠ AUCUNE HORLOGE EN ARGUMENT, et ce n'est pas un oubli : chaque fait porte
  // DÉJÀ son instant, celui de sa réception. Dater la ligne du moment où on la
  // relève la ferait paraître plus récente que ce qui l'a suivie.
  //
  // 🔴 UN ITEM FORGÉ ATTEND SON FORGERON, trois secondes au plus, et tout ce
  // qui suit avec lui : voir `AwaitsCreator`. ⚠ `now_ms` NE DATE RIEN, il ne
  // sert qu'à borner cette attente.
  void Notice(ro::sim::World& world, const CreatorNames& creators, std::uint32_t now_ms);

  // Écrit les nouvelles d'objets que le serveur a données, et les prend :
  //
  //   · le ramassage d'un MEMBRE DU GROUPE, en cyan, dans la case « items
  //     obtenus par le groupe » — seulement d'un autre que soi, et d'un compte
  //     que la liste du groupe connaît ;
  //   · une pièce ABÎMÉE, en rouge, dans la case « équipement endommagé ». La
  //     mienne se nomme par la pièce portée à cet emplacement, et se tait quand
  //     rien n'y est porté ; celle d'un membre du groupe se nomme par la partie
  //     touchée — main droite, main gauche, armure, tête —, et se tait ailleurs.
  //
  // Voir `item_news_report.cpp`.
  void NoticeNews(ro::sim::ItemNews& news, ro::sim::World& world, const ro::sim::PartyRoster& party,
                  const CreatorNames& creators);

  // ⚠ PAS D'OUBLI À DEMANDER, contre le bandeau qui en a un : ce module ne
  // porte aucun affichage — ce qu'il a dit est parti dans la conversation, qui
  // traverse les cartes comme tout ce qui a été dit. Et ses témoins de rang ne
  // doivent surtout pas reculer : les journaux du sac survivent au changement
  // de carte, si bien que les remettre à zéro réécrirait d'un coup les
  // dernières lignes de la carte précédente. Une vraie nouvelle session se
  // reconnaît au rang qui repart de zéro, et `Notice` s'en charge.

 private:
  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;

  std::uint64_t seen_obtained_seq_ = 0;
  std::uint64_t seen_refused_seq_ = 0;
  std::uint64_t seen_dropped_seq_ = 0;
};

// LES DEUX COULEURS, celles du client — RELUES DANS LE BON ORDRE D'OCTETS.
//
// 🔴 LA COULEUR D'UNE LIGNE DE CHAT EST UN `COLORREF` WINDOWS, `0x00BBGGRR`,
// parce que c'est ce que le client passe à GDI : le ROUGE y est l'octet de
// poids FAIBLE. Notre `sim::ChatLine::color`, lui, est en `0xRRGGBB`. Les deux
// valeurs du client se renversent donc, et l'une d'elles ne le crie pas :
//
//   refus, brut `0x0000FF`   → R=FF          → rouge   → `0xFF0000` ici
//   gain,  brut `0xFFFF00`   → G=FF, B=FF    → CYAN    → `0x00FFFF` ici
//
// ⚠ LE GAIN N'EST PAS JAUNE, et c'est le piège : `0xFFFF00` se lit « jaune » à
// l'œil d'un lecteur habitué au RGB, et recopié tel quel il DONNE du jaune —
// une couleur que le client n'affiche jamais. Même règle ailleurs : le rouge du
// message d'accueil arrive en `0x0000FF`, et le lire comme du RGB le rend bleu.
inline constexpr std::uint32_t kPickupGainColor = 0x00FFFF;
inline constexpr std::uint32_t kPickupRefusalColor = 0xFF0000;
// Un objet jeté : la couleur d'un gain, puisque c'est le même genre d'avis.
inline constexpr std::uint32_t kPickupDropColor = kPickupGainColor;

}  // namespace moonclient
