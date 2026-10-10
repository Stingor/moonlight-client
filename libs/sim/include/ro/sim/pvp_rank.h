#pragma once

#include <cstdint>
#include <map>

namespace ro::sim {

// ── LE CLASSEMENT PVP DE LA CARTE ───────────────────────────────────────────
//
// `ZC_NOTIFY_RANKING 0x019a` : le rang du joueur et le nombre de combattants.
//
// 🔴 UNE SEULE CHOSE, MONTRÉE À TROIS ENDROITS. Le client d'origine en tire le
// compteur « rang/total » en bas à droite, l'anneau posé sous quiconque entre
// dans le top dix, et un chronomètre au centre haut. Les trois s'allument
// ensemble, et l'on prendrait volontiers une capture qui les montre pour trois
// travaux séparés : c'est un seul paquet.
//
// ⚠ LE SERVEUR NE PARLE QUE SUR CHANGEMENT. `pc_calc_pvprank` compare le rang
// ET le nombre de joueurs, et se tait si les deux tiennent — son minuteur bat
// pourtant toutes les secondes. Ne rien recevoir est donc le cas ORDINAIRE, et
// non le signe que quelque chose manque.
//
// ⚠ ET IL ARRIVE EN ZONE, PAS EN PRIVÉ. `clif_pvpset` diffuse en AREA : on
// reçoit le rang des AUTRES joueurs autant que le sien. Le client d'origine
// n'écrit ses deux champs d'affichage QUE lorsque l'identifiant du paquet est le
// sien — les rangs d'autrui ne touchent pas son compteur. Voir
// `ro/net/pvp_rank.h` pour ce qu'il en fait par ailleurs.

// LE RANG ET LE TOTAL, tels que le serveur les compte.
//
// ⚠ SIGNÉS, ET CE N'EST PAS UN DÉTAIL. Le serveur envoie `UINT32_MAX` pour le
// rang d'un joueur caché, avec le commentaire « On client displays as -- ». Ce
// commentaire est FAUX pour ce client : lus en entier signé, ces quatre octets
// valent moins un, et la garde de dessin — signée elle aussi — ne dessine alors
// RIEN. Il n'existe d'ailleurs aucun glyphe tiret dans sa fonte de chiffres.
// Les lire en non signé afficherait « 4294967295/2 ».
struct PvpRank {
  // Zéro tant que le serveur n'a rien dit, et zéro de nouveau quand il éteint
  // la zone — il envoie alors explicitement un rang et un total nuls.
  std::int32_t rank = 0;
  std::int32_t total = 0;
  // DEPUIS QUAND CE JOUEUR TIENT LA TÊTE, dans l'horloge de l'APPELANT. Zéro
  // quand il n'est pas premier.
  //
  // 🔴 C'EST LE CLIENT QUI COMPTE, ET AUCUN PAQUET NE LE PORTE. Le serveur ne
  // dit que le rang ; le chronomètre part quand le rang DEVIENT un, et se
  // remet à zéro dès qu'on le perd. Le chercher dans le protocole est une
  // impasse — c'est ce qui a fait chercher un minuteur de salle qui n'existe
  // pas.
  //
  // ⚠ IL NE REPART PAS TANT QU'ON RESTE PREMIER : un paquet qui redit « rang
  // un » parce que le TOTAL a changé ne doit pas remettre le compteur à zéro.
  std::uint32_t leader_since_ms = 0;
};

// Le rang qui déclenche le chronomètre, et celui du dernier des marqués.
//
// ⚠ DEUX SEUILS DIFFÉRENTS, et les confondre ferait tourner l'anneau sous un
// seul joueur : le chronomètre ne vaut que pour le PREMIER, l'anneau pour les
// DIX premiers.
inline constexpr std::int32_t kPvpLeaderRank = 1;
inline constexpr std::int32_t kPvpMarkedRanks = 10;

// L'ANNEAU QUI MARQUE LES DIX PREMIERS : `EF_TOPRANK`, l'effet 159.
//
// 🔴 CE N'EST PAS UN MARQUEUR D'HOSTILITÉ, et c'est ce qui a fait chercher
// pendant longtemps un « anneau des joueurs attaquables » qui n'existe pas. Sa
// condition est le CLASSEMENT ; le client d'origine ne pose aucune figure au
// sol à partir de ce qu'on peut frapper. Voir la fiche 0055.
//
// ⚠ ET SA COULEUR PORTE LE RANG chez le client d'origine : presque blanche au
// premier, rouge vif au dixième. Nous le jouons par le moteur d'effets, qui ne
// reçoit qu'un numéro — la teinte par rang reste donc à faire, et elle se
// vérifie sur une capture côte à côte.
inline constexpr std::uint32_t kPvpMarkEffectId = 159;

// Vrai quand le classement a de quoi se montrer.
//
// ⚠ LA MÊME GARDE QUE LE CLIENT D'ORIGINE, et elle est SIGNÉE des deux côtés :
// un rang ou un total nul ou négatif n'affiche rien du tout, plutôt qu'un
// « 0/0 » que le serveur envoie pourtant en quittant la zone.
[[nodiscard]] constexpr bool PvpRankShown(const PvpRank& rank) noexcept {
  return rank.rank > 0 && rank.total > 0;
}

// Vrai quand ce rang vaut l'anneau des dix premiers.
//
// ⚠ SIGNÉE ELLE AUSSI, et la borne basse compte autant que la haute : le rang
// d'un joueur caché vaut moins un, et un test qui ne garderait que « au plus
// dix » lui poserait l'anneau.
[[nodiscard]] constexpr bool PvpRankMarked(std::int32_t rank) noexcept {
  return rank > 0 && rank <= kPvpMarkedRanks;
}

// ── L'ANNEAU EST UNE CONSÉQUENCE, ET NON UN ÉVÉNEMENT ───────────────────────
//
// 🔴 C'EST CE QUI LE FAIT REVENIR, et l'oublier a coûté trois symptômes qui
// paraissaient sans rapport : l'anneau n'apparaissait qu'à une connexion
// FRAÎCHE, ne revenait pas après une mort, et ne revenait pas non plus quand son
// porteur ressortait puis rentrait dans la vue.
//
// La cause est unique : le serveur ne redit un rang que lorsqu'il CHANGE.
// `pc_calc_pvprank` compare le rang ET le nombre de joueurs, et `clif_pvpset`
// n'est appelé de nulle part ailleurs — il n'existe AUCUN envoi à l'entrée en
// vue. Poser l'anneau au seul moment du paquet le perdait donc pour de bon dès
// que son porteur disparaissait, et les trois cas ci-dessus sont trois façons de
// remplacer un acteur par un acteur neuf.
//
// D'où les deux moitiés, qui ne valent rien l'une sans l'autre : le RANG se
// garde dans un carnet qui survit à son porteur, et l'ANNEAU se relit du rang à
// chaque battement — voir `World::RefreshPvpRings`.
//
// ⚠ ET LA SECONDE DOIT ÊTRE MUETTE : elle compare le rang PORTÉ par l'effet à
// celui de l'acteur, et ne repose que s'ils diffèrent. Sans cette comparaison,
// l'anneau rejouerait son animation à chaque image.

// LE CLASSEMENT TEL QUE LE MONDE LE TIENT, et la règle du chronomètre.
//
// ⚠ IL VIT À PART DU MONDE parce qu'il n'a besoin de rien d'autre que de
// lui-même : décider quand le chronomètre du premier part ne demande de
// regarder ni les acteurs, ni la carte, ni le sac. Ce qui touche aux acteurs —
// l'anneau posé sous les dix premiers — reste dans `World`, seul à les porter.
class PvpStanding {
 public:
  // CE QUE LE SERVEUR VIENT DE DIRE DE NOTRE RANG.
  //
  // 🔴 LE CHRONOMÈTRE PART QUAND LE RANG DEVIENT UN, ET PAS À CHAQUE FOIS QU'IL
  // VAUT UN. Le serveur reparle dès que le rang OU LE TOTAL change : un joueur
  // qui entre sur la carte pendant qu'on est premier fait repartir le paquet
  // avec le même rang. Le remettre à zéro à chaque annonce ferait clignoter le
  // compte à chaque arrivée, et il ne dépasserait jamais une seconde sur une
  // carte fréquentée.
  constexpr void Note(std::int32_t rank, std::int32_t total, std::uint32_t now_ms) noexcept {
    if (rank == kPvpLeaderRank) {
      if (rank_.rank != kPvpLeaderRank) {
        rank_.leader_since_ms = now_ms;
      }
    } else {
      rank_.leader_since_ms = 0;
    }
    rank_.rank = rank;
    rank_.total = total;
  }

