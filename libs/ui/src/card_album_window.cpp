// L'ALBUM DE CARTES : la fenêtre, son en-tête, ses intercalaires et ses
// dialogues. Le classeur lui-même est dans `card_album_book.cpp`.

#include "ro/ui/card_album_window.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "card_album_parts.h"
#include "item_grid.h"

namespace ro::ui {
namespace card_album {

void ArtSize(float* width, float* height) {
  *width = std::floor(Px(kCardAlbumArtWidthPixels) + 0.5F);
  *height = std::floor(Px(kCardAlbumArtHeightPixels) + 0.5F);
}

ImU32 WithAlpha(ImU32 color, float alpha) {
  const unsigned current = (color >> IM_COL32_A_SHIFT) & 0xFFU;
  const auto scaled =
      static_cast<unsigned>(static_cast<float>(current) * std::clamp(alpha, 0.0F, 1.0F) + 0.5F);
  return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

bool PassesNature(const CardAlbumWindowState& state, const CardAlbumView& view,
                  const CardAlbumPocketView& pocket) {
  if (!view.dropper_known || state.nature_mask == 0) {
    return true;
  }
  // Une nature inconnue (serveur plus neuf que nous) ne tient dans aucun bit.
  return pocket.dropper_rank < kNatureFilters.size() &&
         (state.nature_mask & (1U << pocket.dropper_rank)) != 0;
}

void Flip(CardAlbumWindowState& state, int delta) {
  if (state.per_spread <= 0 || delta == 0) {
    return;
  }
  const int total = static_cast<int>(state.order.size());
  const int spreads = std::max(1, (total + state.per_spread - 1) / state.per_spread);
  const int page = std::clamp(state.first / state.per_spread + delta, 0, spreads - 1);
  const int first = page * state.per_spread;
  if (first != state.first) {
    state.first = first;
    state.flip_time = ImGui::GetTime();
  }
}

void ArmMove(CardAlbumWindowState& state, CardAlbumVerb verb, std::uint16_t slot,
             std::uint32_t card_id, int most, bool ask_quantity, CardAlbumWindowAction& action) {
  if (most <= 0) {
    return;
  }
  // Une pile de 1 n'a pas de quantité à choisir : l'invite serait un clic de
  // plus pour une seule réponse possible.
  if (ask_quantity && most > 1) {
    state.quantity = CardAlbumQuantityAsk{
        .verb = verb, .slot = slot, .card_id = card_id, .most = most, .amount = most};
    state.quantity_asked = true;
    return;
  }
  action.verb = verb;
  action.slot = slot;
  action.card_id = card_id;
  action.amount = most;
}

void AskSacrifice(CardAlbumWindowState& state, std::uint16_t slot, std::uint32_t card_id, int rest,
                  CardAlbumWindowAction& action) {
  if (slot == 0 || card_id == 0) {
    return;
  }
  if (state.look.auto_sacrifice) {
    // Le joueur a choisi de ne plus être interrogé : la première copie part
    // tout de suite. Le compte rendu dit ensuite ce qui vient de se passer.
    action.verb = CardAlbumVerb::kSacrifice;
    action.slot = slot;
    action.card_id = card_id;
    action.amount = 1;
    action.rest = std::max(0, rest);
    return;
  }
  // 🔴 Sinon, jamais sans confirmation : la carte est consommée pour de bon.
  state.confirm_slot = slot;
  state.confirm_card = card_id;
  state.confirm_rest = std::max(0, rest);
  state.confirm_asked = true;
}

void OfferFromHand(CardAlbumWindowState& state, const CardAlbumView& view, std::uint16_t slot,
                   CardAlbumWindowAction& action) {
  // Le catalogue n'est pas encore là : le demander. Sans lui, rien ne dit si la
  // pochette est ouverte.
  if (view.pockets.empty()) {
    action.verb = CardAlbumVerb::kRefresh;
    return;
  }
  const CardAlbumHandCard* held = nullptr;
  for (const CardAlbumHandCard& card : view.hand) {
    if (card.client_slot == slot) {
      held = &card;
      break;
    }
  }
  const CardAlbumPocketView* pocket = nullptr;
  if (held != nullptr) {
    for (const CardAlbumPocketView& p : view.pockets) {
      if (p.card_id == held->card_id) {
        pocket = &p;
        break;
      }
    }
  }
  // 🔴 LE CATALOGUE PORTE TOUTES LES CARTES QUE L'ALBUM ACCEPTE : ce qui n'y est
  // pas n'est pas une carte de monstre — un enchant est rangé en carte, lui
  // aussi. Le refuser ici évite une confirmation de sacrifice pour un objet que
  // le serveur refusera de toute façon.
  if (pocket == nullptr) {
    action.refused_not_a_card = true;
    return;
  }
  if (pocket->unlocked) {
    // LA PILE ENTIÈRE, sans demander : offrir une carte à l'album, c'est la
    // ranger. La quantité partielle reste au menu de la pochette.
    ArmMove(state, CardAlbumVerb::kDeposit, slot, held->card_id, held->amount, false, action);
    return;
  }
  // Pochette scellée : le sacrifice, par son unique porte — et le RESTE de la
  // pile suit dès qu'elle est ouverte.
  AskSacrifice(state, slot, held->card_id, held->amount - 1, action);
}

const CardAlbumPocketView* PocketOf(const CardAlbumView& view, std::uint32_t card_id) {
  for (const CardAlbumPocketView& pocket : view.pockets) {
    if (pocket.card_id == card_id) {
      return &pocket;
    }
  }
  return nullptr;
}

}  // namespace card_album

namespace {

using namespace card_album;

[[nodiscard]] bool MatchesFilter(const CardAlbumWindowState& state,
                                 const CardAlbumPocketView& pocket) {
  const std::string_view typed(state.filter);
  if (typed.empty()) {
    return true;
  }
  if (!pocket.name.empty() && core::AsciiContainsFolded(pocket.name, typed)) {
    return true;
  }
  return core::AsciiContainsFolded(std::to_string(pocket.card_id), typed);
}

[[nodiscard]] std::uint8_t SlotMask(int slot_filter) {
  return slot_filter > 0 && slot_filter < static_cast<int>(kSlotFilters.size())
             ? kSlotFilters[static_cast<std::size_t>(slot_filter)].family
             : 0;
}

void RebuildOrder(CardAlbumWindowState& state, const CardAlbumView& view) {
  state.order_dirty = false;
  state.order_for = view.revision;
  state.order.clear();
  state.order.reserve(view.pockets.size());
  const std::uint8_t mask = SlotMask(state.slot_filter);
  state.family_total = 0;
  state.family_unlocked = 0;
  for (std::size_t rank = 0; rank < view.pockets.size(); ++rank) {
    const CardAlbumPocketView& pocket = view.pockets[rank];
    if (mask != 0 && (pocket.families & mask) == 0) {
      continue;
    }
    if (!PassesNature(state, view, pocket)) {
      continue;
    }
    // La complétion se mesure AVANT la recherche et le filtre « débloquées » :
    // c'est la collection, pas la vue.
    ++state.family_total;
    if (pocket.unlocked) {
      ++state.family_unlocked;
    }
    if (state.show_filter == kShowUnlocked && !pocket.unlocked) {
      continue;
    }
    if (state.show_filter == kShowSealed && pocket.unlocked) {
      continue;
    }
    // « Hors des albums » est la question qui a motivé ce filtre : ces cartes-là,
    // aucun des deux albums ne les rend, il faut aller les chercher sur le monstre.
    if (state.show_filter >= kShowFirstSource) {
      bool anywhere = false;
      for (const CardAlbumSourceMark& mark : pocket.sources) {
        anywhere = anywhere || mark.present;
      }
      if (state.show_filter == kShowNoSource) {
        if (anywhere) {
          continue;
        }
      } else if (!pocket.sources[static_cast<std::size_t>(state.show_filter - kShowFirstSource)]
                      .present) {
        continue;
      }
    }
    if (!MatchesFilter(state, pocket)) {
      continue;
    }
    state.order.push_back(static_cast<int>(rank));
  }
  const int mode = state.sort_mode;
  const std::span<const CardAlbumPocketView> pockets = view.pockets;
  std::stable_sort(state.order.begin(), state.order.end(), [mode, pockets](int a, int b) {
    const CardAlbumPocketView& left = pockets[static_cast<std::size_t>(a)];
    const CardAlbumPocketView& right = pockets[static_cast<std::size_t>(b)];
    int compared = 0;
    switch (mode) {
      case kSortName:
        compared = left.name.compare(right.name);
        break;
      case kSortReserve:
        // Les mieux fournies d'abord ; une pochette scellée passe après toutes
        // les ouvertes.
        compared = left.unlocked != right.unlocked ? (left.unlocked ? -1 : 1)
                   : left.reserve > right.reserve  ? -1
                   : left.reserve < right.reserve  ? 1
                                                   : 0;
        break;
      case kSortSlot:
        compared = left.slot_label.compare(right.slot_label);
        break;
      default:
        break;  // le catalogue : l'ordre du serveur
    }
    // Départage par identifiant : sans lui, deux cartes de même réserve
    // changeraient de place d'une réponse à l'autre.
    if (compared == 0) {
      compared = left.card_id < right.card_id ? -1 : (left.card_id > right.card_id ? 1 : 0);
    }
    return compared < 0;
  });
  // ⚠ PAS DE RETOUR EN PAGE 1 ICI : l'ordre se refait aussi à chaque réponse du
  // serveur, et un dépôt ne doit pas renvoyer le joueur au début du classeur.
  // C'est un changement de VUE qui ramène en page 1, et ses widgets le font.
}

void ChangeView(CardAlbumWindowState& state) {
  state.order_dirty = true;
  state.first = 0;
}

// LA POCHETTE DEMANDÉE DOIT ÊTRE DANS LES PAGES : sinon le saut, qui ne
// cherche que dans l'ordre affiché, ne la trouverait pas. Les filtres se lèvent
// alors tous — tri compris, il ne cache rien —, et l'ordre se refait.
//
// ⚠ AVANT LE SAUT, QUI CONSOMME LE NUMÉRO DE SÉRIE : c'est lui qui tournera les
// pages, dans l'ordre refait.
void RevealHighlight(CardAlbumWindowState& state, const CardAlbumView& view) {
  if (!view.highlight_reveal || view.highlight_serial == state.highlight_seen ||
      view.highlight_card == 0 || PocketOf(view, view.highlight_card) == nullptr) {
    return;
  }
  for (const int rank : state.order) {
    if (view.pockets[static_cast<std::size_t>(rank)].card_id == view.highlight_card) {
      return;
    }
  }
  state.filter[0] = '\0';
  state.show_filter = kShowAll;
  state.slot_tab_asked = state.slot_filter != 0;
  state.slot_filter = 0;
  state.nature_mask = 0;
  RebuildOrder(state, view);
}

void DrawHeader(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
                CardAlbumWindowAction& action) {
  constexpr float kFilterPixels = 215.0F;
  constexpr float kSortPixels = 120.0F;
  constexpr float kShowPixels = 150.0F;
  // Assez large pour que l'aide tienne en entier : c'est elle qui apprend que
  // l'identifiant marche aussi.
  ImGui::SetNextItemWidth(Px(kFilterPixels));
  if (ImGui::InputTextWithHint("##album_filtre", core::Tr("Rechercher une carte : nom ou id"),
                               state.filter, sizeof(state.filter))) {
    ChangeView(state);
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(core::Tr("Ordre"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kSortPixels));
  if (SkinCombo("##album_ordre", &state.sort_mode, kSortLabels, art)) {
    ChangeView(state);
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(core::Tr("Afficher"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kShowPixels));
  if (SkinCombo("##album_afficher", &state.show_filter, kShowLabels, art)) {
    ChangeView(state);
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Rafraîchir"), art)) {
    action.verb = CardAlbumVerb::kRefresh;
  }
  // Le dernier geste du STAFF, à part du compte rendu du serveur.
  if (!view.staff_notice.empty()) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, view.staff_notice_ok ? kWarningColor : kRefusalColor);
    ImGui::TextUnformatted(view.staff_notice.data(),
                           view.staff_notice.data() + view.staff_notice.size());
    ImGui::PopStyleColor();
  }
  // 🔴 LE SUCCÈS S'AFFICHE AUTANT QUE L'ÉCHEC. Pour un sacrifice, ne dire que
  // la carte perdue tairait ce qu'on a gagné ; un refus muet, lui, ferait croire
  // à un défaut du client.
  if (!view.notice.empty()) {
    ImGui::SameLine();
    const ImVec4 tone = view.notice_tone == CardAlbumTone::kDone      ? kDoneColor
                        : view.notice_tone == CardAlbumTone::kWarning ? kWarningColor
                                                                      : kRefusalColor;
    ImGui::PushStyleColor(ImGuiCol_Text, tone);
    ImGui::TextUnformatted(view.notice.data(), view.notice.data() + view.notice.size());
    ImGui::PopStyleColor();
  }
}

// Combien de cartes d'une famille sont acquises : l'emplacement `mask`, et la
// nature `rank` si elle est donnée.
struct Tally {
  int total = 0;
  int unlocked = 0;
};

[[nodiscard]] Tally CountFamily(const CardAlbumWindowState& state, const CardAlbumView& view,
                                std::uint8_t mask, int only_rank) {
  Tally tally;
  for (const CardAlbumPocketView& pocket : view.pockets) {
    if (mask != 0 && (pocket.families & mask) == 0) {
      continue;
    }
    if (only_rank >= 0 ? pocket.dropper_rank != only_rank : !PassesNature(state, view, pocket)) {
      continue;
    }
    ++tally.total;
    if (pocket.unlocked) {
      ++tally.unlocked;
    }
  }
  return tally;
}

// Les intercalaires, et à leur droite les boutons de NATURE, qui se combinent
// avec eux — un onglet de plus aurait forcé à choisir entre emplacement et
// nature.
void DrawTabs(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art) {
  const ImVec2 row_top = ImGui::GetCursorScreenPos();
  float tabs_right = row_top.x;
  float tab_height = ImGui::GetFrameHeight();
  if (!BeginSkinTabBar("##album_intercalaires", art)) {
    return;
  }
  const bool forced = std::exchange(state.slot_tab_asked, false);
  for (std::size_t rank = 0; rank < kSlotFilters.size(); ++rank) {
    const char* label = rank == 0 ? core::Tr("Tous") : kSlotFilters[rank].label;
    const ImGuiTabItemFlags flags =
        forced && state.slot_filter == static_cast<int>(rank) ? ImGuiTabItemFlags_SetSelected : 0;
    if (ImGui::BeginTabItem(label, nullptr, flags)) {
      if (!forced && state.slot_filter != static_cast<int>(rank)) {
        state.slot_filter = static_cast<int>(rank);
        ChangeView(state);
      }
      ImGui::EndTabItem();
    }
    tabs_right = ImGui::GetItemRectMax().x;
    tab_height = ImGui::GetItemRectSize().y;
    if (ImGui::IsItemHovered() && !view.pockets.empty()) {
      const Tally tally = CountFamily(state, view, kSlotFilters[rank].family, -1);
      SkinTooltipFormat(core::Tr("%d / %d cartes acquises"), tally.unlocked, tally.total);
    }
  }
  EndSkinTabBar();
  if (!view.dropper_known) {
    return;
  }
  // Calés à droite de la rangée des onglets s'il y a la place, sinon sur leur
  // propre ligne dessous — jamais par-dessus un onglet.
  const ImVec2 after = ImGui::GetCursorScreenPos();
  const float right = after.x + ImGui::GetContentRegionAvail().x;
  const float gap = Px(kNatureGapPixels);
  float width = gap * static_cast<float>(kNatureFilters.size() - 1);
  for (const NatureFilter& nature : kNatureFilters) {
    width += SkinSmallButtonWidth(nature.label, art);
  }
  const bool same_row = right - width >= tabs_right + Px(kNatureLeastGapPixels);
  const float button_height = same_row ? tab_height - 2.0F * Px(kNatureInsetPixels) : 0.0F;
  ImGui::SetCursorScreenPos(
      ImVec2(right - width, same_row ? row_top.y + Px(kNatureInsetPixels) : after.y));
  for (std::size_t rank = 0; rank < kNatureFilters.size(); ++rank) {
    const NatureFilter& nature = kNatureFilters[rank];
    const auto bit = static_cast<std::uint8_t>(1U << nature.rank);
    if (rank > 0) {
      ImGui::SameLine(0.0F, gap);
    }
    ImGui::PushID(static_cast<int>(rank));
    if (SkinSmallToggleButton(nature.label, (state.nature_mask & bit) != 0, art,
                              ImVec2(0.0F, button_height))) {
      state.nature_mask ^= bit;
      // Les trois enclenchés valent aucun : on revient à l'état neutre, sinon
      // plus aucun bouton ne paraîtrait relâché alors que rien n'est filtré.
      if (state.nature_mask == kNatureAll) {
        state.nature_mask = 0;
      }
      ChangeView(state);
    }
    ImGui::PopID();
    if (ImGui::IsItemHovered() && !view.pockets.empty()) {
      const Tally tally = CountFamily(state, view, SlotMask(state.slot_filter), nature.rank);
      SkinTooltipFormat(core::Tr("%d / %d cartes acquises\nClic : filtrer (cumulable)"),
                        tally.unlocked, tally.total);
    }
  }
  if (same_row) {
    ImGui::SetCursorScreenPos(after);
  }
}

// La barre de complétion de la famille affichée, et le total en réserve.
void DrawCompletion(const CardAlbumWindowState& state, const CardAlbumView& view) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 top = ImGui::GetCursorScreenPos();
  const float width = ImGui::GetContentRegionAvail().x;
  const float line = ImGui::GetTextLineHeight();
  const float bar = Px(kBarPixels);
  constexpr float kBarGapPixels = 3.0F;
  constexpr float kAfterBarPixels = 4.0F;
  constexpr float kPercent = 100.0F;

