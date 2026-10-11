#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/connection.h"
#include "ro/net/player_report.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"
#include "ro/ui/player_report_window.h"

namespace moonclient {

// ── « SIGNALER CE JOUEUR » : LA FENÊTRE, L'ENVOI ET L'ACCUSÉ ────────────────
//
// Le menu d'un autre joueur ouvre la fenêtre sur lui ; « Envoyer » fait partir
// le paquet du client d'origine (`ro/net/player_report.h`), que le serveur
// enregistre en base. Son accusé s'écrit dans la conversation, en jaune pâle
// comme chez le client d'origine ; un signalement enregistré fait aussi luire
// notre personnage de l'effet que celui-ci y sème.
//
// Un joueur à la fois : rouvrir sur un autre remplace le premier, motif et
// texte compris.

// La couleur des accusés, celle du client, en `0xRRGGBB`.
inline constexpr std::uint32_t kPlayerReportSaysColor = 0xFFFF64;
// L'effet semé sur notre personnage quand le signalement est enregistré.
inline constexpr std::uint32_t kPlayerReportRecordedEffect = 41;

// La phrase d'un verdict, déjà traduite et complétée du nom ; rien pour un
// numéro inconnu, que le client d'origine tait aussi.
[[nodiscard]] std::optional<std::string> PlayerReportLine(const ro::net::PlayerReportAck& ack);

class PlayerReportDesk {
 public:
  // Ouvre la fenêtre sur ce joueur.
  void Open(ro::ui::PlayerReportWindowState& state, std::uint32_t target_aid,
            const std::string& target_name);

  // L'IMAGE : un envoi demandé part ; la fenêtre fermée oublie le joueur.
  void Advance(const ro::ui::PlayerReportWindowState& state,
               const ro::ui::PlayerReportAnswer& answer, std::uint32_t reporter_aid,
               ro::net::Connection& link);

  // Le signalement que l'envoi ferait partir, pour les essais.
  [[nodiscard]] ro::net::PlayerReport ReportOf(const ro::ui::PlayerReportWindowState& state,
                                               std::uint32_t reporter_aid) const;

  // L'accusé, s'il s'agit de lui : la phrase dans la conversation, l'effet sur
  // notre personnage.
  [[nodiscard]] static ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                        ro::sim::World& world,
                                                        std::uint32_t now_ms);
  static void Note(const ro::net::PlayerReportAck& ack, ro::sim::World& world,
                   std::uint32_t now_ms);

  [[nodiscard]] std::uint32_t target_aid() const noexcept { return target_aid_; }

 private:
  // Zéro : aucun joueur n'est visé.
  std::uint32_t target_aid_ = 0;
};

}  // namespace moonclient
