#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/talk_lines.h"
#include "ro/vfs/vfs.h"

// CE QUE LA PLAISANTERIE ET LE CRI FONT DIRE AU PERSONNAGE.
//
// Deux effets d'acteur ne dessinent rien et ne changent rien à l'apparence : ils
// font PARLER. `EF_TALK_FROSTJOKE` et `EF_TALK_SCREAM` — les compétences du
// barde et de la danseuse — tirent une réplique au sort dans une table du jeu
// et l'envoient au serveur comme une parole ordinaire.
//
// ⚠ C'EST LE SEUL EFFET DU LOT QUI ÉMETTE VERS LE SERVEUR. Tous les autres
// changent ce qu'on voit ; celui-ci fait parler le personnage à notre place,
// exactement comme le client d'origine.
namespace moonclient {

// COMBIEN DE RÉPLIQUES LE CLIENT TIRE, dans l'un et dans l'autre.
//
// ⚠ CES DEUX NOMBRES SONT DANS L'EXÉCUTABLE, pas dans les tables, et ils n'en
// disent pas la taille : `scream_lines.yml` porte cent seize répliques et le
// client n'en tire que cent quatorze. Les compter à la lecture de la table
// changerait ce que le jeu peut dire.
inline constexpr std::size_t kFrostJokeLines = 0x75;
inline constexpr std::size_t kScreamLines = 0x72;

// LES DEUX RÉPLIQUES QUE LE JOUEUR LAISSE PARTIR.
//
// 🔴 ÉTEINTES PAR DÉFAUT, parce que c'est ce que fait le client du joueur : il
// ne trouve aucun des deux fichiers, et la commande s'arrête là, sans rien
// envoyer. Ce que chacune coupe, c'est la ligne partie au serveur — donc la
// bulle au-dessus de notre tête et la ligne que TOUS voient au chat. Les
// répliques des AUTRES arrivent du serveur et ne passent pas par ici.
struct TalkSwitches {
  bool scream = false;
  bool frost_joke = false;
};

// Ce que le personnage a déjà dit, pour ne pas le redire à chaque image.
class ActorTalk {
 public:
  // Fait parler le personnage pour les effets nouvellement joués, si la
  // réplique correspondante est permise.
  //
  // ⚠ IL PARLE POUR L'EFFET JOUÉ SUR N'IMPORTE QUEL ACTEUR, et c'est le client
  // qui le veut : sa fonction ne regarde pas sur qui l'effet porte, et prend
  // toujours le nom de SON personnage. C'est ce qui fait la compétence — un
  // barde plaisante, et tous ceux qu'il touche répondent. Le tri est l'affaire
  // du serveur, qui n'envoie l'effet qu'à qui il vise.
  void Notice(const ro::sim::World& world, ro::vfs::Vfs& vfs, ro::net::Connection& link,
              const TalkSwitches& spoken, std::uint32_t now_ms);

 private:
  // La table lue une fois. Une table absente ou illisible y entre vide : on ne
  // la redemande pas à chaque réplique.
  [[nodiscard]] const ro::vfs::TalkLines& LinesOf(ro::vfs::Vfs& vfs, std::string_view path);

  std::vector<std::pair<std::string, ro::vfs::TalkLines>> files_;
  // Les couples (acteur, instant) déjà dits : le même effet rejoué plus tard
  // fait parler de nouveau, le même effet revu à l'image suivante non.
  //
  // ⚠ RIEN NE LES RETIRE UN À UN, et c'est voulu : savoir quand un témoin ne
  // sert plus demanderait de connaître la vie de l'effet, que ces deux-là n'ont
  // pas. Passé un plafond, on oublie TOUT — au pire une réplique se redit une
  // fois, ce qui est sans conséquence, et la mémoire est bornée.
  std::unordered_set<std::uint64_t> said_;
};

// Combien de répliques dites on retient avant de tout oublier.
inline constexpr std::size_t kTalkWitnessesKept = 256;

}  // namespace moonclient
