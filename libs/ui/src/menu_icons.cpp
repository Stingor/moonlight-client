#include "ro/ui/menu_icons.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"

#include "achievement_paint.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

using enum MenuCommand;

// LE CATALOGUE, dans l'ordre de la grille du client. CATALOGUE : on n'y cherche
// qu'une entrée à la fois.
//
// ⚠ LES HUIT ICÔNES MASQUÉES sont celles que le client de ce serveur ne montre
// pas : le replay, les astuces, les réseaux sociaux, la présence, l'agence, la
// réputation, le guide et les probabilités. Le champ de bataille et la recherche
// de groupe, eux, sont montrés.
constexpr std::array<MenuIconEntry, kMenuCommandCount> kCatalogue = {{
    {kStatus, "status", true, true},
    {kEquipment, "equip", true, false},
    {kItem, "item", true, true},
    {kSkill, "skill", true, true},
    {kBooking, "booking", true, false},
    {kParty, "party", true, false},
    {kGuild, "guild", true, false},
    {kBattleground, "battle", true, false},
    {kQuest, "quest", true, true},
    {kWorldMap, "map", true, false},
    {kNavigation, "navigation", true, false},
    {kOption, "option", true, false},
    {kBank, "bank", true, false},
    {kReplay, "rec", false, false},
    {kMail, "mail", true, true},
    {kAchievement, "achievement", true, true},
    {kTip, "tip", false, false},
    {kCashShop, "shop", true, false},
    {kShortcut, "keyboard", true, false},
    {kSocialNetwork, "sns", false, false},
    {kAttendance, "attendance", false, false},
    {kAdventurerAgency, "adventurerAgency", false, false},
    {kReputation, "repute", false, false},
    {kAdventureGuide, "adventureguide", false, false},
    {kProbability, "probability", false, false},
    {kCardAlbum, "album", true, false},
    {kCraftAtlas, "atlas", true, false},
    {kMvpTracker, "tracker", true, true},
    {kPetWindow, "pet", true, false},
    {kRankingBoard, "ranking", true, false},
    {kMarket, "market", true, false},
    {kDatabase, "database", true, false},
    {kCashShopButton, "NC_CashShop", true, false, true},
}};

constexpr std::string_view kDoramStatusStem = "status_doram";
constexpr std::string_view kMenuIconFolder = "menu_icon\\bt_";
constexpr std::string_view kMenuIconExtension = ".bmp";
constexpr std::string_view kMenuButtonFolder = "basic_interface\\";
constexpr std::string_view kOurMenuIconPrefix = "menu_";
constexpr std::string_view kOurIconExtension = ".png";

// Le voile posé sur le carré de secours, et son filet.
constexpr ImU32 kFallbackFill = IM_COL32(40, 48, 72, 220);
constexpr ImU32 kFallbackEdge = IM_COL32(150, 170, 210, 255);
constexpr ImU32 kFallbackText = IM_COL32(235, 240, 255, 255);
constexpr float kFallbackRounding = 4.0F;
// Le survol, sur le carré de secours seulement : un bitmap du jeu n'en a pas.
constexpr ImU32 kFallbackHover = IM_COL32(255, 255, 255, 40);

// LA PASTILLE D'UN ÉTAT EN COURS, dans le coin haut-droit de l'icône : un rond
// rouge cerné de blanc, comme une notification. En pixels d'interface.
constexpr float kDotRadiusPixels = 4.0F;
constexpr float kDotRingPixels = 1.0F;
constexpr ImU32 kDotFill = IM_COL32(220, 50, 40, 255);
constexpr ImU32 kDotRing = IM_COL32(255, 255, 255, 255);

// Un octet de SUITE en UTF-8 s'écrit `10xxxxxx`.
constexpr unsigned kUtf8LeadMask = 0xC0U;
constexpr unsigned kUtf8Continuation = 0x80U;

// L'IMAGE À PEINDRE pour cet état, et si c'est le « nouveau ». Elle est vide
// quand l'installation n'a pas l'icône.
//
// ⚠ UN ÉTAT MANQUANT SE RABAT SUR LE PLUS PROCHE : un « nouveau » sans son
// enfoncé montre le « nouveau », pas le normal — le signalement ne doit pas
// s'éteindre sous le doigt.
struct ShownArt {
  const SkinImage* image = nullptr;
  bool news = false;
};

