#pragma once

#include <cstdint>
#include <deque>
#include <span>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"
#include "ro/ui/chat_whisper_list.h"

#include "chat/whisper_peer.h"

namespace moonclient {

// LES DERNIERS CORRESPONDANTS, ceux que la box « Pseudo » propose.
//
// 🔴 DANS LES DEUX SENS : celui à qui l'on vient de chuchoter, et celui qui
// vient de nous chuchoter — c'est souvent à lui qu'on veut répondre. Le plus
// récent en tête ; un nom qui revient REMONTE au lieu de se dupliquer, et le
// sens du dernier échange l'emporte.
//
// ⚠ SEULEMENT DES PERSONNAGES. Ce que le serveur chuchote lui-même, sous un
// nom de sa configuration, n'y entre pas : voir `FollowChat`.
class RecentWhispers {
 public:
  // Retient un correspondant, dans ce sens-là. Un nom vide ne se retient pas.
  void Note(std::string_view name, ro::ui::ChatWhisperWay way);

  // RETIENT LES CHUCHOTEMENTS ÉCHANGÉS depuis le dernier appel, dans l'ordre
  // d'arrivée : ceux qu'on a reçus, et l'écho de ceux qu'on a envoyés — d'une
  // conversation 1:1 comme de la barre.
  //
  // 🔴 L'AUTEUR D'UN REÇU DOIT ÊTRE UN PERSONNAGE CONNECTÉ, ce que dit un identifiant non
  // nul. Le serveur chuchote aussi en son nom — réponses de commandes, avis —
  // sous un nom que sa configuration choisit et que ce client ne peut pas
  // connaître ; un PNJ de même. Leur identifiant est nul. ⚠ LA LIMITE : un
  // joueur servi par un AUTRE serveur de carte arrive lui aussi sans
  // identifiant, et n'entre pas.
  //
  // ⚠ ET JAMAIS SOI-MÊME : se chuchoter n'ouvre aucune conversation.
  void FollowChat(std::span<const ro::sim::ChatLine> chat, std::string_view own_name);

  // UNE PARTIE COMMENCE : ce que le monde porte déjà a été dit avant, et n'est
  // pas neuf.
  void StartSession(std::uint64_t last_seq) noexcept { heard_through_ = last_seq; }

  // Ce que la partie d'avant avait retenu. ⚠ REMPLACE la liste, et la borne.
  void Restore(std::span<const WhisperPeer> peers);

  // Oublie tous les correspondants. ⚠ LE FICHIER SUIT À LA FERMETURE : c'est la
  // même liste qui s'écrit.
  void Forget();

  // Les correspondants, le plus récent en tête, pour les écrire.
  [[nodiscard]] std::vector<WhisperPeer> saved() const {
    return std::vector<WhisperPeer>(peers_.begin(), peers_.end());
  }

  // LES VUES QUE LA BARRE REÇOIT. ⚠ REFAITES À CHAQUE APPEL, sur une file qui
  // ne bouge qu'à un échange : la liste de vues, si on la gardait, périmerait
  // au premier nom ajouté.
  [[nodiscard]] std::span<const ro::ui::ChatRecentWhisper> Views();

 private:
  std::deque<WhisperPeer> peers_;
  std::vector<ro::ui::ChatRecentWhisper> views_;
  // Le rang de la dernière ligne du fil déjà lue.
  std::uint64_t heard_through_ = 0;
};

}  // namespace moonclient
