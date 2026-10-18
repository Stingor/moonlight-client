#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "navigation_parts.h"
#include "ui_scale.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Le côté d'une vignette, en pixels d'interface.
constexpr float kThumbnailPixels = 160.0F;

// LA PASTILLE MVP : fond doré soutenu, texte presque noir. En texte doré sur
// le corps clair, elle disparaîtrait.
constexpr ImU32 kMvpFill = IM_COL32(198, 146, 12, 255);
constexpr ImU32 kMvpText = IM_COL32(28, 20, 0, 255);
constexpr float kMvpRounding = 3.0F;
constexpr float kMvpPadShare = 0.7F;
constexpr const char* kMvpLabel = "MVP";

constexpr const char* kPlaceMenuId = "##navi_row_menu";
constexpr const char* kMonsterMenuId = "##navi_monster_menu";

constexpr std::size_t kLabelBytes = 256;
constexpr std::size_t kTailBytes = 64;

// Le geste « Téléporter » du staff dit où il pose : sur la case du NPC, ou sur
// la map entière — celle d'un spawn de monstre comprise.
[[nodiscard]] const char* WarpLabel(const NavigationEntryView& entry) {
  if (entry.kind == NavigationKind::kNpc && entry.has_cell) {
    return core::Tr("Téléporter sur ce NPC");
  }
  return core::Tr("Téléporter sur cette map");
}

void MvpBadge() {
  const ImVec2 text = ImGui::CalcTextSize(kMvpLabel);
  const float pad = ImGui::GetStyle().FramePadding.x * kMvpPadShare;
  const ImVec2 low = ImGui::GetCursorScreenPos();
  const ImVec2 high(low.x + text.x + pad * 2.0F, low.y + text.y);
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(low, high, kMvpFill, kMvpRounding);
  into->AddText(ImVec2(low.x + pad, low.y), kMvpText, kMvpLabel);
  // Réserver la place, sinon ce qui suit se peindrait par-dessus.
  ImGui::Dummy(ImVec2(high.x - low.x, high.y - low.y));
}

[[nodiscard]] const char* KindWord(NavigationKind kind) {
  switch (kind) {
    case NavigationKind::kMap:
      return "map";
    case NavigationKind::kNpc:
      return "NPC";
    case NavigationKind::kMonster:
      return core::Tr("monstre");
  }
  return "";
}

// Le plan de la map au survol : l'en-tête d'un groupe n'y a droit que s'il
// désigne UNE seule map — un monstre groupé par nom vit souvent sur plusieurs.
void MapTooltip(const NavigationEntryView& entry) {
  if (entry.map_name.empty() || !BeginSkinTooltip()) {
    return;
  }
  Unformatted(entry.map_name);
  DrawNavigationMapThumbnail(entry.map_image, Px(kThumbnailPixels));
  EndSkinTooltip();
}

// Le libellé d'une ligne : COURT, le volet de droite dit le reste.
void RowLabel(const NavigationEntryView& entry, bool bare, std::size_t rank,
              std::array<char, kLabelBytes>& label) {
  // Sans en-tête au-dessus, la ligne porte à elle seule la NATURE que
  // l'en-tête disait.
  std::array<char, kTailBytes> tail{};
  if (bare) {
    std::snprintf(tail.data(), tail.size(), "  ·  %s", KindWord(entry.kind));
  }
  const int id = static_cast<int>(rank);
  if (entry.kind == NavigationKind::kMonster) {
    // 🔴 UN MONSTRE N'A PAS DE CELLULE : on dit son niveau.
    std::snprintf(label.data(), label.size(), "%s%s  ·  %s %d###e%d", Owned(entry.name).c_str(),
                  tail.data(), core::Tr("niv."), entry.level, id);
  } else if (entry.kind == NavigationKind::kMap) {
    // Seule, une map montre son nom LISIBLE ; dans un groupe, son nom interne.
    const std::string_view shown = bare && !entry.name.empty()
                                       ? entry.name
                                       : (entry.map_name.empty() ? entry.name : entry.map_name);
    std::snprintf(label.data(), label.size(), "%s%s###e%d", Owned(shown).c_str(), tail.data(), id);
  } else {
    // Le NOM du NPC : le groupe est déjà sa map.
    std::snprintf(label.data(), label.size(), "%s%s###e%d", Owned(entry.name).c_str(), tail.data(),
                  id);
  }
}

