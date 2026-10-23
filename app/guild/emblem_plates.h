#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "ro/formats/image.h"
#include "ro/sim/actor.h"
#include "ro/sim/guild.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// LES EMBLÈMES QUE LES ÉTIQUETTES MONTRENT, décodés, par guilde et par version.
//
// 🔴 UNE IMAGE RANGÉE NE BOUGE NI NE S'EFFACE DE TOUTE LA PARTIE. La couche
// d'écran reconnaît une image à son ADRESSE pour ne la téléverser qu'une fois :
// une image remplacée qui retomberait à la même adresse se montrerait avec la
// texture de l'ancienne. Une nouvelle version est donc une nouvelle entrée, et
// une ancienne ne se montre plus parce qu'on ne la demande plus.
//
// ⚠ ELLE NE TÉLÉCHARGE RIEN : elle note ce que les étiquettes réclament, et
// `GuildEmblems` va le chercher — sur le fil, sur disque, au service web.
class EmblemPlates {
 public:
  // Une guilde et une version d'emblème.
  using Key = std::pair<std::int32_t, std::int32_t>;

  // L'image de cette guilde à cette version, ou nul — et l'absence se note
  // pour être cherchée. ⚠ UNE VERSION NULLE NE SE DEMANDE PAS : la guilde n'a
  // pas d'emblème. Un emblème illisible non plus : il le resterait.
  [[nodiscard]] const ro::formats::Image* Demand(std::int32_t guild_id, std::int32_t version);

  [[nodiscard]] const ro::formats::Image* Find(std::int32_t guild_id, std::int32_t version) const;
  // Rangé ou refusé : il n'y a plus rien à chercher.
  [[nodiscard]] bool Settled(std::int32_t guild_id, std::int32_t version) const;

  // Ce qui a été réclamé depuis la dernière fois, sans doublon. ⚠ SE PREND ET
  // S'EFFACE.
  [[nodiscard]] std::vector<Key> TakeWanted();

  // Range une image. ⚠ LA PREMIÈRE SEULE : une clé déjà tenue garde la sienne,
  // pour que son adresse ne change jamais.
  void Store(std::int32_t guild_id, std::int32_t version, ro::formats::Image image);
  // Un emblème arrivé illisible ne se redemande plus de la partie.
  void Reject(std::int32_t guild_id, std::int32_t version);

  [[nodiscard]] const std::map<Key, ro::formats::Image>& images() const noexcept { return images_; }

 private:
  std::map<Key, ro::formats::Image> images_;
  std::set<Key> rejected_;
  std::set<Key> wanted_;
};

// LA GUILDE QUE L'ÉTIQUETTE DE CET ACTEUR MONTRE, ou zéro. Voir `EmblemOnPlate`.
[[nodiscard]] std::int32_t PlateGuildIdOf(const ro::sim::Actor& actor, const ro::sim::World& world,
                                          const ro::sim::GuildBook& guild);

// L'EMBLÈME DE GUILDE QUE L'ÉTIQUETTE DE CET ACTEUR PORTE, ou nul.
//
// La guilde d'un autre est celle que son apparition déclare ; la mienne, celle
// du carnet — le serveur ne m'annonce jamais à moi-même. La version se lit au
// carnet, par guilde : l'acteur ne porte que sa guilde.
//
// ⚠ TOUTE UNITÉ QUI PORTE UNE GUILDE A SON EMBLÈME, pas seulement un joueur :
// le serveur donne celle de leur maître aux familiers et aux mercenaires, et
// la leur aux gardiens et aux drapeaux.
[[nodiscard]] const ro::formats::Image* EmblemOnPlate(const ro::sim::Actor& actor,
                                                      const ro::sim::World& world,
                                                      const ro::sim::GuildBook& guild,
                                                      EmblemPlates& plates);

}  // namespace moonclient
