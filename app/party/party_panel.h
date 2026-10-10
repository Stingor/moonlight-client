#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/game_images.h"
#include "ro/render/status_icons.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/member_statuses.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/party_grid.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/map_display_names.h"
#include "ro/vfs/status_table.h"

#include "character/status_view.h"
#include "party/party_compose.h"
#include "party/party_options.h"
#include "party/party_poll.h"

namespace moonclient {

// LA FENÊTRE GROUPE / AMIS ET LA GRILLE DE GROUPE, VUES DE LA PARTIE : ce
// qu'elles montrent, le PS qu'on demande pour elles, et ce que le serveur dit
// en retour dans la conversation.
//
// ⚠ ELLE CROISE CE QUE RIEN NE RELIE : la liste du groupe, celle des amis, le
// monde, les tables de l'installation, les icônes, les têtes et les états.

// CE QUE LA VUE EMPRUNTE À LA PARTIE. ⚠ TOUT PEUT MANQUER : une installation
// incomplète donne des lignes plus pauvres, jamais une fenêtre qui plante.
struct PartyPanelPieces {
  ro::render::GameImages* images = nullptr;
  const ro::vfs::JobNames* jobs = nullptr;
  const ro::vfs::MapDisplayNames* map_names = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
  const ro::vfs::StatusTable* status_table = nullptr;
  ro::render::StatusIcons* status_icons = nullptr;
};

// CE QUE LA PARTIE SAIT À CETTE IMAGE, en plus des deux listes et du monde.
struct PartyFrameFacts {
  std::uint32_t char_id = 0;
  // Ma carte en cellules, pour la mini-carte ; zéro quand elle n'est pas
  // chargée.
  int map_width_cells = 0;
  int map_height_cells = 0;
  std::uint32_t now_ms = 0;
};

// LE JUGE DE « INVITER DANS MON CARNET MVP » : la raison du refus, déjà
// traduite, ou nul. `same_account` : un autre personnage de mon compte.
using PartyMvpJudge = std::function<const char*(std::string_view, bool)>;

class PartyPanel final : private PartyLens {
 public:
  explicit PartyPanel(PartyPanelPieces pieces);

  // Refait les deux vues. À appeler une fois par image, avant le dessin.
  void Refresh(const ro::sim::PartyRoster& party, const ro::sim::FriendList& friends,
               const ro::sim::World& world, const PartyOptions& options,
               const ro::ui::PartyFriendWindowState& window, const PartyFrameFacts& facts);

  [[nodiscard]] const ro::ui::PartyFriendView& window() const noexcept { return views_.window(); }
  [[nodiscard]] const ro::ui::PartyGridView& grid() const noexcept { return views_.grid(); }

  // LES ÉTATS DES MEMBRES, que le fil nourrit (`ro::net::ApplyMemberStatusFrame`)
  // et que les deux surfaces montrent.
  [[nodiscard]] ro::sim::MemberStatuses& member_statuses() noexcept { return member_statuses_; }

  // LE PS DES MEMBRES, au rythme de `PartyVitalsPoll` : quand la grille le
  // montre, ou quand la fenêtre ouverte le montre à cette image. ET LEURS
  // ÉTATS, au rythme de `MemberStatusPoll`, dans les mêmes conditions.
  void Poll(ro::net::Connection& link, const ro::sim::PartyRoster& party,
            const PartyOptions& options, const ro::ui::PartyFriendWindowState& window,
            std::uint32_t own_account_id, std::uint32_t now_ms);

  // Le membre sous le curseur qu'un sort de soutien viserait, relevé à la
  // dernière image de la grille.
  void NoteCast(std::uint32_t account_id) noexcept { cast_account_id_ = account_id; }

  // ⚠ POSÉ PAR LA PARTIE AVANT `Refresh` : seul le carnet sait qui peut y
  // entrer. Sans juge, l'invitation est offerte à tous.
  void JudgesMvpInvites(PartyMvpJudge judge) { mvp_judge_ = std::move(judge); }
  // Posé de même : carnet éteint, l'entrée n'est dans le menu d'aucune ligne.
  void WithdrawsMvpInvites(bool withdrawn) noexcept { mvp_withdrawn_ = withdrawn; }
  [[nodiscard]] std::uint32_t cast_account_id() const noexcept { return cast_account_id_; }

 private:
  // ── CE QUE LA COMPOSITION DEMANDE ──────────────────────────────────────
  [[nodiscard]] bool ActorPresent(std::uint32_t account_id) const override;
  [[nodiscard]] ro::ui::SkinImage JobIcon(std::uint16_t job) override;
  [[nodiscard]] std::string_view JobName(std::uint16_t job) const override;
  [[nodiscard]] std::string_view MapName(std::string_view map_raw) const override;
  [[nodiscard]] PartyMinimap Minimap(std::string_view map_raw) override;
  [[nodiscard]] ro::ui::SkinImage Head(std::uint32_t account_id) override;
  [[nodiscard]] ro::ui::SkinImage Crown() override;
  [[nodiscard]] PartyStatuses Statuses(std::uint32_t account_id, bool is_me) override;
  [[nodiscard]] const char* MvpInviteRefusal(std::string_view name,
                                             bool same_account) const override;

  [[nodiscard]] ro::ui::SkinImage ImageAt(std::string_view path);
  // Oublie les états des comptes qui ne sont plus ni dans le groupe ni moi.
  void ForgetStatuses(const ro::sim::PartyRoster& party, std::uint32_t own_account_id);
  // Ne garde des états des membres que ce qui se montre : un membre en ligne
  // qu'un acteur représente, et des états non échus. Tout part quand la carte
  // change.
  void PurgeMemberStatuses(const ro::sim::PartyRoster& party, const ro::sim::World& world);

  PartyPanelPieces pieces_;
  PartyViews views_;
  PartyVitalsPoll poll_;
  ro::sim::MemberStatuses member_statuses_;
  MemberStatusPoll status_poll_;
  std::optional<std::uint32_t> seen_map_entries_;
  std::optional<ro::render::ActorThumbnails> thumbnails_;
  // Un tampon d'états par compte : les vues pointent dedans jusqu'au dessin.
  std::map<std::uint32_t, StatusView> statuses_;

  // Le contexte de l'image en cours, posé par `Refresh`.
  const ro::sim::World* world_ = nullptr;
  std::string own_map_raw_;
  PartyFrameFacts facts_;
  int preview_ = 0;

  std::uint32_t cast_account_id_ = 0;
  PartyMvpJudge mvp_judge_;
  bool mvp_withdrawn_ = false;
};

// CE QUE LE SERVEUR A RÉPONDU au groupe et aux amis, écrit dans la conversation.
//
// ⚠ LE CHANGEMENT DE RÈGLES DU GROUPE N'Y PASSE PAS : la fenêtre l'annonce
// elle-même, et deux lignes pour un seul changement n'apprendraient rien.
void SayPartyNotices(ro::sim::PartyRoster& party, ro::sim::FriendList& friends,
                     ro::sim::World& world, std::uint32_t now_ms);

// QUITTER LE GROUPE, selon `sim::PlanLeave`. Rend vrai quand il faut d'abord
// demander au joueur s'il dissout le groupe : personne n'est là pour le mener.
[[nodiscard]] bool LeaveParty(ro::net::Connection& link, ro::sim::PartyRoster& party,
                              std::uint32_t own_account_id);

}  // namespace moonclient