void DrawRows(NavigationWindowState& state, const NavigationGroupView& group,
              std::size_t group_rank, bool bare, NavigationAnswer& answer) {
  // 🔴 L'ID DU GROUPE, et pas seulement celui de la ligne : sans en-tête, trois
  // groupes d'une ligne poseraient trois fois le même id dans la même pile.
  ImGui::PushID(static_cast<int>(group_rank));
  for (std::size_t rank = 0; rank < group.entries.size(); ++rank) {
    const NavigationEntryView& entry = group.entries[rank];
    ImGui::PushID(static_cast<int>(rank));
    std::array<char, kLabelBytes> label{};
    RowLabel(entry, bare, rank, label);
    const bool chosen = state.selected_group == static_cast<int>(group_rank) &&
                        state.selected_entry == static_cast<int>(rank);
    if (ImGui::Selectable(label.data(), chosen)) {
      state.selected_group = static_cast<int>(group_rank);
      state.selected_entry = static_cast<int>(rank);
    }
    // LA LIGNE EST AUSSI UN LIEN, pour les gestes MODIFIÉS seulement : le clic
    // gauche appartient à la sélection.
    const NavigationRowRef row{.group = group_rank, .entry = rank};
    switch (HitChatLink(ImGui::IsItemHovered())) {
      case ChatLinkGesture::kRepost:
        answer.link_to_chat = row;
        break;
      case ChatLinkGesture::kMenu:
        state.menu_armed = true;
        state.menu_row = row;
        break;
      default:
        break;
    }
    if (entry.is_mvp) {
      ImGui::SameLine();
      MvpBadge();
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

// Le sprite, à sa taille réelle dans le carré : un Poring gonflé à la taille
// d'un Baphomet mentirait sur le gabarit de la bête.
void DrawSprite(const SkinImage& sprite, float side) {
  const float fit = std::min({side / sprite.width, side / sprite.height, 1.0F});
  const ImVec2 low = ImGui::GetCursorScreenPos();
  const float width = sprite.width * fit;
  const float height = sprite.height * fit;
  const ImVec2 at(low.x + (side - width) * 0.5F, low.y + (side - height) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(sprite.id, at, ImVec2(at.x + width, at.y + height));
}

void DrawPreview(const NavigationEntryView& entry, const NavigationRowRef& row,
                 NavigationAnswer& answer) {
  const float side = Px(kNavigationPreviewPixels);
  if (!entry.sprite.usable() && entry.sprite_expected) {
    ImGui::Dummy(ImVec2(side, side));
    return;
  }
  if (!entry.sprite.usable()) {
    // Pour une map, ou un NPC que l'installation ne sait pas montrer : le plan.
    if (!entry.map_name.empty()) {
      DrawNavigationMapThumbnail(entry.map_image, side);
    }
    return;
  }
  DrawSprite(entry.sprite, side);
  // Cliquable pour un MONSTRE : la navigation sait OÙ, la fiche sait QUOI. Un
  // NPC n'a pas de fiche ; son sprite ne sert qu'à le reconnaître.
  const bool clickable = entry.kind == NavigationKind::kMonster;
  static_cast<void>(ImGui::InvisibleButton("##navi_sprite", ImVec2(side, side)));
  if (clickable && ImGui::IsItemClicked()) {
    answer.open_monster = row;
  }
  // Le survol garde le PLAN, qui répond à « où est-ce ? ».
  if (ImGui::IsItemHovered() && BeginSkinTooltip()) {
    if (clickable) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (!entry.map_name.empty()) {
      Unformatted(entry.map_name);
      DrawNavigationMapThumbnail(entry.map_image, Px(kThumbnailPixels));
    }
    if (clickable) {
      ImGui::TextDisabled("%s", core::Tr("Ouvrir la fiche du monstre"));
    }
    EndSkinTooltip();
  }
}

}  // namespace

void DrawNavigationPlaceTip(const NavigationPlaceTip& tip) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(tip.label.empty() ? tip.map_name.c_str() : tip.label.c_str());
  ImGui::Separator();
  DrawNavigationMapThumbnail(tip.map_image, Px(kThumbnailPixels));
  // Le nom INTERNE, celui qu'une commande accepte.
  if (tip.x > 0 && tip.y > 0) {
    ImGui::TextDisabled("%s  (%d, %d)", tip.map_name.c_str(), tip.x, tip.y);
  } else {
    ImGui::TextDisabled("%s", tip.map_name.c_str());
  }
  ImGui::Spacing();
  ImGui::TextDisabled("%s", core::Tr("Clic : lancer le guidage"));
  EndSkinTooltip();
}

void DrawNavigationMapThumbnail(const SkinImage& image, float side) {
  if (!image.usable()) {
    ImGui::TextDisabled("%s", core::Tr("(pas de miniature)"));
    return;
  }
  const float scale = image.width > image.height ? side / image.width : side / image.height;
  ImGui::Image(image.id, ImVec2(image.width * scale, image.height * scale));
}

void DrawNavigationResults(NavigationWindowState& state, const NavigationView& view,
                           NavigationAnswer& answer) {
  if (view.groups.empty()) {
    ImGui::TextDisabled("%s", state.term[0] == '\0' ? core::Tr("Tapez un nom pour chercher.")
                                                    : core::Tr("Aucun résultat."));
    return;
  }
  // Les gestes MODIFIÉS ne se devinent pas : rien d'autre ne les annonce.
  ImGui::TextDisabled("%s", core::Tr("Clic droit : menu · Maj+clic : lien dans le chat"));

  // 🔴 LE SEUL RÉSULTAT VISIBLE SE SÉLECTIONNE TOUT SEUL : il n'y a rien à
  // choisir. Compté sur ce que le filtre laisse voir, et seulement à UN — dès
  // qu'il y a un choix, il appartient au joueur.
  std::size_t visible = 0;
  for (const NavigationGroupView& group : view.groups) {
    if (NavigationGroupShown(state.filter, group.kind)) {
      visible += group.entries.size();
    }
  }
  const bool single = visible == 1;

  for (std::size_t rank = 0; rank < view.groups.size(); ++rank) {
    const NavigationGroupView& group = view.groups[rank];
    if (!NavigationGroupShown(state.filter, group.kind) || group.entries.empty()) {
      continue;
    }
    // UN GROUPE D'UNE LIGNE NE SE PLIE PAS : il n'y aurait qu'un pli à ouvrir
    // pour relire ce qu'on avait demandé.
    const bool bare = group.entries.size() == 1;
    if (!bare) {
      std::array<char, kLabelBytes> header{};
      std::snprintf(header.data(), header.size(), "%s  (%s, %d)###navi_g%d",
                    Owned(group.name).c_str(), KindWord(group.kind),
                    static_cast<int>(group.entries.size()), static_cast<int>(rank));
      const bool opened = ImGui::TreeNode(header.data());
      const bool one_map = std::ranges::all_of(group.entries, [&](const NavigationEntryView& e) {
        return e.map_name == group.entries.front().map_name;
      });
      if (ImGui::IsItemHovered() && one_map) {
        MapTooltip(group.entries.front());
      }
      if (std::ranges::any_of(group.entries,
                              [](const NavigationEntryView& e) { return e.is_mvp; })) {
        ImGui::SameLine();
        MvpBadge();
      }
      if (!opened) {
        continue;
      }
    }
    if (single) {
      state.selected_group = static_cast<int>(rank);
      state.selected_entry = 0;
    }
    DrawRows(state, group, rank, bare, answer);
    // Pas d'arbre ouvert sans en-tête : un `TreePop` de trop déséquilibrerait
    // l'indentation.
    if (!bare) {
      ImGui::TreePop();
    }
  }
}

void DrawNavigationDetail(const NavigationWindowState& state, const NavigationView& view,
                          const SkinArt& art, NavigationAnswer& answer) {
  const NavigationEntryView* chosen = NavigationSelection(state, view);
  if (chosen == nullptr) {
    ImGui::TextDisabled("%s", core::Tr("Choisissez un résultat."));
    return;
  }
  const NavigationEntryView& entry = *chosen;
  const NavigationRowRef row{.group = static_cast<std::size_t>(state.selected_group),
                             .entry = static_cast<std::size_t>(state.selected_entry)};

  ImGui::TextWrapped("%s", Owned(entry.name).c_str());
  if (entry.is_mvp) {
    MvpBadge();
  }
  if (entry.is_shop) {
    ImGui::TextDisabled("%s", core::Tr("boutique"));
  }
  ImGui::Separator();
  if (!entry.map_name.empty()) {
    ImGui::TextDisabled("%s", Owned(entry.map_name).c_str());
  }
  DrawPreview(entry, row, answer);

  if (entry.kind == NavigationKind::kMonster) {
    ImGui::Text("%s %d", core::Tr("niv."), entry.level);
    ImGui::Text("%s : %d", core::Tr("Exemplaires"), entry.amount);
    if (entry.has_stats) {
      ImGui::Text("%s %d", NavigationElementName(entry.element), entry.element_level);
      ImGui::TextUnformatted(NavigationSizeName(entry.size));
      ImGui::TextUnformatted(NavigationRaceName(entry.race));
    }
  } else if (entry.kind == NavigationKind::kNpc && entry.has_cell) {
    ImGui::Text("(%d, %d)", entry.x, entry.y);
  }
  ImGui::Separator();

  const bool reachable = !entry.map_name.empty();
  ImGui::BeginDisabled(!reachable);
  if (SkinButton(core::Tr("Y aller"), art)) {
    answer.go_to = row;
  }
  // « Partager » ne fait qu'ÉCRIRE dans la barre de chat : le joueur relit et
  // envoie lui-même.
  if (SkinButton(core::Tr("Partager"), art)) {
    answer.share = row;
  }
  ImGui::EndDisabled();
  SkinTooltip(core::Tr("Écrit un lien dans votre barre de chat ; à vous de l'envoyer."));
  if (state.staff_warp && reachable) {
    if (SkinButton(WarpLabel(entry), art)) {
      answer.warp_to = row;
    }
    SkinTooltip(core::Tr("Commande du staff : @warp map,x,y."));
  }
}

void DrawNavigationRowMenu(NavigationWindowState& state, const NavigationView& view,
                           NavigationAnswer& answer) {
  const NavigationRowRef row = state.menu_row;
  const NavigationEntryView* entry = nullptr;
  if (row.group < view.groups.size() && row.entry < view.groups[row.group].entries.size()) {
    entry = &view.groups[row.group].entries[row.entry];
  }
  if (state.menu_armed) {
    state.menu_armed = false;
    if (entry != nullptr) {
      ImGui::OpenPopup(entry->kind == NavigationKind::kMonster ? kMonsterMenuId : kPlaceMenuId);
    }
  }
  if (entry == nullptr) {
    return;
  }
  // UN MONSTRE A LE MENU DE TOUT LIEN DE MONSTRE : la fiche, le lien dans le
  // chat et la recherche. ⚠ NI BESTIAIRE NI COMMANDES : son identifiant est la
  // classe de sprite des données de navigation, pas celui de la base.
  if (entry->kind == NavigationKind::kMonster) {
    const MvpLinkMenuFacts facts{.named = !entry->name.empty(), .sprite_class_only = true};
    const ChatLinkAsk asked =
        DrawChatLinkMenu(kMonsterMenuId, ChatLinkKind::kMonster, entry->monster_id, 0, entry->name,
                         {}, nullptr, &facts);
    if (asked.gesture != ChatLinkGesture::kNothing) {
      answer.monster_link = asked;
      answer.monster_name = std::string(entry->name);
      answer.monster_is_mvp = entry->is_mvp;
    }
    return;
  }
  if (!BeginSkinPopup(kPlaceMenuId)) {
    return;
  }
  if (ImGui::MenuItem(core::Tr("Y aller"))) {
    answer.go_to = row;
  }
  // Ce qu'il y a SUR la map, par opposition à « y aller ».
  if (ImGui::MenuItem(core::Tr("Voir ce qu'il y a là-bas"))) {
    answer.show_map_of = row;
  }
  if (state.staff_warp && !entry->map_name.empty() && ImGui::MenuItem(WarpLabel(*entry))) {
    answer.warp_to = row;
  }
  if (ImGui::MenuItem(core::Tr("Lien dans le chat"))) {
    answer.link_to_chat = row;
  }
  if (ImGui::MenuItem(core::Tr("Copier le nom"))) {
    answer.copy_name = row;
  }
  EndSkinPopup();
}

}  // namespace ro::ui
