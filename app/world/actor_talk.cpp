#include "world/actor_talk.h"

#include <random>

#include "ro/core/log.h"
#include "ro/formats/talk_lines.h"
#include "ro/net/action.h"
#include "ro/render/actor_effect_table.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// LA COMMANDE QUE CES DEUX EFFETS ENVOIENT à l'interface.
//
// Sur la commande 0xA5, le client ouvre un fichier de répliques, en tire une au
// sort, et l'envoie au serveur comme une parole ordinaire, préfixée du nom de
// son personnage.
constexpr std::uint16_t kTalkCommand = 0xA5;

// Lequel des deux fichiers : le PARAMÈTRE de la commande vaut un pour la
// plaisanterie du barde, zéro pour le cri de la danseuse.
constexpr std::uint32_t kFrostJokeEffect = 295;

}  // namespace

const std::string& ActorTalk::Contents(ro::vfs::Vfs& vfs, std::string_view path) {
  for (std::size_t rank = 0; rank < files_.size(); ++rank) {
    if (files_[rank].first == path) {
      return files_[rank].second;
    }
  }
  // ⚠ LE CHEMIN EST CELUI DU CLIENT, en octets bruts : ces deux-là sont nommés
  // en ASCII, mais ils vivent parmi des voisins coréens et le VFS ne doit pas
  // les retraduire.
  std::string contents;
  if (auto bytes = vfs.ReadRaw(path); bytes) {
    contents.assign(bytes->begin(), bytes->end());
  } else {
    // Un fichier absent ne fait pas taire le client : il fait taire l'effet, et
    // le dit UNE FOIS. Une installation incomplète reste jouable.
    ro::core::LogWarning("répliques : fichier absent ({}) — ces PNJ resteront muets", path);
  }
  files_.emplace_back(std::string(path), std::move(contents));
  return files_.back().second;
}

void ActorTalk::Notice(const ro::sim::World& world, ro::vfs::Vfs& vfs, ro::net::Connection& link,
                       const TalkSwitches& spoken, std::uint32_t now_ms) {
  if (!link.is_open()) {
    return;
  }
  // Le tirage vient de l'horloge, comme celui des fichiers d'effet : deux
  // plaisanteries de suite ne donnent pas la même réplique.
  std::minstd_rand draw(now_ms);
  for (const auto& [id, actor] : world.actors()) {
    for (const ro::sim::PlayedEffect& effect : actor.effects) {
      const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(effect.id);
      if (change == nullptr || change->command != kTalkCommand) {
        continue;
      }
      // Le témoin porte l'INSTANT de la pose, et non le seul numéro d'effet : le
      // même effet rejoué plus tard fait parler de nouveau.
      const std::uint64_t witness_key =
          (static_cast<std::uint64_t>(id) << 32U) ^ static_cast<std::uint64_t>(effect.since_ms);
      if (said_.size() >= kTalkWitnessesKept) {
        said_.clear();
      }
      if (!said_.insert(witness_key).second) {
        continue;
      }
      const bool joke = effect.id == kFrostJokeEffect;
      if (joke ? !spoken.frost_joke : !spoken.scream) {
        continue;
      }
      const std::string& contents =
          Contents(vfs, joke ? ro::formats::kFrostJokeLinesPath : ro::formats::kScreamLinesPath);
      if (contents.empty()) {
        continue;
      }
      const auto count = static_cast<std::uint32_t>(joke ? kFrostJokeLines : kScreamLines);
      const std::string said =
          ro::formats::TalkLine(contents, static_cast<std::size_t>(draw() % count) + 1);
      if (said.empty()) {
        continue;
      }
      if (auto told = ro::net::RequestChat(link, world.player_name(), said); !told) {
        // ⚠ « NON ENCODÉE » ET NON « REFUSÉE » : `RequestChat` ne peut échouer qu'à
        // l'encodage et n'examine jamais le retour de `Send`. Le mot « refusée »
        // envoyait chercher du côté du serveur une faute purement locale.
        ro::core::LogWarning("réplique non encodée : {} — le personnage restera muet",
                             ro::core::Describe(told.error()));
      }
    }
  }
}

}  // namespace moonclient