  // LE RANG D'UN AUTRE COMBATTANT, gardé QU'IL SOIT EN VUE OU NON.
  //
  // 🔴 LE SERVEUR NE PARLE QUE SUR CHANGEMENT, et jamais à l'entrée en vue :
  // `pc_calc_pvprank` compare le rang ET le nombre de joueurs, et se tait si les
  // deux tiennent — il n'existe aucun appel de `clif_pvpset` au moment où un
  // personnage entre dans la vue d'un autre. Le rang reçu une fois est donc le
  // SEUL qu'on aura, et le lier à un acteur qui va disparaître le perd.
  //
  // ⚠ C'EST CE QUI FAISAIT QUE L'ANNEAU NE REVENAIT JAMAIS. Un joueur qui meurt
  // et revient, ou qui sort de la vue et y rentre, est un acteur NEUF : son rang
  // repartait de zéro, aucun paquet ne venait le redire, et il ne portait plus
  // rien. Vu en jeu, trois fois de suite.
  void NoteActor(std::uint32_t id, std::int32_t rank);

  // Zéro pour qui n'a jamais été classé — la même valeur que « pas de rang ».
  [[nodiscard]] std::int32_t RankOf(std::uint32_t id) const;

  // LE CLASSEMENT S'OUBLIE EN CHANGEANT DE CARTE : il appartient à la carte
  // qu'on quitte, et le serveur ne le redit qu'une fois l'entrée faite.
  //
  // ⚠ LE CARNET AVEC, et c'est la même raison : les rangs d'une carte ne disent
  // rien de la suivante, et les garder poserait des anneaux sur des inconnus.
  void Forget() {
    rank_ = PvpRank();
    ranks_.clear();
  }

  [[nodiscard]] constexpr const PvpRank& rank() const noexcept { return rank_; }

 private:
  PvpRank rank_;
  // ⚠ BORNÉ PAR LA CARTE, pas par nous : il ne porte que les combattants dont un
  // rang est arrivé, et `Forget` le vide à chaque entrée.
  std::map<std::uint32_t, std::int32_t> ranks_;
};

}  // namespace ro::sim
