// Le volet de droite du journal des quêtes : le détail de la quête choisie.
// Voir l'en-tête.

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/rich_text.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "quest_window_parts.h"
#include "ui_scale.h"

namespace ro::ui::quest_window {
namespace {

// L'écart sous le titre, entre deux blocs, et autour du filet d'une section.
constexpr float kBlockGap = 4.0F;
constexpr float kSectionGap = 8.0F;
constexpr float kRulePixels = 1.0F;
// La pastille d'état, à côté de laquelle se lit « Objectifs remplis ». Son
// texte prend l'état du thème : réussite, secondaire, avertissement.
constexpr ImU32 kActiveFill = IM_COL32(0xD8, 0xEF, 0xDC, 0xFF);
constexpr ImU32 kSuspendedFill = IM_COL32(0xE4, 0xE6, 0xEC, 0xFF);
constexpr ImU32 kWaitingFill = IM_COL32(0xFC, 0xEB, 0xD2, 0xFF);
// Une illustration d'affiche ne dépasse pas cette hauteur, en pixels d'art.
constexpr float kPosterMostHeight = 220.0F;
// Une chasse : la barre sous son libellé.
constexpr float kHuntBarHeight = 5.0F;
constexpr float kHuntBarGap = 2.0F;
// Une récompense : son icône, à la hauteur d'une ligne et un peu plus.
constexpr float kRewardIconSide = 24.0F;
constexpr float kRewardRowGap = 2.0F;
// Le filigrane du détail vide : la place sous l'image, avant la phrase.
constexpr float kEmptyTextGap = 8.0F;

// Assez pour « Échéance : 2026/10/07 18:00 » et « 1 234 567 890 » dans toute
// langue.
constexpr std::size_t kLineBytes = 192;

// UN TEXTE EN GRAS, replié à `wrap` pixels d'écran : le faux gras du client,
// le même texte repassé un pixel plus loin.
void BoldText(std::string_view text, ImU32 color, float wrap) {
  if (text.empty()) {
    return;
  }
  ImFont* const font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const float bold = Px(kFakeBoldPixels);
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, wrap, begin, end);
  ImDrawList* const into = ImGui::GetWindowDrawList();
  into->AddText(font, size, at, color, begin, end, wrap);
  into->AddText(font, size, ImVec2(at.x + bold, at.y), color, begin, end, wrap);
  ImGui::Dummy(ImVec2(extent.x + bold, extent.y));
}

void ColoredText(ImU32 color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::PushTextWrapPos(0.0F);
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

// LE FILET qui ouvre une section, puis son titre.
void SectionHeading(const char* title) {
  ImGui::Dummy(ImVec2(0.0F, Px(kSectionGap)));
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const float width = ImGui::GetContentRegionAvail().x;
  ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + width, at.y + Px(kRulePixels)),
                                            PaperColor(kRuleColor));
  ImGui::Dummy(ImVec2(0.0F, Px(kBlockGap)));
  BoldText(title, PaperInk(kDetailTitleText), 0.0F);
  ImGui::Dummy(ImVec2(0.0F, Px(kBlockGap)));
}