  // LA FAMILLE MESURÉE : l'intercalaire, puis les natures retenues (« Armor ·
  // Boss + MVP »). Sans elle, la barre d'un filtre MVP se lirait comme celle
  // de toute la collection.
  std::string family;
  if (state.slot_filter != 0) {
    family = kSlotFilters[static_cast<std::size_t>(state.slot_filter)].label;
  }
  if (view.dropper_known && state.nature_mask != 0) {
    bool first = true;
    for (const NatureFilter& nature : kNatureFilters) {
      if ((state.nature_mask & (1U << nature.rank)) == 0) {
        continue;
      }
      family += first ? (family.empty() ? "" : " \xC2\xB7 ") : " + ";
      family += nature.label;
      first = false;
    }
  }
  const float percent = state.family_total > 0
                            ? kPercent * static_cast<float>(state.family_unlocked) /
                                  static_cast<float>(state.family_total)
                            : 0.0F;
  std::array<char, 192> left = {};
  if (family.empty()) {
    std::snprintf(left.data(), left.size(), core::Tr("%d / %d cartes  ·  %.1f %%"),
                  state.family_unlocked, state.family_total, static_cast<double>(percent));
  } else {
    std::snprintf(left.data(), left.size(), core::Tr("%s : %d / %d  ·  %.1f %%"), family.c_str(),
                  state.family_unlocked, state.family_total, static_cast<double>(percent));
  }
  std::array<char, 96> right = {};
  std::snprintf(right.data(), right.size(), core::Tr("En réserve : %lld"),
                static_cast<long long>(view.total_reserve));
  into->AddText(top, ImGui::GetColorU32(ImGuiCol_Text), left.data());
  const float right_width = ImGui::CalcTextSize(right.data()).x;
  into->AddText(ImVec2(top.x + width - right_width, top.y),
                ImGui::ColorConvertFloat4ToU32(kLabelColor), right.data());

