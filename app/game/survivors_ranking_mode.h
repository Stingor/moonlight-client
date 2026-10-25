#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/survivors_ranking.h"
#include "ro/net/world_update.h"
#include "ro/sim/survivors_rules.h"

#include "game/survivors_book_mode.h"
#include "survivors/survivors_ranking_session.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LE CLASSEMENT SURVIVORS DANS LA PARTIE ──────────────────────────────────
//
// Il possède la session de classement et la nourrit du vrai flux : la poignée
// de main, les entrées en carte, le personnage et le serveur, les quatre ZC du
// service. Il émet ce que la session rend, porte au carnet les résultats en
// attente, et dit au journal du client ce que la session a à dire.
//
// 🔴 MUET TANT QUE LE SERVEUR N'A PAS ANNONCÉ LE SERVICE pour la version de
// règles du client : aucun octet de `0x0F4C` à `0x0F52` ne part avant `ZC
// 0x0F53`, et rien ne part hors d'une poignée de main vérifiée.
//
// 🔴 LE BIT `kUiCapSurvivors` N'ENTRE DANS `0x0F24` QUE SI LA TABLE DES
// LONGUEURS CONNAÎT LES QUATRE ZC DU SERVICE, vérifié une fois, à la
// construction : un ZC inconnu de la table arrête la liaison.

// La longueur qu'une table donne à un opcode, comme `ro::proto::PacketLength`.
using PacketLengthOf = std::function<std::optional<std::int16_t>(std::uint16_t)>;

// Vrai quand la table connaît chacun des ZC du service.
[[nodiscard]] bool SurvivorsRepliesKnown(const PacketLengthOf& length_of);

// La clé de serveur d'un résultat en attente : l'adresse du serveur de
// connexion choisi, `hôte:port`, ou un nom fixe quand elle manque.
[[nodiscard]] std::string SurvivorsServerKeyOf(const std::string& login_address);

// CE QUE LA PARTIE DONNE AU CLASSEMENT, à chaque image.
struct SurvivorsRankingFacts {
  bool verified = false;          // la poignée de main d'intégrité est reconnue
  std::uint32_t map_entries = 0;  // `ro::sim::World::map_entries`
  std::string server_key;
  std::uint32_t character_id = 0;
  std::string character_name;
  SurvivorsRankingNow now;
};

class SurvivorsRankingMode {
 public:
  // `length_of` vide : la table des longueurs du client.
  explicit SurvivorsRankingMode(std::uint16_t ruleset = ro::sim::kSurvivorsRulesetVersion,
                                const PacketLengthOf& length_of = {},
                                SurvivorsSaveFile file = SurvivorsSaveFile());

  // Le bit du service à ajouter au masque de `0x0F24`, ou 0.
  [[nodiscard]] std::uint32_t caps() const noexcept { return caps_; }

  // UNE TRAME REÇUE à l'instant monotone `received_at_ms`, celui de `Pump` :
  // un des quatre ZC du service se lit ici, ou se dit indéchiffrable ; toute
  // autre trame reste non traitée. Un jeton se juge à cet instant.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 std::int64_t received_at_ms);

  // À CHAQUE IMAGE, liaison ouverte ou non, après l'avis de chargement de la
  // carte : le carnet relu à la première, le personnage, la poignée de main,
  // l'entrée en carte, l'échéance du jeton, les trames émises, le carnet écrit
  // et le journal.
  void Pump(ro::net::Connection& link, const SurvivorsRankingFacts& facts,
            const std::filesystem::path& client_root);

  // LE DÉPART D'UNE PARTIE (Q2, Q3). Une partie classée ou un défi demande son
  // jeton ; une partie classée service fermé part libre. Le départ rendu est
  // celui à commencer tout de suite ; il peut attendre son jeton (`holding`).
  [[nodiscard]] SurvivorsBookMode::Begun Launch(SurvivorsBookMode& book,
                                                const ro::sim::SurvivorsRules& rules,
                                                const std::filesystem::path& client_root,
                                                std::uint64_t seed, std::uint16_t first_skill_id);
  // Vrai tant que le départ attend son jeton : l'écran de chargement le couvre.
  [[nodiscard]] bool holding() const noexcept {
    return session_.run_state() == SurvivorsRunRanking::kAwaiting;
  }
  // LE DÉPART RÉGLÉ, une fois : celui à recommencer quand la graine change —
  // la partie classée sur la graine du serveur, ou libre faute de jeton ; le
  // défi sur la semaine du serveur. Rien quand le départ tient.
  [[nodiscard]] std::optional<SurvivorsBookMode::Begun> Settle(
      SurvivorsBookMode& book, const ro::sim::SurvivorsRules& rules,
      const std::filesystem::path& client_root);

  // LA PARTIE FINIE ENTRE AU BILAN, ou se fige. Une partie classée finie
  // envoie ses composantes ; figée par la perte de liaison, ou abandonnée,
  // elle n'envoie rien, ni une partie libre ou à départ chargé.
  void FollowEnded(const SurvivorsStage& stage, bool frozen_by_link_loss);

  // LE TABLEAU MONDIAL DEMANDÉ pour l'arène et la classe des règles : vrai
  // quand la demande part à la prochaine image — service ouvert et cadence
  // passée.
  bool AskBoard(const ro::sim::SurvivorsRules& rules, ro::net::SurvivorsBoardKind board);
  // LA PAGE QUE LA FENÊTRE DU TABLEAU ATTEND encore, la cadence ayant retenu
  // sa demande ; rien quand aucune n'est due.
  [[nodiscard]] std::optional<ro::net::SurvivorsBoardKind> board_owed() const noexcept {
    return board_owed_;
  }
  void OweBoard(std::optional<ro::net::SurvivorsBoardKind> board) noexcept { board_owed_ = board; }

  // CE QUE LA SESSION A DIT depuis la dernière lecture, déjà écrit au journal :
  // `app` en fait les lignes de conversation qu'il faut.
  [[nodiscard]] std::vector<SurvivorsRankingNotice> TakeNotices() {
    return std::exchange(told_, {});
  }

  [[nodiscard]] const SurvivorsRankingSession& session() const noexcept { return session_; }
  // Les instants de la dernière image : l'âge des pages et la cadence s'y lisent.
  [[nodiscard]] const SurvivorsRankingNow& now() const noexcept { return now_; }

 private:
  // Le départ qui attend son jeton.
  struct PendingLaunch {
    SurvivorsBoardKind kind = SurvivorsBoardKind::kFree;
    std::uint64_t start_seed = 0;
    std::uint64_t local_seed = 0;
    std::uint16_t first_skill_id = 0;
  };

  void Write(const std::filesystem::path& client_root);

  SurvivorsRankingSession session_;
  SurvivorsSaveFile file_;
  std::uint16_t ruleset_;
  std::uint32_t caps_ = 0;
  std::optional<std::filesystem::path> loaded_root_;
  bool verified_ = false;
  std::optional<std::uint32_t> map_entries_;
  std::optional<std::string> server_key_;
  std::uint32_t character_id_ = 0;
  std::optional<PendingLaunch> launch_;
  SurvivorsRankingNow now_;
  std::optional<ro::net::SurvivorsBoardKind> board_owed_;
  std::vector<SurvivorsRankingNotice> told_;
};

}  // namespace moonclient