// Un lien du détail : la main et le soulignement au survol.
void DecorateLink(bool hovered) {
  if (!hovered) {
    return;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  const ImVec2 least = ImGui::GetItemRectMin();
  const ImVec2 most = ImGui::GetItemRectMax();
  ImGui::GetWindowDrawList()->AddLine(ImVec2(least.x, most.y), ImVec2(most.x, most.y),
                                      ImGui::GetColorU32(ImGuiCol_TextLink));
}

// ── LE DÉTAIL VIDE ──────────────────────────────────────────────────────────

void DrawNothingChosen(const SkinImage& mark, const char* text) {
  const ImVec2 room = ImGui::GetContentRegionAvail();
  const ImVec2 text_size = ImGui::CalcTextSize(text);
  // L'image se rapetisse si le volet est plus petit qu'elle, jamais ne grandit.
  float shown_width = 0.0F;
  float shown_height = 0.0F;
  if (mark.usable()) {
    const float ratio = std::min(
        {Scale(), room.x / mark.width, (room.y - text_size.y - Px(kEmptyTextGap)) / mark.height});
    shown_width = std::max(0.0F, std::floor(mark.width * ratio));
    shown_height = std::max(0.0F, std::floor(mark.height * ratio));
  }
  const float block = shown_height + (shown_height > 0.0F ? Px(kEmptyTextGap) : 0.0F) + text_size.y;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float top = origin.y + std::max(0.0F, std::floor((room.y - block) * 0.5F));
  if (shown_height > 0.0F) {
    const float left = origin.x + std::floor((room.x - shown_width) * 0.5F);
    PaintOnPaper(ImGui::GetWindowDrawList(), mark, ImVec2(left, top),
                 ImVec2(left + shown_width, top + shown_height), Paper());
  }
  const float text_top = top + (shown_height > 0.0F ? shown_height + Px(kEmptyTextGap) : 0.0F);
  ImGui::SetCursorScreenPos(
      ImVec2(origin.x + std::max(0.0F, std::floor((room.x - text_size.x) * 0.5F)), text_top));
  ColoredText(FaintInk(), text);
}

// ── L'EN-TÊTE ───────────────────────────────────────────────────────────────

// LA PASTILLE D'ÉTAT : son texte et ses deux couleurs.
struct StatusPill {
  const char* text = "";
  ImU32 fill = 0;
  SkinTone tone = SkinTone::kText;
};

[[nodiscard]] StatusPill StatusOf(const QuestRowView& row) {
  if (!row.active) {
    return {core::Tr("Suspendue"), kSuspendedFill, SkinTone::kFaint};
  }
  if (row.waiting && !row.expired) {
    return {core::Tr("En attente"), kWaitingFill, SkinTone::kWarn};
  }
  if (row.waiting) {
    return {core::Tr("Prête"), kActiveFill, SkinTone::kGood};
  }
  return {core::Tr("Active"), kActiveFill, SkinTone::kGood};
}

// L'ÉTAT : actif, en attente, prêt ou suspendu, puis l'invitation à rendre la
// quête.
void DrawStatus(const QuestRowView& row) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
  const StatusPill pill = StatusOf(row);
  const float size = ImGui::GetFontSize() / Scale();
  const ImU32 fill = PaperColor(pill.fill);
  const float width = PaintPill(paint, 0.0F, 0.0F, pill.text, size, fill,
                                ReadableOn(PaintingLook().Tone(pill.tone), fill));
  ImGui::Dummy(ImVec2(Px(width), Px(PillHeight(paint, size))));
  const char* invitation = nullptr;
  if (row.objectives_met) {
    invitation = core::Tr("Objectifs remplis : retournez voir le PNJ.");
  } else if (row.active && row.waiting && row.expired) {
    invitation = core::Tr("Délai écoulé : retournez voir le PNJ.");
  }
  if (invitation != nullptr) {
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + Px(kPillPaddingY));
    ColoredText(ToneInk(SkinTone::kGood), invitation);
  }
}

// L'ÉCHÉANCE : en rouge pour une quête qu'elle presse ; pour une attente, le
// moment où elle sera prête, sans alarme.
void DrawDeadline(const QuestRowView& row) {
  if (row.deadline.empty()) {
    return;
  }
  std::array<char, kLineBytes> line{};
  if (row.waiting) {
    std::snprintf(line.data(), line.size(), core::Tr("Prête le : %s"), Owned(row.deadline).c_str());
  } else {
    std::snprintf(line.data(), line.size(), core::Tr("Échéance : %s"), Owned(row.deadline).c_str());
  }
  ImGui::PushStyleColor(ImGuiCol_Text, row.waiting ? PaperInk(kTimeText) : ToneInk(SkinTone::kBad));
  ImGui::TextUnformatted(line.data());
  ImGui::PopStyleColor();
  if (!row.time_left.empty() && !(row.waiting && row.expired)) {
    ImGui::SameLine();
    const ImU32 ink = row.expired  ? ToneInk(SkinTone::kBad)
                      : row.urgent ? ToneInk(SkinTone::kWarn)
                                   : PaperInk(kTimeText);
    ColoredText(ink, "(" + Owned(row.time_left) + ")");
  }
}

