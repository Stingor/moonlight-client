#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/net/entity_props.h"
#include "ro/net/world_update.h"
#include "ro/ui/entity_inspector_window.h"

namespace moonclient {

// ── « PROPRIÉTÉS… » : L'INSPECTEUR D'ENTITÉ DU STAFF ─────────────────────────
//
// Une entité à la fois. L'ouvrir demande au serveur ce qu'il en sait
// (`ro/net/entity_props.h`) ; la fenêtre montre la réponse, et « Actualiser »
// redemande.
//
// 🔴 UNE RÉPONSE NE S'APPARIE QUE PAR SON IDENTIFIANT : celle qui arrive après un
// changement de cible décrit l'ANCIENNE entité, et la montrer sous la nouvelle
// serait un mensonge silencieux. Elle est jetée.
//
// 🔴 LA FENÊTRE SE FERME AU CHANGEMENT DE CARTE : un identifiant ne survit pas à
// la carte, et la nouvelle peut l'avoir donné à une autre entité.
//
// ⚠ UNE ATTENTE A UNE FIN. Le serveur ne répond qu'aux clients dont il a
// reconnu la poignée de main, et un serveur qui ne connaît pas le paquet ne
// répond jamais : passé `kEntityInspectorWaitMs`, la fenêtre le dit plutôt que
// d'attendre toujours.
inline constexpr std::uint32_t kEntityInspectorWaitMs = 5000;

class EntityInspector {
 public:
  // Inspecte l'entité `gid` : la fenêtre s'ouvre et la demande part à l'image.
  // `subject` est DÉJÀ TRADUIT ; `map_name` est la carte où elle a été désignée.
  void Open(ro::ui::EntityInspectorWindowState& state, std::uint32_t gid, std::string subject,
            std::string_view map_name, std::uint32_t now_ms);

  // La réponse du serveur, si la trame en est une : `kApplied` quand elle
  // décrit l'entité inspectée, `kIgnored` quand elle en décrit une autre.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame);
  void Note(const ro::net::EntityPropsReply& reply);

  // L'IMAGE : la fenêtre fermée oublie tout, une autre carte la ferme,
  // « Actualiser » redemande, l'attente expire, puis la vue se compose.
  void Advance(ro::ui::EntityInspectorWindowState& state,
               const ro::ui::EntityInspectorAnswer& answer, std::string_view map_name,
               std::uint32_t now_ms);

  // La demande à émettre, prise une seule fois.
  [[nodiscard]] std::optional<std::uint32_t> TakeRequest();

  // Émet la demande en attente, s'il y en a une.
  void SendRequest(ro::net::Connection& link);

  [[nodiscard]] const ro::ui::EntityInspectorView& view() const noexcept { return view_; }
  [[nodiscard]] std::uint32_t gid() const noexcept { return gid_; }

 private:
  void Ask(std::uint32_t now_ms);
  void Forget();

  // Zéro : rien n'est inspecté.
  std::uint32_t gid_ = 0;
  std::string map_name_;
  std::uint32_t asked_ms_ = 0;
  std::optional<std::uint32_t> pending_request_;
  ro::ui::EntityInspectorView view_;
};

}  // namespace moonclient
