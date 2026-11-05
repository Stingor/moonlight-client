#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/survivors_coop.h"
#include "ro/net/ui_caps.h"
#include "ro/net/world_update.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_rules.h"

#include "game/survivors_ranking_mode.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LA COOP DU MODE SURVIVORS, côté partie ─────────────────────────────────
//
// Le salon du groupe, le départ que le serveur donne, et le transport des trames
// d'entrées entre l'étage et le fil. Voir `ro/net/survivors_coop.h` et le cahier
// `docs/survivors/lot-14-coop.md`.
//
// 🔴 MUETTE TANT QUE LE SERVEUR N'A RIEN ANNONCÉ : le bit n'entre au masque que si
// la table des longueurs connaît les quatre ZC, et rien ne part avant un `0x0F59`.

// L'APPARENCE D'UN JOUEUR, à la suite de sa part : un membre du groupe peut être
// sur une autre carte, où le client ne le voit pas.
[[nodiscard]] std::vector<std::uint8_t> EncodeCoopLook(const ro::render::ActorLook& look);
[[nodiscard]] std::optional<ro::render::ActorLook> DecodeCoopLook(
    std::span<const std::uint8_t> bytes);

// UNE PARTIE COOP QUI PART, lue de `0x0F5A`.
struct SurvivorsCoopDeparture {
  ro::sim::SurvivorsStart start;
  std::size_t local_player = 0;
  std::uint32_t input_delay_ticks = 0;
  std::array<ro::render::ActorLook, ro::sim::kMostHeroes> looks{};
};
// Rien si les parts se lisent mal, si les règles diffèrent, ou si ce client n'est
// pas de la partie.
[[nodiscard]] std::optional<SurvivorsCoopDeparture> DepartureOf(
    const ro::net::SurvivorsCoopStart& start, std::uint32_t own_char_id,
    const ro::sim::SurvivorsRules& rules);

// LE CHEMIN RAPIDE, par l'UDP de la voix : ce qu'il a apporté, et ce qu'on lui confie.
// Il ne fait que mieux prédire le présent ; le serveur de carte porte la partie.
struct SurvivorsCoopFastPath {
  std::function<std::vector<ro::net::SurvivorsCoopFrame>()> take;
  std::function<void(std::vector<std::uint8_t>)> post;
};

class SurvivorsCoopMode {
 public:
  // `length_of` vide : la table des longueurs du client.
  explicit SurvivorsCoopMode(const PacketLengthOf& length_of = {});

  // Le bit de la coop à ajouter au masque de `0x0F24`, ou 0.
  [[nodiscard]] std::uint32_t caps() const noexcept { return caps_; }
  // Les quatre ZC se lisent ici ; toute autre trame reste non traitée.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame);
  // La liaison est perdue : tout s'oublie.
  void Forget();

  // Le serveur tient la coop sur cette liaison.
  [[nodiscard]] bool announced() const noexcept { return announced_; }
  [[nodiscard]] const ro::net::SurvivorsCoopLobby& lobby() const noexcept { return lobby_; }
  // Le départ arrivé, une fois.
  [[nodiscard]] std::optional<ro::net::SurvivorsCoopStart> TakeStart();

  // Entrer au salon, s'y dire prêt avec sa part, le quitter. Sans annonce du
  // serveur, rien ne part.
  void Ask(ro::net::Connection& link, ro::net::SurvivorsCoopAction action,
           std::span<const std::uint8_t> part = {});

  // Ce joueur renonce au salon ou au départ qui l'attend, une fois.
  void Forsake(ro::net::Connection& link);

  // UNE IMAGE DE PARTIE COOP : ce que le fil a apporté entre dans l'étage, un joueur
  // parti passe au robot, et les trames de l'étage partent par les deux chemins. La
  // voix peut manquer : le serveur de carte porte tout.
  void Carry(SurvivorsStage& stage, ro::net::Connection& link,
             const SurvivorsCoopFastPath& fast_path);

  // Les noms des joueurs de la partie, dans l'ordre des places.
  [[nodiscard]] std::span<const std::string> player_names() const noexcept { return names_; }
  // Tient la liste des joueurs pendant la partie.
  void Roster(const ro::net::SurvivorsCoopStart& start, std::uint32_t own_char_id);
  // Le serveur a sorti ce joueur de la partie, une fois.
  [[nodiscard]] bool TakeExcluded() noexcept;

 private:
  [[nodiscard]] std::optional<std::size_t> PlayerOf(std::uint32_t char_id) const noexcept;

  std::uint32_t caps_ = 0;
  bool announced_ = false;
  ro::net::SurvivorsCoopLobby lobby_;
  std::optional<ro::net::SurvivorsCoopStart> start_;
  std::vector<ro::net::SurvivorsCoopFrame> frames_;
  std::vector<ro::net::SurvivorsCoopLeft> leaves_;
  std::vector<std::uint32_t> roster_;
  std::uint32_t own_char_id_ = 0;
  bool excluded_ = false;
  std::vector<std::string> names_;
};

}  // namespace moonclient