void DrawNpc(const std::optional<QuestNpcView>& place, const SkinArt& skin,
             QuestWindowAction& action) {
  if (!place.has_value()) {
    return;
  }
  const QuestNpcView& npc = *place;
  ImGui::AlignTextToFramePadding();
  ColoredText(FaintInk(), core::Tr("PNJ :"));
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(npc.label.data(), npc.label.data() + npc.label.size());
  if (npc.map_name.empty()) {
    return;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Y aller"), skin)) {
    action.guide_to = RichPlace{.map = Owned(npc.map_name), .x = npc.cell_x, .y = npc.cell_y};
  }
  SkinTooltip(core::Tr("Guider le personnage jusqu'au PNJ"));
}

// ── LES SECTIONS ────────────────────────────────────────────────────────────

// L'objet que vise un lien, ou zéro : un lien d'un autre genre n'en vise aucun.
[[nodiscard]] std::uint32_t ItemOf(const RichLink& link) {
  return link.kind == RichLinkKind::kItem ? ReadRichItem(link.payload).value_or(0) : 0;
}

// DES LIGNES BALISÉES, aux liens actifs : un lieu guide, une adresse remonte, un
// objet a les gestes d'une récompense — ils s'écrivent dans `action.item`, et
// son clic droit arme `item_menu`, celui de la fenêtre.
void DrawMarkedLines(const char* id, std::span<const std::string> lines, QuestWindowAction& action,
                     ItemLinkMenu& item_menu) {
  RichText laid = LayOutRichTextHere(lines, BodyInk(), ImGui::GetContentRegionAvail().x);
  KeepReadable(laid, Paper());
  const RichTextAction acted = DrawRichText(id, laid, ImGui::GetTextLineHeightWithSpacing());
  const auto link_at = [&laid](int rank) -> const RichLink* {
    return rank >= 0 && rank < static_cast<int>(laid.links.size())
               ? &laid.links[static_cast<std::size_t>(rank)]
               : nullptr;
  };
  if (const RichLink* hovered = link_at(acted.hovered); hovered != nullptr) {
    if (const std::uint32_t item_id = ItemOf(*hovered); item_id != 0) {
      action.item.hovered_id = item_id;
      action.item.hovered_kind = ChatLinkKind::kItem;
      action.item.hovered_token = 0;
      action.item.hovering = true;
    }
  }
  if (const RichLink* aimed = link_at(acted.menu); aimed != nullptr) {
    if (const std::uint32_t item_id = ItemOf(*aimed); item_id != 0) {
      item_menu = ItemLinkMenu{.armed = true,
                               .kind = ChatLinkKind::kItem,
                               .item_id = item_id,
                               .label = RichLinkText(laid, acted.menu)};
    }
  }
  const RichLink* const link = link_at(acted.followed);
  if (link == nullptr) {
    return;
  }
  // ⚠ LA CHARGE SE RECOPIE : la disposition meurt à la fin de cette image.
  switch (link->kind) {
    case RichLinkKind::kPlace:
      action.guide_to = ReadRichPlace(link->payload);
      break;
    case RichLinkKind::kItem:
      // MAJ + CLIC REPOSE LE LIEN dans la saisie, le clic seul le décrit :
      // les gestes de `HitChatLink`.
      action.item.gesture =
          acted.followed_with_shift ? ChatLinkGesture::kRepost : ChatLinkGesture::kDescribe;
      action.item.kind = ChatLinkKind::kItem;
      action.item.item_id = ItemOf(*link);
      action.item.token = 0;
      break;
    case RichLinkKind::kAddress:
      action.followed_address = link->payload;
      break;
  }
}

void DrawDescription(const QuestRowView& row, QuestWindowAction& action, PaneRequests& requests) {
  if (row.description.empty()) {
    return;
  }
  SectionHeading(core::Tr("Description"));
  DrawMarkedLines("##quete_description", row.description, action, requests.reward_menu);
}

