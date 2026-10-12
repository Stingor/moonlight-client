#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/sim/pet_talk.h"

namespace ro::sim {

// ── CE QUE DISENT LES MONSTRES QUAND ILS FRAPPENT ──────────────────────────
//
// Le coup d'un AUTRE acteur que le nôtre — monstre, PNJ ou joueur, le client
// ne les distingue pas — lui fait peut-être dire une réplique : celle que la
// table de son ESPÈCE porte pour le coup, tirée au sort. Le coup du nôtre fait
// parler notre familier (voir `PetHuntWatch`).
//
// 🔴 LE COUP SE JUGE COMME LA CHASSE DU FAMILIER, sur l'acteur qui frappe et
// sur ce qu'il était AVANT ce coup : il faut qu'il se tienne au repos, que
// l'action soit un coup (ni ramasser, ni s'asseoir, ni se relever), et que la
// cible ne soit pas celle qu'il a mémorisée. Chaque acteur mémorise la cible
// de chacun de ses coups, de chacun de ses coups de compétence et de chacune
// de ses incantations qui vise quelqu'un. Il n'y a ni tirage ni silence : seul
// le changement de cible fait parler.
//
// 🔴 LA LIGNE EST « <nom de l'acteur> : <réplique> », dans la bulle comme dans
// la conversation, sans les mots interdits masqués. Un acteur sans nom ne dit
// rien. `/notalkmsg` et `/notalkmsg2` taisent la ligne, pas la bulle.

// L'espèce d'une classe : son nom de ressource, en minuscules ASCII.
[[nodiscard]] std::string MonsterTalkSpecies(std::string_view resource_name);

// La ligne que l'on voit.
[[nodiscard]] std::string MonsterLineShown(std::string_view speaker_name, std::string_view line);

// LA CIBLE QUE CHAQUE ACTEUR A MÉMORISÉE. Un acteur qui quitte la vue l'oublie
// — il revient comme un acteur neuf —, et l'entrée sur une carte les oublie
// toutes.
class StrikeMemories {
 public:
  // Un coup de cet acteur : vrai quand il le fait parler. La cible est
  // mémorisée dans tous les cas.
  [[nodiscard]] bool Strike(std::uint32_t actor_id, bool at_rest, std::uint32_t target_id,
                            std::uint8_t action);
  // Un coup de compétence, ou une incantation qui vise quelqu'un.
  void Remember(std::uint32_t actor_id, std::uint32_t target_id);
  void Forget(std::uint32_t actor_id) { watches_.erase(actor_id); }
  void Clear() { watches_.clear(); }

 private:
  std::unordered_map<std::uint32_t, PetHuntWatch> watches_;
};

}  // namespace ro::sim
