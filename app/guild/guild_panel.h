#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/gl_texture.h"
#include "ro/render/item_icons.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/guild.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/style.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/guild_tab.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/vfs.h"

#include "guild/guild_emblems.h"

namespace moonclient {

// L'ONGLET GUILDE, VU DE LA PARTIE : ce qu'il montre, et ce qu'on fait de ce
// qu'il demande.
//
// ⚠ IL CROISE CE QUE RIEN NE RELIE : le carnet de guilde, le personnage joué,
// les tables de l'installation, les icônes, les emblèmes et le dossier
// `emblem\` du disque. `ui` ne connaît aucune de ces sources.

// CE QUE LA VUE EMPRUNTE À LA PARTIE. ⚠ TOUT PEUT MANQUER : une installation
// incomplète donne un onglet plus pauvre, jamais un onglet qui plante.
struct GuildPanelPieces {
  ro::vfs::Vfs* vfs = nullptr;
  const ro::vfs::JobNames* jobs = nullptr;
  const ro::vfs::ItemTable* items = nullptr;
  ro::render::ItemIcons* item_icons = nullptr;
  const ro::sim::SkillTree* skills = nullptr;
  ro::render::SkillIcons* skill_icons = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
  // Le dossier de l'installation, où vit `emblem\`.
  std::filesystem::path client_root;
};

// CE QUE L'ONGLET DEMANDE AILLEURS QUE DANS LA GUILDE.
struct GuildElsewhere {
  // Le nom à poser dans la case du destinataire de la conversation.
  std::string whisper_to;
  // Le correspondant d'une conversation 1:1 à ouvrir.
  std::string whisper_window;
  // Le destinataire d'un courrier à écrire.
  std::string mail_to;
  // La compétence dont on veut la description.
  std::uint16_t describe_skill = 0;
  // Le membre à inviter dans notre carnet de chasse MVP, ou vide.
  std::string mvp_invite;
};

// LE JUGE DE « INVITER DANS MON CARNET MVP » : la raison, déjà traduite, qui
// grise l'entrée pour ce nom, ou nul si elle est offerte. Le booléen dit que le
// membre est un personnage de notre propre compte.
using MvpInviteJudge = std::function<const char*(std::string_view, bool)>;

// QUI JOUE : le personnage, et son compte sur la carte.
struct GuildPlayer {
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
};

class GuildPanel {
 public:
  explicit GuildPanel(GuildPanelPieces pieces);