// UNE CHASSE : son libellé — un lien vers la fiche du monstre quand il est
// nommé —, son compte à droite, puis sa barre.
void DrawHunt(const QuestHuntView& hunt, std::size_t rank, QuestWindowAction& action) {
  ImGui::PushID(static_cast<int>(rank));
  const float left = ImGui::GetCursorPosX();
  const float width = ImGui::GetContentRegionAvail().x;
  const std::string label = Owned(hunt.label);
  if (hunt.mob_id != 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextLink));
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();
    const bool hovered = ImGui::IsItemHovered();
    DecorateLink(hovered);
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      action.open_monster = hunt.mob_id;
    }
    SkinTooltip(core::Tr("Ouvrir la fiche du monstre"));
  } else {
    ImGui::TextUnformatted(label.c_str());
  }
  std::array<char, kLineBytes> count{};
  if (hunt.done) {
    std::snprintf(count.data(), count.size(), "%s", core::Tr("Terminé"));
  } else if (hunt.total > 0) {
    std::snprintf(count.data(), count.size(), "%d / %d", static_cast<int>(hunt.killed),
                  static_cast<int>(hunt.total));
  }
  if (count[0] != '\0') {
    // À droite du volet, sans jamais mordre sur un libellé long.
    const float count_width = ImGui::CalcTextSize(count.data()).x;
    const float after_label =
        left + ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(std::max(after_label, left + width - count_width));
    ColoredText(hunt.done ? ToneInk(SkinTone::kGood) : FaintInk(), count.data());
  }
  if (hunt.total > 0) {
    const ImVec2 corner = ImGui::GetCursorScreenPos();
    const Painter paint(ImGui::GetWindowDrawList(), corner, Scale());
    const float fraction = static_cast<float>(hunt.killed) / static_cast<float>(hunt.total);
    quest_paint::PaintBar(paint, 0.0F, 0.0F, width / Scale(), kHuntBarHeight, fraction,
                          PaperBar(hunt.done ? kProgressBarDone : kProgressBar));
    ImGui::Dummy(ImVec2(width, Px(kHuntBarHeight + kHuntBarGap)));
  }
  ImGui::PopID();
}

void DrawObjectives(const QuestRowView& row, QuestWindowAction& action) {
  if (row.hunts.empty()) {
    return;
  }
  SectionHeading(core::Tr("Objectifs"));
  for (std::size_t rank = 0; rank < row.hunts.size(); ++rank) {
    DrawHunt(row.hunts[rank], rank, action);
  }
}

// UNE RÉCOMPENSE D'ITEM : l'icône et le nom forment une seule zone, avec les
// gestes de tout objet — clic : description, Maj+clic : lien de chat, clic
// droit : menu.
void DrawRewardItem(const QuestRewardView& reward, std::size_t rank, QuestWindowAction& action,
                    PaneRequests& requests) {
  ImGui::PushID(static_cast<int>(rank));
  const char* const unknown = core::Tr("(item inconnu)");
  const std::string name = reward.name.empty() ? std::string(unknown) : Owned(reward.name);
  const float side = Px(kRewardIconSide);
  DrawItemLinkCell(ItemLinkCell{.item_id = reward.item_id,
                                .text = name,
                                .icon = reward.icon,
                                .icon_side = side,
                                .keep_icon_place = true},
                   requests.reward_menu, action.item);
  if (reward.count > 0) {
    std::array<char, kLineBytes> count{};
    std::snprintf(count.data(), count.size(), "\xC3\x97 %u", static_cast<unsigned>(reward.count));
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                         std::floor((side - ImGui::GetTextLineHeight()) * 0.5F));
    ColoredText(FaintInk(), count.data());
  }
  ImGui::Dummy(ImVec2(0.0F, Px(kRewardRowGap)));
  ImGui::PopID();
}

void DrawRewards(const QuestRowView& row, QuestWindowAction& action, PaneRequests& requests) {
  if (row.reward_base_exp.empty() && row.reward_job_exp.empty() && row.reward_items.empty()) {
    return;
  }
  SectionHeading(core::Tr("Récompenses"));
  std::array<char, kLineBytes> line{};
  if (!row.reward_base_exp.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Expérience de base : %s"),
                  Owned(row.reward_base_exp).c_str());
    ImGui::TextUnformatted(line.data());
  }
  if (!row.reward_job_exp.empty()) {
    std::snprintf(line.data(), line.size(), core::Tr("Expérience de job : %s"),
                  Owned(row.reward_job_exp).c_str());
    ImGui::TextUnformatted(line.data());
  }
  for (std::size_t rank = 0; rank < row.reward_items.size(); ++rank) {
    DrawRewardItem(row.reward_items[rank], rank, action, requests);
  }
}

// ── UNE AFFICHE RECOMMANDÉE ─────────────────────────────────────────────────

