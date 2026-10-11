#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/chat_blocks.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/ui/chat_block_log_window.h"

namespace moonclient {

// ── « JOURNAL DES BLOCAGES DE CHAT » : CE QUE LE STAFF DEMANDE D'UN JOUEUR ──
//
// Un joueur à la fois. L'ouvrir demande au serveur les `@mute` et `@unmute`
// qui l'ont visé (`ro/net/chat_blocks.h`) ; la fenêtre les montre en liste
// datée, et « Actualiser » redemande.
//
// 🔴 UNE RÉPONSE NE S'APPARIE QUE PAR SON AID : celle qui arrive après un
// changement de joueur décrit l'ANCIEN, et la montrer sous le nouveau serait un
// mensonge silencieux. Elle est jetée.
//
// L'AID survit au changement de carte : la fenêtre reste ouverte.
//
// ⚠ UNE ATTENTE A UNE FIN. Le serveur ne répond qu'aux clients dont il a
// reconnu la poignée de main, et un serveur qui ne connaît pas le paquet ne
// répond jamais : passé `kChatBlockLogWaitMs`, la fenêtre le dit plutôt que
// d'attendre toujours.
inline constexpr std::uint32_t kChatBlockLogWaitMs = 5000;

// Une date du journal, à l'heure de ce poste ; vide quand elle ne se convertit
// pas.
[[nodiscard]] std::string ChatBlockDateText(std::uint32_t date_s);

// Une ligne de la fenêtre, ses cellules écrites et traduites.
[[nodiscard]] ro::ui::ChatBlockLogRow ChatBlockRowOf(const ro::net::ChatBlock& block);

class ChatBlockLog {
 public:
  // Ouvre le journal du joueur `aid` : la fenêtre s'ouvre et la demande part à
  // l'image. `subject` est DÉJÀ TRADUIT.
  void Open(ro::ui::ChatBlockLogWindowState& state, std::uint32_t aid, std::string subject,
            std::uint32_t now_ms);

  // La réponse du serveur, si la trame en est une : `kApplied` quand elle
  // décrit le joueur demandé, `kIgnored` quand elle en décrit un autre.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame);
  void Note(const ro::net::ChatBlocksReply& reply);

  // L'IMAGE : la fenêtre fermée oublie tout, « Actualiser » redemande,
  // l'attente expire.
  void Advance(ro::ui::ChatBlockLogWindowState& state, const ro::ui::ChatBlockLogAnswer& answer,
               std::uint32_t now_ms);

  // La demande à émettre, prise une seule fois.
  [[nodiscard]] std::optional<std::uint32_t> TakeRequest();

  // Émet la demande en attente, s'il y en a une.
  void SendRequest(ro::net::Connection& link);

  [[nodiscard]] const ro::ui::ChatBlockLogView& view() const noexcept { return view_; }
  [[nodiscard]] std::uint32_t aid() const noexcept { return aid_; }

 private:
  void Ask(std::uint32_t now_ms);
  void Forget();

  // Zéro : aucun joueur n'est consulté.
  std::uint32_t aid_ = 0;
  std::uint32_t asked_ms_ = 0;
  std::optional<std::uint32_t> pending_request_;
  ro::ui::ChatBlockLogView view_;
};

}  // namespace moonclient
