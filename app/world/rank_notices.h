#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/rank_notices.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace moonclient {

// ── LES CLASSEMENTS ET LES AVIS D'ÉTOILE, DANS LA CONVERSATION ──────────────
//
// `/blacksmith`, `/alchemist` et `/taekwon` demandent un classement ; le
// serveur répond par dix identifiants de personnage et leurs points. Les avis
// d'étoile disent la cible d'une mission Taekwon, et ce que Feeling et Hatred
// ont désigné.
//
// 🔴 LE CLASSEMENT ATTEND SES NOMS. Ils se demandent un par un, et le client
// d'origine les attend jusqu'à trois secondes, en regardant chaque seconde : tant
// qu'il en manque, il écrit qu'il cherche ; au bout du délai, il imprime avec ce
// qu'il a, un inconnu sous « sans nom ».

// Une ligne de la conversation, et sa couleur en `0xRRGGBB` — zéro pour celle
// de son genre.
struct RankNoticeLine {
  std::string text;
  std::uint32_t color = 0;
};

// Les lignes d'un classement : l'en-tête, dix rangs, un séparateur, ses propres
// points, un séparateur. `names` dit le nom de chaque rang, vide s'il manque.
[[nodiscard]] std::vector<RankNoticeLine> RankingLines(
    const ro::net::RankingList& list, const std::array<std::string, ro::net::kRankingSize>& names);

// La ligne des points gagnés, ou rien pour un classement que le client ne nomme
// pas.
[[nodiscard]] std::optional<RankNoticeLine> RankingPointsLine(const ro::net::RankingPoints& points);

// Ce qu'un avis d'étoile fait : une ligne, l'effet qu'il pose sur le
// personnage du joueur (zéro : aucun), et le son qu'il joue (vide : aucun).
struct StarNoticeOutcome {
  RankNoticeLine line;
  std::uint32_t effect_id = 0;
  std::string sound;
};

// Rien pour un code que le client ne connaît pas. `own_name` est le nom du
// personnage du joueur.
[[nodiscard]] std::optional<StarNoticeOutcome> StarNoticeOutcomeOf(
    const ro::net::StarNotice& notice, std::string_view own_name);

class RankNotices {
 public:
  // Lit une trame : un classement, des points, un avis, ou le nom d'un
  // personnage qu'un classement attend. ⚠ ÉMET : un classement demande les
  // noms qui lui manquent.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                                 ro::net::Connection& link, std::uint32_t now_ms);
  // Imprime le classement en attente quand ses noms sont là, ou que le délai
  // est passé.
  void Advance(ro::sim::World& world, std::uint32_t now_ms);
  // Les sons à jouer depuis la dernière fois, nommés dans le dossier des sons.
  [[nodiscard]] std::vector<std::string> TakeSounds();

 private:
  struct Pending {
    ro::net::RankingList list;
    std::uint32_t received_ms = 0;
    // Combien de secondes ont déjà été regardées.
    std::uint32_t looks_done = 0;
  };

  [[nodiscard]] bool AllNamesKnown(const ro::net::RankingList& list) const;
  void Print(ro::sim::World& world, const ro::net::RankingList& list, std::uint32_t now_ms) const;

  std::map<std::uint32_t, std::string> names_;
  std::optional<Pending> pending_;
  std::vector<std::string> sounds_;
};

}  // namespace moonclient