// L'ILLUSTRATION, à la largeur du volet sans jamais grandir au-delà de sa
// taille d'art, ni dépasser la hauteur d'une affiche.
void DrawPoster(const SkinImage& image) {
  if (!image.usable()) {
    return;
  }
  const float room = ImGui::GetContentRegionAvail().x;
  const float ratio = std::min({Scale(), room / image.width, Px(kPosterMostHeight) / image.height});
  const float shown_width = std::floor(image.width * ratio);
  const float shown_height = std::floor(image.height * ratio);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float left = origin.x + std::floor((room - shown_width) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(image.id, ImVec2(left, origin.y),
                                       ImVec2(left + shown_width, origin.y + shown_height));
  ImGui::Dummy(ImVec2(room, shown_height + Px(kBlockGap)));
}

// Les lignes d'un bloc, coupées sur ses fins de ligne.
[[nodiscard]] std::vector<std::string> LinesOf(std::string_view block) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= block.size()) {
    const std::size_t end = block.find('\n', start);
    if (end == std::string_view::npos) {
      lines.emplace_back(block.substr(start));
      break;
    }
    lines.emplace_back(block.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

void DrawRecommendedDetail(const QuestRecommendedView& entry, const SkinArt& skin,
                           PaneRequests& requests, QuestWindowAction& action) {
  DrawPoster(entry.image);
  ImGui::PushFont(nullptr, kDetailTitleSize);
  BoldText(entry.title, PaperInk(kDetailTitleText), ImGui::GetContentRegionAvail().x);
  ImGui::PopFont();
  ImGui::PushFont(nullptr, kDetailBodySize);
  ImGui::PushStyleColor(ImGuiCol_Text, BodyInk());
  if (!entry.summary.empty()) {
    ColoredText(FaintInk(), entry.summary);
  }
  ImGui::Dummy(ImVec2(0.0F, Px(kBlockGap)));
  DrawNpc(entry.npc, skin, action);
  for (std::size_t rank = 0; rank < entry.blocks.size(); ++rank) {
    ImGui::Dummy(ImVec2(0.0F, Px(kSectionGap)));
    const std::vector<std::string> lines = LinesOf(entry.blocks[rank]);
    ImGui::PushID(static_cast<int>(rank));
    DrawMarkedLines("##affiche_bloc", lines, action, requests.reward_menu);
    ImGui::PopID();
  }
  ImGui::Dummy(ImVec2(0.0F, Px(kSectionGap)));
  ColoredText(FaintInk(),
              core::Tr("Une recommandation n'est pas une quête : la lire ne vous engage à rien."));
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

}  // namespace

void DrawDetail(const QuestWindowView& view, const QuestArt& art, const SkinArt& skin,
                QuestWindowAction& action, PaneRequests& requests) {
  if (view.tab == QuestTab::kRecommended) {
    if (view.selected_recommended == nullptr) {
      DrawNothingChosen(art.detail_recommended.usable() ? art.detail_recommended : art.detail_empty,
                        core::Tr("Choisissez une recommandation."));
    } else {
      DrawRecommendedDetail(*view.selected_recommended, skin, requests, action);
    }
    return;
  }
  if (view.selected == nullptr) {
    DrawNothingChosen(art.detail_empty, core::Tr("Choisissez une quête."));
    return;
  }
  const QuestRowView& row = *view.selected;
  ImGui::PushFont(nullptr, kDetailTitleSize);
  BoldText(row.title, PaperInk(kDetailTitleText), ImGui::GetContentRegionAvail().x);
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0.0F, Px(kBlockGap)));
  ImGui::PushFont(nullptr, kDetailBodySize);
  ImGui::PushStyleColor(ImGuiCol_Text, BodyInk());
  // UNE QUÊTE SORTIE DU JOURNAL n'a plus d'état de jeu : ni actif, ni
  // échéance, ni PNJ à guider.
  if (view.tab != QuestTab::kHistory) {
    DrawStatus(row);
    DrawDeadline(row);
    DrawNpc(row.npc, skin, action);
  }
  if (row.unknown) {
    ColoredText(FaintInk(), core::Tr("Cette quête n'est pas dans le catalogue."));
  }
  DrawDescription(row, action, requests);
  DrawObjectives(row, action);
  DrawRewards(row, action, requests);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

}  // namespace ro::ui::quest_window
