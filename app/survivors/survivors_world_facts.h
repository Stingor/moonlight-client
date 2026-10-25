#pragma once

#include <cstdint>
#include <span>

#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "survivors/real_world_watch.h"

namespace moonclient {

// LE RELEVÉ DU MONDE RÉEL, composé depuis le `World` de la partie et ce que
// les pièces de partie savent à côté de lui. La garde d'entrée et la veille
// (`real_world_watch.h`) ne lisent que ce relevé : elles s'essaient sans jeu,
// et ce fichier s'essaie sur un `World` de test.

// CE QUE LE MONDE NE PORTE PAS : app le relève dans les pièces qui le tiennent
// (commerces, groupe, guilde, amis, tables de cartes, liaison).
struct WorldAside {
  bool on_town_map = false;    // la table des villes a une fiche pour la carte
  bool on_indoor_map = false;  // la carte est un intérieur
  bool trade_open = false;     // un échange proposé, demandé ou ouvert
  bool trade_offered = false;  // un échange proposé PAR UN AUTRE, sans réponse
  bool shop_open = false;      // un commerce de PNJ, une boutique, un troc
  bool loading = false;        // un changement de carte ou de serveur en cours
  bool party_invited = false;
  bool guild_invited = false;
  bool friend_invited = false;
  bool link_open = true;
};

// Les faits de la garde d'entrée : le personnage, les monstres hostiles
// vivants autour de lui, le dialogue et le stockage que le monde tient.
[[nodiscard]] EntryGuardFacts EntryFactsOf(const ro::sim::World& world, const WorldAside& aside);

// Le relevé de l'image pour la veille. `whispers` est le compte des messages
// privés reçus depuis l'entrée (`WhisperCounter`).
[[nodiscard]] RealWorldFacts RealFactsOf(const ro::sim::World& world, const WorldAside& aside,
                                         std::uint32_t whispers);

// LE COMPTE DES MESSAGES PRIVÉS REÇUS, ligne par ligne de la conversation :
// chaque ligne reçue se compte une fois, par son rang, même quand le tampon
// en a déjà jeté de plus anciennes.
class WhisperCounter {
 public:
  // Le point de départ : rien de ce qui est déjà dit ne compte.
  void Start(std::span<const ro::sim::ChatLine> lines) noexcept;
  // Compte les lignes reçues depuis l'appel précédent ; rend le total.
  std::uint32_t Follow(std::span<const ro::sim::ChatLine> lines) noexcept;
  [[nodiscard]] std::uint32_t received() const noexcept { return received_; }

 private:
  std::uint64_t seen_seq_ = 0;
  std::uint32_t received_ = 0;
};

}  // namespace moonclient
