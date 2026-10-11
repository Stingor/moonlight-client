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

#include "world/taekwon_mission.h"

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
  // est passé ; et tient le compte de la mission Taekwon.
  void Advance(ro::sim::World& world, std::uint32_t now_ms);
  [[nodiscard]] const TaekwonMission& mission() const noexcept { return mission_; }
  // Les sons à jouer depuis la dernière fois, nommés dans le dossier des sons.
  [[nodiscard]] std::vector<std::string> TakeSounds();

  // LA PROCHAINE RÉPONSE DE CE CLASSEMENT NE S'ÉCRIT PAS dans la conversation :
  // elle a été demandée par un onglet du tableau, qui la montre. Les commandes
  // écrivent toujours.
  //
  // ⚠ L'ATTENTE A UNE FIN : une demande restée sans réponse ne doit pas faire
  // taire, des minutes plus tard, la réponse à une commande.
  void ExpectQuiet(ro::net::RankKind kind, std::uint32_t now_ms) {
    quiet_ = QuietRequest{.kind = kind, .asked_ms = now_ms};
  }

  // LE DERNIER CLASSEMENT REÇU, qu'il attende encore ses noms ou non ; rien
  // avant le premier.
  [[nodiscard]] const std::optional<ro::net::RankingList>& latest() const noexcept {
    return latest_;
  }
  // Grandit à chaque classement reçu.
  [[nodiscard]] std::uint32_t list_serial() const noexcept { return list_serial_; }
  // Le dernier classement attend encore des noms, et le délai n'est pas passé.
  [[nodiscard]] bool waiting_names() const noexcept { return pending_.has_value(); }
  // Le nom de chaque rang ; vide quand il n'est pas (encore) connu.
  [[nodiscard]] std::array<std::string, ro::net::kRankingSize> NamesOf(
      const ro::net::RankingList& list) const;

 private:
  struct Pending {
    ro::net::RankingList list;
    std::uint32_t received_ms = 0;
    // Combien de secondes ont déjà été regardées.
    std::uint32_t looks_done = 0;
    // Demandé par le tableau : rien ne s'écrit dans la conversation.
    bool quiet = false;
  };

  [[nodiscard]] bool AllNamesKnown(const ro::net::RankingList& list) const;
  void Print(ro::sim::World& world, const ro::net::RankingList& list, std::uint32_t now_ms) const;

  std::map<std::uint32_t, std::string> names_;
  std::optional<Pending> pending_;
  std::optional<ro::net::RankingList> latest_;
  std::uint32_t list_serial_ = 0;
  struct QuietRequest {
    ro::net::RankKind kind = ro::net::RankKind::kBlacksmith;
    std::uint32_t asked_ms = 0;
  };
  std::optional<QuietRequest> quiet_;
  std::vector<std::string> sounds_;
  TaekwonMission mission_;
};

}  // namespace moonclient
