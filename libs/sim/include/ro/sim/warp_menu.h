#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ro::sim {

// LA QUESTION QU'UNE COMPÉTENCE DE DÉPLACEMENT POSE AU JOUEUR : où aller.
//
// Teleport au niveau 2 et plus, Warp Portal et Odin's Recall ne partent pas au
// clic : le serveur envoie une LISTE de destinations et attend le choix. Tant
// qu'il ne l'a pas reçu, la compétence reste suspendue de son côté.
//
// 🔴 LES DESTINATIONS SONT GARDÉES TELLES QUE LE SERVEUR LES ÉCRIT —
// « prontera.gat », « Random.gat » —, parce que c'est ce nom-là, et lui seul,
// que la réponse doit lui rendre. Le nom lisible se calcule à l'affichage.
//
// ⚠ LE CLIENT AJOUTE LUI-MÊME UNE DERNIÈRE LIGNE, l'annulation : le serveur ne
// l'envoie pas. Elle ne vit pas ici, mais dans la fenêtre, qui la sait toujours
// présente.

// Une liste reçue, retenue telle quelle.
struct WarpOffer {
  // La compétence qui pose la question, à rendre avec la réponse : le serveur
  // ignore une réponse qui ne nomme pas celle qu'il attend.
  std::uint16_t skill_id = 0;
  // Les destinations non vides, dans l'ordre du serveur.
  std::vector<std::string> destinations;
  // Le rang de l'entrée sur carte pendant laquelle la liste est arrivée.
  std::uint32_t map_entry = 0;
};

class WarpMenu {
 public:
  // Une liste est arrivée : elle ouvre la question, ou REMPLACE celle en
  // cours, comme le client qui referme sa fenêtre et la recrée.
  void Offer(std::uint16_t skill_id, std::vector<std::string> destinations,
             std::uint32_t map_entry);

  // Le joueur a répondu, ou la question ne vaut plus.
  void Close() noexcept { open_ = false; }

  // UN CHANGEMENT DE CARTE EMPORTE LA QUESTION : le client détruit alors ses
  // fenêtres, et celle-ci avec, sans rien dire au serveur.
  void CloseIfMapChanged(std::uint32_t map_entry) noexcept {
    if (open_ && offer_.map_entry != map_entry) {
      open_ = false;
    }
  }

  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] const WarpOffer& offer() const noexcept { return offer_; }

  // LE RANG DE LA DERNIÈRE LISTE, qui ne recule jamais. ⚠ DEUX LISTES
  // IDENTIQUES DE SUITE SONT DEUX QUESTIONS, et c'est lui qui le dit : la
  // fenêtre y remet sa sélection sur la première ligne.
  [[nodiscard]] std::uint64_t offer_seq() const noexcept { return offer_seq_; }

 private:
  bool open_ = false;
  WarpOffer offer_;
  std::uint64_t offer_seq_ = 0;
};

}  // namespace ro::sim