  // La barre sous les chiffres : dorée comme les pastilles de réserve.
  const float bar_top = top.y + line + Px(kBarGapPixels);
  const float filled = state.family_total > 0 ? width * static_cast<float>(state.family_unlocked) /
                                                    static_cast<float>(state.family_total)
                                              : 0.0F;
  const float round = bar * 0.5F;
  into->AddRectFilled(ImVec2(top.x, bar_top), ImVec2(top.x + width, bar_top + bar), kBarTrack,
                      round);
  if (filled > 0.0F) {
    into->AddRectFilled(ImVec2(top.x, bar_top),
                        ImVec2(top.x + std::max(filled, bar), bar_top + bar), kBarFill, round);
  }
  into->AddRect(ImVec2(top.x, bar_top), ImVec2(top.x + width, bar_top + bar), kBarEdge, round);
  ImGui::Dummy(ImVec2(width, line + Px(kBarGapPixels) + bar + Px(kAfterBarPixels)));
}

// Le calage par pochette entière : la taille demandée se ramène sur un nombre
// entier de pochettes, en largeur (les DEUX pages gagnent une colonne ensemble)
// comme en hauteur. `UserData` = l'état de la fenêtre.
void SnapToPockets(ImGuiSizeCallbackData* asked) {
  const auto* state = static_cast<const CardAlbumWindowState*>(asked->UserData);
  if (state == nullptr || !state->snap_ready) {
    return;
  }
  const float gap = Px(kPocketGapPixels);
  const float step_x = state->cell_step.x;
  const float step_y = state->cell_step.y;
  const float grid_width = (asked->DesiredSize.x - state->chrome.x) * 0.5F;
  const float grid_height = asked->DesiredSize.y - state->chrome.y;
  const int columns = std::max(kLeastColumns, static_cast<int>((grid_width + gap) / step_x + 0.5F));
  const int rows = std::max(kLeastRows, static_cast<int>((grid_height + gap) / step_y + 0.5F));
  asked->DesiredSize.x = state->chrome.x + 2.0F * (static_cast<float>(columns) * step_x - gap);
  asked->DesiredSize.y = state->chrome.y + static_cast<float>(rows) * step_y - gap;
}

}  // namespace

std::string CardAlbumShortName(std::string_view full_name) {
  if (full_name.empty()) {
    return "(?)";
  }
  // 1. L'emplacement, au premier crochet, avec les espaces qui le précèdent :
  //    l'intercalaire et l'infobulle le disent déjà.
  std::size_t length = full_name.find('[');
  if (length == std::string_view::npos) {
    length = full_name.size();
  }
  while (length > 0 && full_name[length - 1] == ' ') {
    --length;
  }
  if (length == 0) {
    length = full_name.size();  // « [Armor] » seul : mieux vaut tout que rien
  }
  // 2. Le suffixe « Card », que TOUTES portent : dans un album de cartes il ne
  //    distingue rien, et il vole cinq caractères aux noms longs.
  constexpr std::string_view kSuffix = " Card";
  if (length > kSuffix.size()) {
    const std::string_view tail = full_name.substr(length - kSuffix.size(), kSuffix.size());
    if (std::equal(tail.begin(), tail.end(), kSuffix.begin(), kSuffix.end(), [](char a, char b) {
          return std::tolower(static_cast<unsigned char>(a)) ==
                 std::tolower(static_cast<unsigned char>(b));
        })) {
      length -= kSuffix.size();
    }
  }
  return std::string(full_name.substr(0, length));
}

CardAlbumWindowAction DrawCardAlbumWindow(CardAlbumWindowState& state, const CardAlbumView& view,
                                          const SkinArt& art) {
  CardAlbumWindowAction action;
  state.wanted.clear();
  if (!state.open) {
    state.offered_slot = 0;
    state.withdraw_asked = 0;
    // 🔴 FERMÉE D'AILLEURS — Échap, le raccourci — ELLE REND AUSSI LE VERROU :
    // tant qu'on le tient, aucun autre compte de jeu du même compte Moonlight
    // n'ouvre son album.
    if (state.shown) {
      state.shown = false;
      action.verb = CardAlbumVerb::kClose;
    }
    return action;
  }
  // Les demandes venues du sac à l'image précédente.
  if (state.offered_slot != 0) {
    OfferFromHand(state, view, state.offered_slot, action);
    state.offered_slot = 0;
  }
  if (state.withdraw_asked != 0) {
    if (const CardAlbumPocketView* pocket = PocketOf(view, state.withdraw_asked);
        pocket != nullptr && pocket->unlocked && pocket->reserve > 0) {
      ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket->card_id, pocket->reserve, true, action);
    }
    state.withdraw_asked = 0;
  }
  if (state.order_dirty || state.order_for != view.revision) {
    RebuildOrder(state, view);
  }
  RevealHighlight(state, view);

