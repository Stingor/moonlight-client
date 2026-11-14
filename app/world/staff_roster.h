#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/staff_roster.h"
#include "ro/net/world_update.h"
#include "ro/ui/staff_roster_window.h"
#include "ro/vfs/job_names.h"

namespace moonclient {

// ── « JOUEURS EN LIGNE » : CE QUE LE STAFF DEMANDE DU SERVEUR ENTIER ────────
//
// La fenêtre ouverte demande la liste (`ro/net/staff_roster.h`), puis la
// redemande à cadence fixe tant que sa case le veut. Fermée, ou sans le droit
// du staff, la pièce oublie tout et ne demande rien.

// Passé ce délai sans réponse, la fenêtre le dit plutôt que d'attendre
// toujours : un serveur qui ne connaît pas le paquet ne répond jamais.
inline constexpr std::uint32_t kStaffRosterWaitMs = 5000;
// L'actualisation automatique.
inline constexpr std::uint32_t kStaffRosterRefreshMs = 10000;
// Entre deux demandes, au-dessus de la cadence que le serveur accepte : une
// demande hors cadence resterait sans réponse.
inline constexpr std::uint32_t kStaffRosterLeastGapMs = 2500;

// La commande que la fenêtre remplace pour le staff, telle qu'elle part au
// serveur pour qui ne l'est pas.
inline constexpr std::string_view kStaffRosterCommand = "@users";

// `@users`, seule sur sa ligne et à la casse près.
[[nodiscard]] bool IsStaffRosterCommand(std::string_view line);

// Ce client, pour juger celui des autres.
struct OwnClient {
  // Négatif : jamais patché.
  std::int32_t patch_index = -1;
  std::string_view commit;
};

// Les lignes de la fenêtre. Un client est « en retard » sous le patch le plus
// récent connu du sien — parmi les joueurs en ligne, et le nôtre pour
// moonclient. `job_names` peut être nul : un job sans nom se lit encore par ses
// niveaux.
[[nodiscard]] std::vector<ro::ui::StaffRosterRow> StaffRosterRowsOf(
    std::span<const ro::net::StaffRosterPlayer> players, const ro::vfs::JobNames* job_names,
    const OwnClient& own);

// Ce que la pièce lit de la partie à chaque image.
struct StaffRosterContext {
  // Le droit du staff, liaison tenue.
  bool allowed = false;
  const ro::vfs::JobNames* job_names = nullptr;
  OwnClient own;
};

class StaffRoster {
 public:
  // `@users`, ou le bouton de l'établi : la fenêtre s'ouvrira à l'image. Rend
  // faux sans le droit du staff, et rien ne s'ouvre.
  [[nodiscard]] bool Ask();

  // Une trame de la réponse, si c'en est une : `kApplied` quand elle termine la
  // réponse attendue.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, std::uint32_t now_ms);
  void Note(const ro::net::StaffRosterAnswer& answer, std::uint32_t now_ms);

  // L'image : ouvre sur demande, redemande à la cadence ou au bouton, laisse
  // l'attente expirer, et oublie tout sans le droit ou fenêtre fermée.
  void Advance(ro::ui::StaffRosterWindowState& state, const ro::ui::StaffRosterAnswer& answer,
               const StaffRosterContext& context, std::uint32_t now_ms);

  // Le numéro de la demande à émettre, pris une seule fois.
  [[nodiscard]] std::optional<std::uint16_t> TakeRequest();

  // Émet la demande en attente, s'il y en a une.
  void SendRequest(ro::net::Connection& link);

  [[nodiscard]] const ro::ui::StaffRosterView& view() const noexcept { return view_; }

  // Le joueur de la liste sous ce compte, ou rien.
  [[nodiscard]] const ro::ui::StaffRosterRow* Find(std::uint32_t account_id) const;

 private:
  void Request(std::uint32_t now_ms);
  void Forget();

  bool allowed_ = false;
  const ro::vfs::JobNames* job_names_ = nullptr;
  std::int32_t own_patch_index_ = -1;
  std::string own_commit_;
  // `Ask` a été entendu, et l'image ouvrira la fenêtre.
  bool ask_heard_ = false;
  std::uint16_t last_request_id_ = 0;
  std::optional<std::uint16_t> pending_request_;
  ro::net::StaffRosterAssembly assembly_;
  // L'instant de la dernière demande ; sans valeur tant qu'aucune n'est partie.
  std::optional<std::uint32_t> asked_ms_;
  // Celui de la dernière réponse entière ; sans valeur depuis l'ouverture.
  std::optional<std::uint32_t> answered_ms_;
  ro::ui::StaffRosterView view_;
};

}  // namespace moonclient
