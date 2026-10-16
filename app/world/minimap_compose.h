#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/guild.h"
#include "ro/sim/map_guidance.h"
#include "ro/sim/map_viewpoints.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/store_search.h"
#include "ro/sim/world.h"
#include "ro/ui/minimap_window.h"
#include "ro/vfs/town_info.h"

#include "world/navi_file.h"

namespace moonclient {

// Une image du jeu, par son chemin sous la racine des skins, en UTF-8.
using MinimapImageAt = std::function<ro::ui::SkinImage(std::string_view)>;

// Ce que la minimap lit, rassemblé : tout vient d'ailleurs et rien n'est à elle.
struct MinimapSources {
  const ro::sim::World* world = nullptr;
  std::uint32_t now_ms = 0;
  // La taille de la carte en cellules, zéro tant qu'elle n'est pas chargée.
  int cells_width = 0;
  int cells_height = 0;
  std::string_view display_name;
  // L'heure du serveur, « HH:MM », vide quand elle ne se montre pas. RECOPIÉE :
  // le texte peut mourir avec l'appel.
  std::string_view server_clock;
  const ro::vfs::TownInfo* towns = nullptr;
  const ro::formats::NaviData* navi = nullptr;
  const ro::sim::PartyRoster* party = nullptr;
  const ro::sim::GuildBook* guild = nullptr;
  const ro::sim::MapViewpoints* viewpoints = nullptr;
  const ro::sim::NaviGuide* guide = nullptr;
  // La recherche d'échoppes, pour le marqueur de l'échoppe désignée.
  const ro::sim::StoreSearch* store_search = nullptr;
  std::span<const NaviMemo> memos;
  // Les tombes des MVP connus du groupe sur cette carte, déjà filtrées par le
  // carnet de chasse ; elles passent telles quelles dans la vue.
  std::span<const ro::ui::MinimapTomb> tombs;
  // Le boss qu'un Convex Mirror révèle, quand le serveur en donne la case.
  std::optional<ro::sim::BossCell> boss;
  // LES COÉQUIPIERS DU CHAMP DE BATAILLE, que le monde tient : montrés quand le
  // réglage le veut, tous dans la couleur de notre camp.
  bool battle_mates_shown = false;
  ImU32 battle_mate_color = IM_COL32(255, 255, 255, 255);
  MinimapImageAt image_at;
};

// La vue de la minimap, composée à chaque image dans des tampons qu'elle garde :
// les `span` de la vue pointent dedans, et restent valables jusqu'à la
// composition suivante.
class MinimapComposer {
 public:
  [[nodiscard]] const ro::ui::MinimapView& Compose(const MinimapSources& from);

 private:
  void ComposeTown(const MinimapSources& from);
  void ComposeQuests(const MinimapSources& from);
  void ComposeMates(const MinimapSources& from);
  void ComposeBattleMates(const MinimapSources& from);
  void ComposeRoute(const MinimapSources& from);

  ro::ui::MinimapView view_;
  std::string map_name_;
  std::string server_clock_;
  std::vector<std::string> labels_;
  std::vector<ro::ui::MinimapTownIcon> town_;
  std::vector<ro::ui::MinimapTownIcon> portals_;
  std::vector<ro::ui::MinimapQuest> quests_;
  std::vector<ro::ui::MinimapMate> party_;
  std::vector<ro::ui::MinimapMate> guild_;
  // Les noms des coéquipiers, recopiés : la vue les désigne jusqu'au dessin.
  std::vector<std::string> battle_mate_names_;
  std::vector<ro::ui::MinimapBattleMate> battle_mates_;
  std::vector<ro::ui::MinimapViewpoint> viewpoints_;
  std::vector<ro::ui::MinimapMemo> memos_;
  std::vector<ro::ui::MinimapCell> route_;
};

// L'ANGLE DE VISÉE du client pour une direction de boussole : sud zéro, est
// quatre-vingt-dix, nord cent quatre-vingts — celui que la flèche de la
// minimap attend.
[[nodiscard]] float MinimapFacingDegrees(std::uint8_t compass_direction) noexcept;

// La couleur du n-ième membre : une palette fixe, pour qu'un membre garde sa
// couleur d'une image à l'autre.
[[nodiscard]] ImU32 MinimapMateColor(std::size_t rank) noexcept;

}  // namespace moonclient