[[nodiscard]] ShownArt ArtOf(const MenuIconView& icon, bool held) {
  // Une bascule ouverte se montre enfoncée, doigt levé ou non.
  held = held || icon.shows_open;
  if (icon.news_lit && icon.news.usable()) {
    return {.image = held && icon.news_pressed.usable() ? &icon.news_pressed : &icon.news,
            .news = true};
  }
  return {.image = held && icon.pressed.usable() ? &icon.pressed : &icon.normal, .news = false};
}

// LE CARRÉ DE SECOURS : l'initiale du libellé, dans un cadre. Il tient la place
// d'un bitmap que l'installation n'a pas, sans décaler les voisines.
void DrawFallback(ImDrawList* into, ImVec2 top_left, float side, const char* label, bool hovered) {
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  into->AddRectFilled(top_left, bottom_right, kFallbackFill, kFallbackRounding * Scale());
  if (hovered) {
    into->AddRectFilled(top_left, bottom_right, kFallbackHover, kFallbackRounding * Scale());
  }
  into->AddRect(top_left, bottom_right, kFallbackEdge, kFallbackRounding * Scale());
  // ⚠ LA PREMIÈRE LETTRE ENTIÈRE, accent compris : en UTF-8 elle peut peser
  // plusieurs octets, et n'en garder qu'un la couperait en deux.
  const char* end = label;
  if (*end != '\0') {
    ++end;
    while ((static_cast<unsigned char>(*end) & kUtf8LeadMask) == kUtf8Continuation) {
      ++end;
    }
  }
  const ImVec2 text = ImGui::CalcTextSize(label, end);
  into->AddText(ImVec2(top_left.x + (side - text.x) * 0.5F, top_left.y + (side - text.y) * 0.5F),
                kFallbackText, label, end);
}

void DrawTooltip(const MenuIconView& icon, const SkinArt& art) {
  // Le libellé, puis la touche, puis — pour une icône en attente — ce qu'elle
  // fera au clic. ⚠ UNE SEULE BULLE : trois bulles superposées se liraient mal.
  char text[256];
  const char* label = icon.label.empty() ? MenuIconLabel(icon.command) : icon.label.c_str();
  int written = icon.shortcut.empty()
                    ? std::snprintf(text, sizeof(text), "%s", label)
                    : std::snprintf(text, sizeof(text), "%s (%s)", label, icon.shortcut.c_str());
  if (!icon.ported && written > 0 && static_cast<std::size_t>(written) < sizeof(text)) {
    std::snprintf(text + written, sizeof(text) - static_cast<std::size_t>(written), "\n%s",
                  core::Tr("Pas encore disponible dans ce client"));
  }
  // ⚠ L'HABILLAGE EST EN MAIN, et aucune fenêtre habillée n'est en cours : les
  // icônes sont des cadres libres. La bulle le reçoit donc directement.
  if (BeginSkinnedTooltip(art)) {
    ImGui::TextUnformatted(text);
    EndSkinnedTooltip(art);
  }
}

// La taille d'une icône à l'écran : celle de son bitmap normal, ou du carré de
// secours. ⚠ LE NORMAL, jamais le « nouveau » : c'est lui qui donne la place,
// le badge ne fait que déborder au-dessus.
[[nodiscard]] ImVec2 IconSize(const MenuIconView& icon, float scale) {
  return icon.normal.usable()
             ? ImVec2(icon.normal.width * scale, icon.normal.height * scale)
             : ImVec2(kMenuIconFallbackSide * scale, kMenuIconFallbackSide * scale);
}

// LA PASTILLE, posée DANS le coin haut-droit de la case : elle ne déborde pas
// sur la voisine, et le cadre de l'icône ne la rogne pas.
void PaintDot(ImDrawList* into, ImVec2 cell, float width, float scale) {
  const float radius = kDotRadiusPixels * scale;
  const float ring = kDotRingPixels * scale;
  const ImVec2 center(cell.x + width - radius - ring, cell.y + radius + ring);
  into->AddCircleFilled(center, radius + ring, kDotRing);
  into->AddCircleFilled(center, radius, kDotFill);
}

