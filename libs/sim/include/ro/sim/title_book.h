#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LES TITRES DU PERSONNAGE, tels que le client les tient.
//
// 🔴 UN TITRE N'EST QU'UN NUMÉRO, ET LE FIL NE DIT JAMAIS LESQUELS ON POSSÈDE.
// La liste des succès ne porte aucun titre ; c'est le client qui la croise avec
// la table de l'installation — tel succès récompense tel titre. On retient donc
// ici ce que le serveur DIT — quels succès ont leur récompense réclamée —, et
// le croisement se fait là où l'on connaît la table.
//
// ⚠ LE SERVEUR EN ACCEPTE DAVANTAGE : il tient pour possédé tout titre d'un
// succès OBTENU, réclamé ou non. Le client, lui, ne propose que les réclamés, et
// c'est ce qu'on reproduit.
//
// ⚠ ELLE VIT HORS DU MONDE, qui touche son plafond de lignes, et elle n'a rien
// à y faire : rien ne la lit en même temps qu'un acteur. Elle naît avec la
// partie et meurt avec elle — ce qui est exactement quand le client d'origine
// la vide, au retour à l'écran de connexion.
class TitleBook {
 public:
  // Le titre PORTÉ. ⚠ ZÉRO VEUT DIRE « AUCUN », ET AUSSI « PAS ENCORE DIT » :
  // le serveur ne le pousse pas à l'entrée en jeu, il ne le dit que dans la
  // carte de nom du joueur — qu'il faut lui demander — et dans l'accusé d'un
  // changement.
  [[nodiscard]] std::int32_t equipped() const noexcept { return equipped_; }
  void TellEquipped(std::int32_t title_id) noexcept { equipped_ = title_id; }

  // Les succès dont la récompense est réclamée, dans l'ordre où on l'a appris.
  [[nodiscard]] std::span<const std::uint32_t> rewarded_achievements() const noexcept {
    return rewarded_;
  }

  // Note une récompense réclamée. Faux si elle l'était déjà.
  //
  // ⚠ RIEN NE S'EN RETIRE JAMAIS. La liste complète revient à chaque réclamation,
  // et le client ne fait qu'y AJOUTER ce qu'il n'avait pas : un succès absent
  // d'une liste ne reprend pas le titre qu'il avait donné.
  bool NoteRewarded(std::uint32_t achievement_id);

 private:
  std::int32_t equipped_ = 0;
  std::vector<std::uint32_t> rewarded_;
};

}  // namespace ro::sim