  // Refait la vue. À appeler une fois par image, avant le dessin. ⚠ `shown`
  // dit si l'onglet est à l'écran : les têtes des membres ne se composent que
  // pour lui.
  void Refresh(const ro::sim::GuildBook& book, const ro::sim::World& world,
               ro::ui::GuildTabState& state, const GuildEmblems& emblems, GuildPlayer player,
               bool shown, std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::GuildView& view() const noexcept { return view_; }
  // L'invitation qui attend sa réponse : le nom de la guilde, ou vide.
  [[nodiscard]] std::string_view invitation() const noexcept { return invitation_; }

  // Posé par la partie à chaque image, AVANT `Refresh` : le carnet de chasse
  // n'est pas à la guilde, et c'est lui qui sait qui peut y être invité. Sans
  // juge, l'entrée de chaque membre est offerte.
  void JudgesMvpInvites(MvpInviteJudge judge) { mvp_judge_ = std::move(judge); }
  // Posé de même : carnet éteint, l'entrée n'est dans le menu d'aucun membre.
  void WithdrawsMvpInvites(bool withdrawn) noexcept { mvp_withdrawn_ = withdrawn; }

  // 🔴 LA FENÊTRE SE REDEMANDE À L'OUVERTURE DE L'ONGLET, PUIS TOUTES LES
  // TRENTE SECONDES tant qu'il est à l'écran : le serveur ne pousse ni les
  // membres, ni les postes, ni les compétences.
  void RequestIfDue(ro::net::Connection& link, bool shown, std::uint32_t now_ms);

  // Fait ce que l'onglet a demandé.
  [[nodiscard]] GuildElsewhere Apply(const ro::ui::GuildAction& action,
                                     ro::ui::GuildTabState& state, const ro::sim::GuildBook& book,
                                     const ro::sim::World& world, GuildEmblems& emblems,
                                     ro::net::Connection& link, GuildPlayer player);

  // Ce que le service d'emblèmes a répondu, et les réponses du serveur à nos
  // invitations : deux comptes rendus qui arrivent après coup.
  void Settle(ro::ui::GuildTabState& state, const ro::sim::GuildBook& book, GuildEmblems& emblems,
              ro::net::Connection& link);

 private:
  void RefreshMembers(const ro::sim::GuildBook& book, const ro::sim::StyleBook& styles,
                      GuildPlayer player, bool shown);
  void RefreshRights(const ro::sim::GuildBook& book, GuildPlayer player);
  void RefreshSkills(const ro::sim::GuildBook& book, const ro::sim::World& world,
                     GuildPlayer player, std::uint32_t now_ms);
  void RefreshEmblemEditor(const ro::sim::World& world, ro::ui::GuildTabState& state,
                           std::uint32_t now_ms);
  // L'arbre des compétences de guilde, lu une fois dans l'installation.
  void EnsureGuildTree();

  // ── L'ÉDITEUR D'EMBLÈME ─────────────────────────────────────────────────
  void ScanEmblemFolder();
  void OpenEmblemEditor(ro::ui::GuildTabState& state, const GuildEmblems& emblems,
                        std::int32_t guild_id, bool from_current);
  [[nodiscard]] bool LoadCurrentEmblem(ro::ui::GuildTabState& state, const GuildEmblems& emblems,
                                       std::int32_t guild_id);
  void UploadFile(ro::ui::GuildTabState& state, GuildEmblems& emblems, std::int32_t guild_id,
                  int file);
  void UploadCanvas(ro::ui::GuildTabState& state, GuildEmblems& emblems, std::int32_t guild_id,
                    const std::string& file_name);
  void SaveCanvas(ro::ui::GuildTabState& state, const std::string& file_name);
  void LoadFileIntoCanvas(ro::ui::GuildTabState& state, int file);
  void ImportItemIcon(ro::ui::GuildTabState& state, std::uint32_t item_id);
  [[nodiscard]] bool WriteEmblemFile(const std::string& file_name,
                                     const std::vector<std::uint8_t>& bmp);
  [[nodiscard]] std::filesystem::path EmblemFolder() const;

  GuildPanelPieces pieces_;
  MvpInviteJudge mvp_judge_;
  bool mvp_withdrawn_ = false;
  ro::ui::GuildView view_;
  std::optional<ro::render::ActorThumbnails> thumbnails_;

  // Ce que les vues POINTENT, gardé d'une image à l'autre.
  std::vector<ro::ui::GuildMemberView> members_;
  std::vector<std::string> member_seen_;
  std::array<ro::ui::GuildPositionView, ro::ui::kGuildTabPositions> positions_ = {};
  std::vector<ro::ui::GuildRelationView> relations_;
  std::vector<ro::ui::GuildBanView> bans_;
  std::string creation_answer_;
  std::string my_position_;
  std::string invitation_;

  // LES COMPÉTENCES DE GUILDE que l'arbre de guilde du serveur connaît, rangées
  // par palier de prérequis. ⚠ VIDE QUAND IL MANQUE : l'onglet se limite alors à
  // ce que le serveur envoie — ni niveau maximal, ni verrouillées.
  struct GuildTreeNode {
    std::uint16_t id = 0;
    std::uint16_t max_level = 0;
    std::string idname;
    std::vector<ro::sim::SkillRequirement> needs;
    int depth = 0;
  };
  std::vector<GuildTreeNode> guild_nodes_;
  bool guild_tree_tried_ = false;
  std::vector<ro::ui::GuildSkillView> skills_;
  std::vector<std::vector<ro::ui::SkillNeedView>> skill_needs_;
  std::vector<std::string> skill_requirements_;

  // L'éditeur d'emblème : le dossier, ses aperçus, la galerie du sac.
  struct EmblemFile {
    std::string name;
    std::vector<std::uint8_t> bytes;
    std::string why;
    bool usable = false;
    int transparency = 0;
  };
  std::vector<EmblemFile> files_;
  std::vector<ro::ui::EmblemFileView> file_views_;
  std::map<std::string, ro::render::GlTexture> previews_;
  std::string folder_shown_;
  std::vector<std::uint32_t> gallery_ids_;
  std::vector<ro::ui::EmblemItemView> gallery_;
  std::uint32_t gallery_scanned_ms_ = 0;
  bool gallery_scanned_ = false;
  // Le BMP du dessin n'est refait que quand le dessin a bougé : la boîte se
  // redessine à chaque image.
  int canvas_revision_ = -1;
  std::size_t canvas_bytes_ = 0;
  int canvas_transparency_ = 0;

  // La fenêtre à redemander : zéro veut dire « tout de suite ».
  std::uint32_t requested_ms_ = 0;
  bool request_due_ = true;
  std::int32_t seen_guild_id_ = 0;
  // Les réponses déjà vues : une réponse ne se montre qu'à la demande qui l'a
  // provoquée.
  std::uint64_t creation_seq_seen_ = 0;
  std::uint64_t invite_answer_seq_seen_ = 0;
};

}  // namespace moonclient