// Peint une icône dans son cadre. `held` : le doigt appuie dessus.
void PaintIcon(const MenuIconView& icon, const HudRect& rect, bool held, bool hovered,
               float scale) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 cell(rect.x, rect.y);
  const ShownArt art = ArtOf(icon, held);
  const SkinImage& shown = *art.image;
  // ⚠ LA PASTILLE CÈDE AU « NOUVEAU » : le badge peint dit déjà qu'il y a
  // quelque chose à voir, et deux signaux superposés n'en diraient pas plus.
  const bool dotted = icon.dot_lit && !art.news;
  if (!shown.usable()) {
    DrawFallback(into, cell, rect.width,
                 icon.label.empty() ? MenuIconLabel(icon.command) : icon.label.c_str(), hovered);
    if (dotted) {
      PaintDot(into, cell, rect.width, scale);
    }
    return;
  }
  // ⚠ LE « NOUVEAU » MONTE, il ne descend pas : son badge déborde en haut, et
  // le bas de l'icône reste aligné sur ses voisines. Il sort donc du cadre, dont
  // la découpe s'élargit d'autant le temps de le peindre.
  const float lift = art.news ? kMenuIconNewsLift * scale : 0.0F;
  const ImVec2 top_left(cell.x, cell.y - lift);
  const ImVec2 bottom_right(top_left.x + shown.width * scale, top_left.y + shown.height * scale);
  into->PushClipRect(top_left, bottom_right, false);
  into->AddImage(shown.id, top_left, bottom_right);
  into->PopClipRect();
  if (dotted) {
    PaintDot(into, cell, rect.width, scale);
  }
}

// Le rang de chaque commande dans la grille du jeu, et le nombre de cases.
// ⚠ UN BOUTON HORS GRILLE N'Y COMPTE PAS : il ne décale aucune case.
struct GridRanks {
  std::array<std::size_t, kMenuCommandCount> of{};
  std::size_t count = 0;
};

// LE RANG DE CHAQUE ICÔNE DANS LA GRILLE.
//
// ⚠ ACCROCHÉE SOUS LA FENÊTRE « BASIC INFO », LA GRILLE SE TASSE comme celle du
// jeu : une icône masquée n'y garde pas sa case.
[[nodiscard]] GridRanks RanksOf(std::span<const MenuIconView> icons, const MenuIconsLook& look,
                                bool docked) {
  GridRanks ranks;
  for (const MenuIconView& icon : icons) {
    const bool hidden = look.places[static_cast<std::size_t>(icon.command)].hidden;
    if (!MenuIconOf(icon.command).off_grid && !(docked && hidden)) {
      ranks.of[static_cast<std::size_t>(icon.command)] = ranks.count++;
    }
  }
  return ranks;
}

// La place d'origine d'une icône : sa case dans la grille, ou celle du bouton.
//
// ⚠ ACCROCHÉE, LA GRILLE GLISSE EN BLOC : chaque case garde sa place dans la
// grille, et c'est l'origine qui passe du coin de l'écran à celui de la
// fenêtre. La marge réservée au badge « nouveau » reste au-dessus de la
// première rangée, d'où le retrait de `kMenuIconNewsLift`.
[[nodiscard]] ImVec2 HomeCorner(MenuCommand command, const GridRanks& ranks,
                                const MenuIconsState& state, float scale) {
  const ImVec2 corner =
      MenuIconOf(command).off_grid
          ? DefaultMenuButtonCorner(ranks.count, scale)
          : DefaultMenuIconCorner(ranks.of[static_cast<std::size_t>(command)], scale);
  if (!state.dock.has_value()) {
    return corner;
  }
  const float margin = kMenuIconsScreenMargin * scale;
  return {corner.x - margin + state.dock->x + kMenuIconsDockLeft * scale,
          corner.y - margin + state.dock->y +
              (kMenuIconsDockTop - kMenuGridAboveDock - kMenuIconNewsLift) * scale};
}