  // Le titre porte la complétion. ⚠ `###` FIGE L'IDENTITÉ : sans lui, la
  // fenêtre perdrait sa place et sa taille à chaque carte débloquée.
  const int total = static_cast<int>(view.pockets.size());
  const std::size_t language = core::LanguageSerial();
  if (state.title.empty() || state.title_unlocked != view.unlocked_count ||
      state.title_total != total || state.title_language != language) {
    state.title_unlocked = view.unlocked_count;
    state.title_total = total;
    state.title_language = language;
    std::array<char, 128> shown = {};
    std::snprintf(shown.data(), shown.size(), "%s  %d / %d###%s", core::Tr("Album de cartes"),
                  view.unlocked_count, total, kCardAlbumWindowId);
    state.title = shown.data();
  }
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  if (state.snap_ready) {
    const float gap = Px(kPocketGapPixels);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(state.chrome.x + 2.0F * (kLeastColumns * state.cell_step.x - gap),
               state.chrome.y + kLeastRows * state.cell_step.y - gap),
        ImVec2(FLT_MAX, FLT_MAX), SnapToPockets, &state);
  } else {
    ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                        ImVec2(FLT_MAX, FLT_MAX));
  }
  // La puce de la barre de titre mène aux réglages de CETTE fenêtre.
  SetNextWindowBulletClickable(core::Tr("Options de l'album"));
  PoseBugReportButton(state.bug_report);
  state.shown = true;
  if (BeginSkinnedWindow(state.title.c_str(), &state.open, art)) {
    if (TitleBulletClicked()) {
      action.settings_asked = true;
    }
    action.report_bug = state.bug_report && TitleActionClicked(0);
    DrawHeader(state, view, art, action);
    DrawTabs(state, view, art);
    if (state.order_dirty) {
      RebuildOrder(state, view);
    }
    DrawCompletion(state, view);
    constexpr float kLeastBookPixels = 160.0F;
    DrawBook(state, view, art, std::max(Px(kLeastBookPixels), ImGui::GetContentRegionAvail().y),
             action);
    // Déclarés au niveau de la FENÊTRE, jamais dans la boucle de pochettes.
    DrawDialogs(state, view, art, action);
  }
  EndSkinnedWindow();
  // La croix : rendre le verrou au serveur.
  if (!state.open) {
    state.shown = false;
    action.verb = CardAlbumVerb::kClose;
  }
  return action;
}

}  // namespace ro::ui