// LA GRILLE ACCROCHÉE : son panneau sombre et son bandeau de tête, dans une
// fenêtre à elle, posée sous les icônes. Rend vrai quand les icônes s'y montrent.
//
// ⚠ OUVERTE, TOUT SON RECTANGLE ARRÊTE LES CLICS ; fermée, son bandeau seul.
// Le bandeau du « nouveau » remplace celui qui ouvre quand une icône attend.
[[nodiscard]] bool DrawDockedGrid(MenuIconsState& state, std::span<const MenuIconView> icons,
                                  std::size_t grid_count, const SkinArt& art, float scale) {
  const ImVec2 top_left(state.dock->x, state.dock->y - kMenuGridAboveDock * scale);
  const float height = grid_count > kMenuGridTallAfter ? kMenuGridTallHeight : kMenuGridHeight;
  const bool news = std::ranges::any_of(icons, [](const MenuIconView& icon) {
    return icon.news_lit && !MenuIconOf(icon.command).off_grid;
  });
  const bool news_band = !state.grid_open && news;
  const float band_top = news_band ? 0.0F : kMenuGridBandTop;
  const float band_height = news_band ? kMenuGridNewsBandHeight : kMenuGridBandHeight;
  const float shown_height = state.grid_open ? height : band_top + band_height;

  ImGui::SetNextWindowPos(top_left, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(kMenuGridWidth * scale, std::ceil(shown_height * scale)),
                           ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("##grille_accrochee", nullptr, kFlags)) {
    const achievement::Painter paint(ImGui::GetWindowDrawList(), top_left, scale);
    if (state.grid_open) {
      // LE PANNEAU COUPÉ au bas de la grille : l'image est plus haute qu'elle.
      paint.PushClip(0.0F, kMenuGridBackgroundTop, kMenuGridWidth, height - kMenuGridBackgroundTop);
      paint.Image(art.Get(render::SkinPiece::kMenuGridBackground), 0.0F, kMenuGridBackgroundTop);
      paint.PopClip();
      // Le reste du rectangle arrête les clics : rien ne passe au monde.
      static_cast<void>(paint.Hit("##panneau", 0.0F, band_top + band_height, kMenuGridWidth,
                                  height - band_top - band_height));
    }
    const achievement::HitResult band = paint.Hit("##bandeau", 0.0F, band_top, kMenuGridBandWidth,
                                                  band_height, achievement::kOnRelease);
    using render::SkinPiece;
    const SkinPiece out = state.grid_open ? SkinPiece::kMenuGridCloseOut
                          : news_band     ? SkinPiece::kMenuGridNewsOut
                                          : SkinPiece::kMenuGridOpenOut;
    const auto step = static_cast<int>(band.held ? 2 : band.hovered ? 1 : 0);
    paint.Image(art.Get(static_cast<SkinPiece>(static_cast<int>(out) + step)), 0.0F, band_top);
    if (band.pressed) {
      state.grid_open = !state.grid_open;
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  return state.grid_open;
}

// L'ICÔNE SUIT-ELLE LA FENÊTRE « BASIC INFO » ? Toute icône de la grille, tant
// que la fenêtre est montrée.
[[nodiscard]] bool FollowsDock(const MenuIconsState& state, MenuCommand command) {
  return state.dock.has_value() && !MenuIconOf(command).off_grid;
}

// L'identité ImGui du cadre d'une icône. ⚠ PAR LE NOM DU CATALOGUE, jamais par
// celui du dessin : le statut d'un Doram a un autre bitmap, pas une autre place.
using FrameId = std::array<char, 48>;

[[nodiscard]] FrameId FrameIdOf(MenuCommand command) {
  FrameId id{};
  const std::string_view stem = MenuIconOf(command).stem;
  std::snprintf(id.data(), id.size(), "##menu_icon_%.*s", static_cast<int>(stem.size()),
                stem.data());
  return id;
}

void DrawPlaceholder(MenuIconsState& state, const SkinArt& art) {
  if (!state.placeholder_open) {
    return;
  }
  // ⚠ LE TITRE CHANGE AVEC L'ICÔNE, L'IDENTITÉ NON : une seule fenêtre
  // d'attente, à une seule place, quelle que soit l'icône qui l'a ouverte.
  char title[128];
  std::snprintf(title, sizeof(title), "%s###%s", MenuIconLabel(state.placeholder),
                kMenuPlaceholderId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  if (BeginSkinnedWindow(title, &state.placeholder_open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    ImGui::TextUnformatted(core::Tr("Cette fenêtre n'existe pas encore dans ce client."));
    ImGui::TextUnformatted(
        core::Tr("Son icône est déjà à sa place : elle l'ouvrira dès qu'elle sera portée."));
    ImGui::Spacing();
    if (SkinButton(core::Tr("Fermer"), art)) {
      state.placeholder_open = false;
    }
  }
  EndSkinnedWindow();
}

}  // namespace

std::span<const MenuIconEntry> MenuIconCatalogue() { return kCatalogue; }

const MenuIconEntry& MenuIconOf(MenuCommand command) {
  return kCatalogue[static_cast<std::size_t>(command)];
}

const char* MenuIconLabel(MenuCommand command) {
  // ⚠ UN LITTÉRAL PAR CAS, et non une table : le relevé des traductions ne lit
  // que les littéraux passés à `Tr`.
  switch (command) {
    case kStatus:
      return core::Tr("Statut");
    case kEquipment:
      return core::Tr("Équipement");
    case kItem:
      return core::Tr("Inventory");
    case kSkill:
      return core::Tr("Skill");
    case kBooking:
      return core::Tr("Recherche de party");
    case kParty:
      return core::Tr("Party");
    case kGuild:
      return core::Tr("Guilde");
    case kBattleground:
      return core::Tr("Champ de bataille");
    case kQuest:
      return core::Tr("Quêtes");
    case kWorldMap:
      return core::Tr("Carte du monde");
    case kNavigation:
      return core::Tr("Navigation");
    case kOption:
      return core::Tr("Options");
    case kBank:
      return core::Tr("Banque");
    case kReplay:
      return core::Tr("Replay");
    case kMail:
      return core::Tr("Courrier");
    case kAchievement:
      return core::Tr("Succès");
    case kTip:
      return core::Tr("Astuces");
    case kCashShop:
      return core::Tr("Vote Shop");
    case kShortcut:
      return core::Tr("Configuration des raccourcis");
    case kSocialNetwork:
      return core::Tr("Réseaux sociaux");
    case kAttendance:
      return core::Tr("Présence");
    case kAdventurerAgency:
      return core::Tr("Agence d'aventuriers");
    case kReputation:
      return core::Tr("Réputation");
    case kAdventureGuide:
      return core::Tr("Guide de l'aventurier");
    case kProbability:
      return core::Tr("Probabilités");
    case kCardAlbum:
      return core::Tr("Album de cartes");
    case kCraftAtlas:
      return core::Tr("Atlas des recettes");
    case kMvpTracker:
      return core::Tr("Carnet de chasse MVP");
    case kPetWindow:
      return core::Tr("Fiche du pet");
    case kRankingBoard:
      return core::Tr("Tableau des classements");
    case kMarket:
      return core::Tr("Marché");
    case kDatabase:
      return core::Tr("Base de données");
    case kCashShopButton:
      return core::Tr("Bouton du Vote Shop");
  }
  return "";
}

std::string_view MenuIconStem(MenuCommand command, bool doram) {
  if (command == kStatus && doram) {
    return kDoramStatusStem;
  }
  return MenuIconOf(command).stem;
}

std::string MenuIconPath(std::string_view stem, MenuIconArt art) {
  std::string path(render::kSkinRoot);
  path += kMenuIconFolder;
  path += stem;
  switch (art) {
    case MenuIconArt::kNormal:
      break;
    case MenuIconArt::kPressed:
      path += "_press";
      break;
    case MenuIconArt::kNews:
      path += "_new";
      break;
    case MenuIconArt::kNewsPressed:
      path += "_new_press";
      break;
  }
  path += kMenuIconExtension;
  return path;
}

std::string OurMenuIconPath(std::string_view stem, MenuIconArt art) {
  std::string path(kOurIconFolder);
  path += kOurMenuIconPrefix;
  path += stem;
  switch (art) {
    case MenuIconArt::kNormal:
      break;
    case MenuIconArt::kPressed:
      path += "_pressed";
      break;
    case MenuIconArt::kNews:
      path += "_new";
      break;
    case MenuIconArt::kNewsPressed:
      path += "_new_pressed";
      break;
  }
  path += kOurIconExtension;
  return path;
}

SkinImage AtOurDensity(SkinImage ours) {
  ours.width /= kOurIconDensity;
  ours.height /= kOurIconDensity;
  return ours;
}

SkinImage PreferredMenuIconArt(const SkinImage& ours, const SkinImage& native) {
  return ours.usable() ? AtOurDensity(ours) : native;
}

std::string MenuButtonPath(std::string_view stem) {
  std::string path(render::kSkinRoot);
  path += kMenuButtonFolder;
  path += stem;
  path += kMenuIconExtension;
  return path;
}

ImVec2 MenuIconCellOffset(std::size_t shown_rank) {
  const std::size_t column = shown_rank % kMenuIconsPerRow;
  const std::size_t row = shown_rank / kMenuIconsPerRow;
  return {static_cast<float>(column) * kMenuIconColumnPitch,
          kMenuIconNewsLift + static_cast<float>(row) * kMenuIconRowPitch};
}

ImVec2 MenuIconsGridSize(std::size_t shown_count) {
  const std::size_t columns = std::min(shown_count, kMenuIconsPerRow);
  const std::size_t rows = (shown_count + kMenuIconsPerRow - 1) / kMenuIconsPerRow;
  return {static_cast<float>(columns) * kMenuIconColumnPitch,
          kMenuIconNewsLift + static_cast<float>(rows) * kMenuIconRowPitch};
}

std::optional<MenuCommand> PressMenuIcon(MenuIconsState& state, const MenuIconView& icon) {
  if (icon.ported) {
    return icon.command;
  }
  if (state.placeholder_open && state.placeholder == icon.command) {
    state.placeholder_open = false;
  } else {
    state.placeholder_open = true;
    state.placeholder = icon.command;
  }
  return std::nullopt;
}

bool MenuIconPlaced(const MenuIconPlace& place) noexcept {
  // ⚠ UNE ANCRE AUTRE QUE LE COIN HAUT-GAUCHE NE NAÎT QUE D'UNE POSE : un écart
  // au centre peut être négatif sans dire « jamais posée ».
  return place.across != 0 || place.down != 0 || (place.x >= 0.0F && place.y >= 0.0F);
}

ImVec2 DefaultMenuIconCorner(std::size_t grid_rank, float scale) {
  const ImVec2 cell = MenuIconCellOffset(grid_rank);
  const float margin = kMenuIconsScreenMargin * scale;
  return {margin + cell.x * scale, margin + cell.y * scale};
}

ImVec2 DefaultMenuButtonCorner(std::size_t grid_count, float scale) {
  const ImVec2 grid = MenuIconsGridSize(grid_count);
  const float margin = kMenuIconsScreenMargin * scale;
  return {margin, margin + (grid.y + kMenuButtonGap) * scale};
}

float MenuIconsHomeRight(float scale) {
  return (kMenuIconsScreenMargin + MenuIconsGridSize(kMenuIconsPerRow).x) * scale;
}

MenuIconsAnswer DrawMenuIcons(MenuIconsState& state, std::span<const MenuIconView> icons,
                              MenuIconsLook& look, const AlignGrid& grid, HudGrip& grip,
                              const SkinArt& art) {
  MenuIconsAnswer answer;
  const float scale = Scale();
  const GridRanks ranks = RanksOf(icons, look, state.dock.has_value());
  // ACCROCHÉE ET FERMÉE, la grille ne montre que son bandeau.
  const bool grid_hidden =
      state.dock.has_value() && !DrawDockedGrid(state, icons, ranks.count, art, scale);

  // LES CADRES DE CETTE IMAGE, un par commande : c'est la liste des frères qui
  // s'aimantent et partent en bloc. ⚠ UNE ICÔNE MASQUÉE N'Y COMPTE PAS : on ne
  // s'aligne pas sur ce qu'on ne voit pas.
  std::array<HudRect, kMenuCommandCount> rects{};
  std::array<bool, kMenuCommandCount> shown{};
  for (const MenuIconView& icon : icons) {
    const std::size_t index = static_cast<std::size_t>(icon.command);
    MenuIconPlace& place = look.places[index];
    if (place.hidden) {
      continue;
    }
    const ImVec2 size = IconSize(icon, scale);
    if (grid_hidden && FollowsDock(state, icon.command)) {
      continue;
    }
    if (FollowsDock(state, icon.command)) {
      const ImVec2 corner = HomeCorner(icon.command, ranks, state, scale);
      rects[index] = HudRect{.x = corner.x, .y = corner.y, .width = size.x, .height = size.y};
      shown[index] = true;
      continue;
    }
    if (!MenuIconPlaced(place) && !look.locked) {
      const ImVec2 corner = HomeCorner(icon.command, ranks, state, scale);
      place.x = corner.x;
      place.y = corner.y;
      answer.moved = true;
    }
    // ⚠ LA PLACE RETENUE, ANCRÉE : c'est le cadre qui la ramène dans l'écran au
    // dessin, sans toucher au fichier.
    if (MenuIconPlaced(place)) {
      rects[index] = HudRect{.x = place.x,
                             .y = place.y,
                             .width = size.x,
                             .height = size.y,
                             .across = place.across,
                             .down = place.down};
    } else {
      const ImVec2 corner = HomeCorner(icon.command, ranks, state, scale);
      rects[index] = HudRect{.x = corner.x, .y = corner.y, .width = size.x, .height = size.y};
    }
    shown[index] = true;
  }

  for (const MenuIconView& icon : icons) {
    const std::size_t index = static_cast<std::size_t>(icon.command);
    if (!shown[index]) {
      continue;
    }
    // ⚠ ACCROCHÉE, L'ICÔNE SE CLIQUE TOUJOURS : sa place n'est pas à ranger.
    const bool locked = look.locked || FollowsDock(state, icon.command);
    HudFrameLook frame;
    frame.locked = locked;
    frame.clickable = locked;
    frame.resizable = false;
    frame.lit_on_hover = false;
    // Le filet pendant la pose seulement : les icônes n'ont pas de fond, et
    // sans lui on ne voit pas la place qu'on attrape.
    frame.border = !locked;
    frame.sticky = look.sticky;
    frame.grid = &grid;
    frame.least_width = rects[index].width;
    frame.least_height = rects[index].height;

    HudSiblings siblings;
    siblings.frames = rects;
    siblings.shown = shown;
    siblings.mine = static_cast<int>(index);

    const FrameId id = FrameIdOf(icon.command);
    HudFrameClicks clicks;
    bool moved = false;
    if (BeginHudFrame(id.data(), rects[index], frame, siblings, grip, &moved, &clicks)) {
      // ⚠ EN POSE, NI ENFONCÉE NI OUVERTE : on range des icônes, l'état des
      // fenêtres n'a rien à y dire.
      MenuIconView painted = icon;
      if (!locked) {
        painted.shows_open = false;
      }
      PaintIcon(painted, HudOnScreen(rects[index], ImGui::GetIO().DisplaySize), clicks.held,
                clicks.hovered, scale);
      if (clicks.hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        DrawTooltip(icon, art);
      }
      if (clicks.left) {
        if (const std::optional<MenuCommand> pressed = PressMenuIcon(state, icon);
            pressed.has_value()) {
          answer.clicked = pressed;
        }
      }
    }
    EndHudFrame(grip);
    if (moved) {
      answer.moved = true;
    }
  }

  // ⚠ TOUTES LES PLACES RECOPIÉES, et non celle de l'icône saisie : le bloc CTRL
  // déplace ses frères dans `rects`, et ils doivent s'en souvenir aussi.
  if (answer.moved) {
    for (std::size_t index = 0; index < kMenuCommandCount; ++index) {
      if (shown[index] && !FollowsDock(state, static_cast<MenuCommand>(index))) {
        look.places[index].x = rects[index].x;
        look.places[index].y = rects[index].y;
        look.places[index].across = rects[index].across;
        look.places[index].down = rects[index].down;
      }
    }
  }

  DrawPlaceholder(state, art);
  return answer;
}

}  // namespace ro::ui
