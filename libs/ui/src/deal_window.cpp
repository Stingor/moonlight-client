#include "ro/ui/deal_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "item_grid.h"
#include "ui_popup.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LES MESURES ─────────────────────────────────────────────────────────────
//
// Celles de la fenêtre d'échange, élargies de la ligne de zeny que
// chaque colonne porte sous elle.
constexpr float kFirstLeftPixels = 140.0F;
constexpr float kFirstTopPixels = 120.0F;
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 520.0F;
constexpr float kColumnGapPixels = 8.0F;
constexpr float kColumnHeightPixels = 170.0F;
constexpr float kPickHeightPixels = 150.0F;
constexpr float kZenyFieldPixels = 130.0F;
constexpr float kActionButtonPixels = 130.0F;
constexpr float kChildRoundingPixels = 6.0F;
// Le zeny du partenaire s'écrit plus gros que le reste : un zéro de trop doit
// sauter aux yeux.
constexpr float kPartnerZenyFontScale = 1.4F;

// Une ligne arrivée ou grossie après mon verrou, et ce qui alerte.
constexpr ImVec4 kChangedText = ImVec4(0.85F, 0.45F, 0.05F, 1.0F);
constexpr ImVec4 kAlertText = kTradeRefusedText;
constexpr ImVec4 kLockedText = kTradeDoneText;

// La place d'un montant tapé : le plafond des saisies, séparateurs compris.
constexpr std::size_t kZenyTextBytes = 32;
// La place d'une phrase composée d'un nom de personnage.
constexpr std::size_t kSentenceBytes = 256;

constexpr const char* kConfirmId = "##deal_confirm";
constexpr const char* kItemMenuId = "##deal_item_menu";

// LE JETON DU MENU D'UN ITEM : d'où il vient, puis son rang dans sa colonne —
// ou sa case, pour une pile du sac à offrir.
enum class MenuSource : std::uint32_t { kMine = 0, kPartner = 1, kBag = 2 };
constexpr std::uint32_t kMenuSourceCount = 3;

[[nodiscard]] std::uint32_t MenuToken(MenuSource source, std::uint32_t rank) {
  return rank * kMenuSourceCount + static_cast<std::uint32_t>(source);
}

// Le choix du menu, rendu dans les termes de l'action.
void NoteMenuChoice(const ChatLinkAsk& chosen, DealWindowAction& action) {
  if (chosen.gesture == ChatLinkGesture::kNothing) {
    return;
  }
  const auto source = static_cast<MenuSource>(chosen.token % kMenuSourceCount);
  const std::uint32_t rank = chosen.token / kMenuSourceCount;
  if (source == MenuSource::kBag) {
    action.described_bag_slot = static_cast<std::uint16_t>(rank);
  } else {
    action.described =
        DealLineRef{.partner = source == MenuSource::kPartner, .index = static_cast<int>(rank)};
  }
  action.item_gesture = chosen.gesture;
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && text.front() == ' ') {
    text.remove_prefix(1);
  }
  while (!text.empty() && text.back() == ' ') {
    text.remove_suffix(1);
  }
  return text;
}

[[nodiscard]] std::string_view PartnerName(const DealView& view) {
  return view.partner_name.empty() ? std::string_view(core::Tr("l'autre joueur"))
                                   : view.partner_name;
}

[[nodiscard]] const DealBagView* FindBag(std::span<const DealBagView> bag,
                                         std::uint16_t client_slot) {
  const auto found = std::ranges::find_if(
      bag, [client_slot](const DealBagView& on) { return on.client_slot == client_slot; });
  return found == bag.end() ? nullptr : &*found;
}

// « 1,000,000z (1 million) » : les chiffres pour la lecture, les lettres pour
// le zéro de trop.
[[nodiscard]] std::string ZenyWithWords(std::int64_t zeny) {
  return TradeAmountText(zeny, "z") + " (" + SpelledTradeAmount(zeny) + ")";
}

[[nodiscard]] std::string CountSuffix(int amount) {
  return amount > 1 ? "x" + TradeAmountText(amount, {}) : std::string();
}

void TextOf(ImVec4 color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
  ImGui::PopStyleColor();
}

// Les raisons qui retiennent un bouton, une par ligne, pour son infobulle.
[[nodiscard]] std::string Joined(std::span<const std::string_view> reasons) {
  std::string joined;
  for (const std::string_view reason : reasons) {
    if (!joined.empty()) {
      joined.push_back('\n');
    }
    joined.append(reason);
  }
  return joined;
}

void TipWhenHeld(std::span<const std::string_view> reasons) {
  if (!reasons.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(Joined(reasons).c_str());
  }
}

// Le recap d'un côté : chaque ligne, puis le zeny ; « rien » quand il n'y a ni
// l'un ni l'autre.
void AppendSide(std::string& text, std::span<const DealLineView> lines, std::int64_t zeny) {
  for (const DealLineView& line : lines) {
    text.append("\n   ");
    text.append(line.item.name.empty() ? std::string_view("?") : line.item.name);
    text.append(" x ");
    text.append(TradeAmountText(line.amount, {}));
  }
  if (zeny > 0) {
    text.append("\n   ");
    text.append(ZenyWithWords(zeny));
  }
  if (lines.empty() && zeny <= 0) {
    text.append("\n   ");
    text.append(core::Tr("rien"));
  }
}

// ── LES COLONNES ────────────────────────────────────────────────────────────

void NoteGesture(const TradeItemGesture& gesture, const TradeItemView& item, DealLineRef ref,
                 ItemLinkMenu& menu, DealWindowAction& action) {
  if (gesture.hovered) {
    action.hovered = ref;
  }
  const MenuSource source = ref.partner ? MenuSource::kPartner : MenuSource::kMine;
  if (NoteTradeItemLink(gesture, item, MenuToken(source, static_cast<std::uint32_t>(ref.index)),
                        menu)) {
    action.described = ref;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

void DrawLine(const DealLineView& line, DealLineRef ref, ItemLinkMenu& menu,
              DealWindowAction& action) {
  ImGui::PushID(ref.index);
  // UN DÉPÔT EN ROUTE SE GRISE sans se désactiver : il se survole et se décrit
  // comme les autres.
  if (line.in_flight) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                        ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
  }
  if (line.changed_after_my_lock) {
    ImGui::PushStyleColor(ImGuiCol_Text, kChangedText);
  }
  NoteGesture(DrawTradeItem(line.item, CountSuffix(line.amount)), line.item, ref, menu, action);
  if (line.changed_after_my_lock) {
    ImGui::SameLine();
    ImGui::TextUnformatted(core::Tr("nouveau"));
    ImGui::PopStyleColor();
  }
  if (line.in_flight) {
    ImGui::SameLine();
    ImGui::TextUnformatted(core::Tr("en route…"));
    ImGui::PopStyleVar();
  }
  if (!line.note.empty()) {
    ImGui::Indent();
    ImGui::TextDisabled("%.*s", static_cast<int>(line.note.size()), line.note.data());
    ImGui::Unindent();
  }
  ImGui::PopID();
}

void DrawLamp(bool locked, bool sent) {
  if (locked) {
    ImGui::TextColored(kLockedText, "%s", core::Tr("Verrouillé"));
  } else if (sent) {
    ImGui::TextDisabled("%s", core::Tr("Verrouillage…"));
  } else {
    ImGui::TextDisabled("%s", core::Tr("En cours"));
  }
}

// UN ITEM LÂCHÉ SUR MA COLONNE : seul le sac en donne, et pas ce qu'on porte.
void AcceptBagDrop(DealWindowState& state, const DealView& view, DealWindowAction& action) {
  if (!ImGui::BeginDragDropTarget()) {
    return;
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && dropped->DataSize == sizeof(ItemDrag)) {
    ItemDrag carried;
    std::memcpy(&carried, dropped->Data, sizeof(carried));
    const DealBagView* bag = FindBag(view.bag, carried.client_slot);
    if (carried.from != ItemHolder::kInventory || carried.worn) {
      state.flash = core::Tr("Seul un item de l'inventory, non porté, peut s'offrir.");
    } else if (bag == nullptr) {
      state.flash = core::Tr("Cet item ne peut pas s'offrir.");
    } else {
      ChooseDealBagSlot(state, *bag, action);
      state.asking_by_drop = true;
    }
  }
  ImGui::EndDragDropTarget();
}

void DrawMyZeny(DealWindowState& state, const DealView& view, const SkinArt& art,
                DealWindowAction& action) {
  const float scale = Scale();
  const DealZenyEntry typed = ReadDealZeny(state.zeny_text, view.purse);
  if (view.lock_sent) {
    ImGui::BeginDisabled();
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Zeny :"));
  ImGui::SameLine();
  std::array<char, kZenyTextBytes> buffer{};
  state.zeny_text.copy(buffer.data(), buffer.size() - 1);
  ImGui::SetNextItemWidth(kZenyFieldPixels * scale);
  const bool entered = ImGui::InputTextWithHint("##deal_zeny", "0", buffer.data(), buffer.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
  state.zeny_text = buffer.data();
  ImGui::SameLine();
  // « PROPOSER » MONTRE LE ZENY AVANT LE VERROU : le partenaire le voit, et la
  // ligne de dessous dit ce qui est réellement offert.
  const bool held = view.lock_sent || !typed.amount.has_value() || *typed.amount == view.my_zeny;
  if (held && !view.lock_sent) {
    ImGui::BeginDisabled();
  }
  const bool proposed = SkinSmallButton(core::Tr("Proposer"), art);
  if (held && !view.lock_sent) {
    ImGui::EndDisabled();
  }
  if (view.lock_sent) {
    ImGui::EndDisabled();
  }
  if (!held && (proposed || entered)) {
    action.zeny = *typed.amount;
  }
  if (!typed.refusal.empty()) {
    TextOf(kAlertText, typed.refusal);
  }
  const std::string offered = TradeAmountText(view.my_zeny, "z");
  ImGui::TextDisabled(core::Tr("Offert : %s"), offered.c_str());
}

void DrawMine(DealWindowState& state, const DealView& view, const SkinArt& art, float width,
              DealWindowAction& action) {
  ImGui::BeginGroup();
  ImGui::TextUnformatted(core::Tr("Mon offre"));
  ImGui::BeginChild("##deal_mine", ImVec2(width, kColumnHeightPixels * Scale()),
                    ImGuiChildFlags_Borders);
  for (std::size_t index = 0; index < view.mine.size(); ++index) {
    DrawLine(view.mine[index], DealLineRef{.partner = false, .index = static_cast<int>(index)},
             state.item_menu, action);
  }
  if (view.mine.empty()) {
    ImGui::TextDisabled(
        "%s", view.lock_sent ? core::Tr("(vide)") : core::Tr("Glissez un item de l'inventory ici"));
  }
  ImGui::EndChild();
  // ⚠ LA CIBLE EST LE VOLET QU'ON VIENT DE FERMER, et elle ne l'est plus une
  // fois l'offre verrouillée : rien n'y entre après le verrou.
  if (!view.lock_sent) {
    AcceptBagDrop(state, view, action);
  }
  // ⚠ UN DÉPÔT NE SE RETIRE PAS : le protocole n'a aucun paquet pour cela, le
  // serveur ne sait qu'ajouter ou tout annuler. Le dire évite de chercher le
  // geste.
  if (!view.mine.empty() && !view.lock_sent) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
    ImGui::TextDisabled("%s", core::Tr("Un item offert ne se retire pas : annulez l'échange pour "
                                       "recommencer."));
    ImGui::PopTextWrapPos();
  }
  DrawLamp(view.my_locked, view.lock_sent);
  DrawMyZeny(state, view, art, action);
  if (!view.lock_sent) {
    if (SkinSmallToggleButton(core::Tr("Ajouter depuis l'inventory"), state.picking, art)) {
      state.picking = !state.picking;
    }
  }
  ImGui::EndGroup();
}

void DrawPartnerZeny(const DealView& view) {
  if (view.partner_zeny == 0) {
    ImGui::TextDisabled("%s", core::Tr("Zeny : aucun"));
    return;
  }
  const ImVec4 color = view.partner_zeny_changed_after_my_lock ? kAlertText : kTradeBlackText;
  ImGui::PushFont(nullptr, ImGui::GetFontSize() * kPartnerZenyFontScale);
  ImGui::TextColored(color, "%s", TradeAmountText(view.partner_zeny, "z").c_str());
  ImGui::PopFont();
  TextOf(color, SpelledTradeAmount(view.partner_zeny));
  if (view.partner_zeny_changed_after_my_lock) {
    TextOf(kAlertText, core::Tr("Ce montant a changé après votre verrou."));
  }
}

void DrawPartner(const DealView& view, float width, ItemLinkMenu& menu, DealWindowAction& action) {
  ImGui::BeginGroup();
  const std::string_view name = PartnerName(view);
  Unformatted(name);
  ImGui::BeginChild("##deal_partner", ImVec2(width, kColumnHeightPixels * Scale()),
                    ImGuiChildFlags_Borders);
  for (std::size_t index = 0; index < view.partner.size(); ++index) {
    DrawLine(view.partner[index], DealLineRef{.partner = true, .index = static_cast<int>(index)},
             menu, action);
  }
  if (view.partner.empty()) {
    ImGui::TextDisabled("%s", core::Tr("(vide)"));
  }
  ImGui::EndChild();
  DrawLamp(view.partner_locked, false);
  DrawPartnerZeny(view);
  ImGui::EndGroup();
}

// ── LE SAC ET LA QUANTITÉ ───────────────────────────────────────────────────

void DrawBagRow(DealWindowState& state, const DealBagView& bag, const SkinArt& art,
                DealWindowAction& action) {
  ImGui::PushID(static_cast<int>(bag.client_slot));
  const bool refused = bag.most <= 0;
  const std::string refusal(bag.refusal);
  if (refused) {
    ImGui::BeginDisabled();
  }
  if (SkinSmallButton(core::Tr("Offrir"), art)) {
    ChooseDealBagSlot(state, bag, action);
    state.asking_by_drop = false;
  }
  if (refused) {
    ImGui::EndDisabled();
    SkinTooltip(refusal.c_str());
  }
  ImGui::SameLine();
  // ⚠ GRISÉE PAR L'OPACITÉ, NON DÉSACTIVÉE : son nom doit encore dire son
  // refus au survol.
  if (refused) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                        ImGui::GetStyle().Alpha * ImGui::GetStyle().DisabledAlpha);
  }
  const TradeItemGesture gesture = DrawTradeItem(bag.item, CountSuffix(bag.amount));
  if (refused) {
    ImGui::PopStyleVar();
    if (gesture.hovered && !refusal.empty()) {
      SkinTooltip(refusal.c_str());
    }
  }
  if (NoteTradeItemLink(gesture, bag.item, MenuToken(MenuSource::kBag, bag.client_slot),
                        state.item_menu)) {
    action.described_bag_slot = bag.client_slot;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
  ImGui::PopID();
}

void DrawPicking(DealWindowState& state, const DealView& view, const SkinArt& art,
                 DealWindowAction& action) {
  ImGui::SetNextItemWidth(-1.0F);
  if (ImGui::InputTextWithHint("##deal_filter", core::Tr("Filtrer..."), state.filter.InputBuf,
                               IM_ARRAYSIZE(state.filter.InputBuf))) {
    state.filter.Build();
  }
  ImGui::BeginChild("##deal_bag", ImVec2(0.0F, kPickHeightPixels * Scale()),
                    ImGuiChildFlags_Borders);
  bool any = false;
  for (const DealBagView& bag : view.bag) {
    if (!PassesTextFilter(state.filter, bag.item.name)) {
      continue;
    }
    any = true;
    DrawBagRow(state, bag, art, action);
  }
  if (!any) {
    ImGui::TextDisabled("%s", core::Tr("(rien dans l'inventory)"));
  }
  ImGui::EndChild();
}

void DrawAskAmount(DealWindowState& state, const DealView& view, const SkinArt& art,
                   DealWindowAction& action) {
  const DealBagView* bag = FindBag(view.bag, state.asking_slot);
  // La case a disparu, ou ne peut plus partir : la question n'a plus d'objet.
  if (bag == nullptr || bag->most <= 0 || view.lock_sent) {
    state.asking_amount = false;
    return;
  }
  // UNE PILE LÂCHÉE : la modale de quantité du sac et du storage, même geste,
  // même défaut — la pile entière, sélectionnée —, sous le curseur qui vient
  // de lâcher.
  if (state.asking_by_drop) {
    const AmountAnswer given =
        DrawCountPrompt(state.asking_value, bag->most, core::Tr("Offrir"), art);
    if (given.chosen) {
      action.offer = DealOffer{.client_slot = bag->client_slot,
                               .amount = std::clamp(given.amount, 1, bag->most)};
    }
    if (given.closed) {
      state.asking_amount = false;
    }
    return;
  }
  // UNE PILE CHOISIE DANS LA LISTE : la quantité se règle sur place, sous elle.
  state.asking_value = std::clamp(state.asking_value, 1, bag->most);
  const std::string_view name = bag->item.name;
  std::array<char, kSentenceBytes> question{};
  std::snprintf(question.data(), question.size(), core::Tr("Combien de %.*s ? (au plus %d)"),
                static_cast<int>(name.size()), name.data(), bag->most);
  ImGui::TextWrapped("%s", question.data());
  // Ses boutons portent les libellés de ceux de la fenêtre : ils prennent une
  // identité à part.
  ImGui::PushID("##deal_amount");
  static_cast<void>(DrawTradeQuantity(state.asking_value, bag->most, art));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Offrir"), art)) {
    action.offer = DealOffer{.client_slot = bag->client_slot, .amount = state.asking_value};
    state.asking_amount = false;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Annuler"), art)) {
    state.asking_amount = false;
  }
  ImGui::PopID();
}

// ── LES BOUTONS ET L'ÉTAT ───────────────────────────────────────────────────

void DrawLockButton(const DealWindowState& state, const DealView& view, const SkinArt& art,
                    DealWindowAction& action) {
  const DealZenyEntry typed = ReadDealZeny(state.zeny_text, view.purse);
  // ⚠ UN MONTANT ILLISIBLE RETIENT LE VERROU : verrouiller sur le zeny d'avant
  // n'est pas ce que le joueur a tapé.
  const bool held = view.lock_sent || !view.lock_blockers.empty() || !typed.refusal.empty();
  if (held) {
    ImGui::BeginDisabled();
  }
  const char* label = view.my_locked ? core::Tr("Verrouillé") : core::Tr("Verrouiller");
  if (SkinButton(label, art, ImVec2(kActionButtonPixels * Scale(), 0.0F)) && !held) {
    // LE ZENY TAPÉ PART D'ABORD, puis le verrou : verrouiller fige ce que le
    // champ montre, pas ce qui a été proposé avant.
    if (typed.amount.has_value() && *typed.amount != view.my_zeny) {
      action.zeny = *typed.amount;
    }
    action.lock = true;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  if (!view.lock_sent) {
    TipWhenHeld(view.lock_blockers);
  }
}

void DrawExecuteButton(DealWindowState& state, const DealView& view, const SkinArt& art) {
  const ImVec2 size(kActionButtonPixels * Scale(), 0.0F);
  if (view.execute_sent) {
    ImGui::BeginDisabled();
    static_cast<void>(SkinButton(core::Tr("En attente…"), art, size));
    ImGui::EndDisabled();
    return;
  }
  const bool held = !view.execute_blockers.empty() || state.confirming;
  if (held) {
    ImGui::BeginDisabled();
  }
  // 🔴 LE CLIC NE VALIDE PAS : il ouvre le récapitulatif, au rang de l'offre
  // qu'il montre.
  if (SkinButton(core::Tr("Échanger"), art, size) && !held) {
    state.confirming = true;
    state.confirming_revision = view.partner_revision;
  }
  if (held) {
    ImGui::EndDisabled();
  }
  TipWhenHeld(view.execute_blockers);
}

void DrawStatus(const DealWindowState& state, const DealView& view) {
  if (!view.lock_sent) {
    for (const std::string_view reason : view.lock_blockers) {
      TextOf(kAlertText, reason);
    }
  }
  const std::string status = DealStatusText(view);
  ImGui::TextWrapped("%s", status.c_str());
  if (view.partner_changed_after_my_lock) {
    const std::string_view name = PartnerName(view);
    std::array<char, kSentenceBytes> alert{};
    std::snprintf(alert.data(), alert.size(),
                  core::Tr("%.*s a changé son offre après votre verrou : relisez-la."),
                  static_cast<int>(name.size()), name.data());
    TextOf(kAlertText, alert.data());
  }
  if (!view.last_refusal.empty()) {
    TextOf(kTradeRefusedText, view.last_refusal);
  }
  if (!state.flash.empty()) {
    TextOf(kTradeRefusedText, state.flash);
  }
}

void DrawConfirmation(DealWindowState& state, const DealView& view, const SkinArt& art,
                      DealWindowAction& action) {
  // 🔴 UNE OFFRE CHANGÉE SOUS LA BOÎTE LA FERME : on ne valide pas ce qu'on n'a
  // pas lu. Le bouton reste là pour reposer la question sur l'offre neuve.
  if (state.confirming && (view.partner_revision != state.confirming_revision ||
                           view.execute_sent || !view.execute_blockers.empty())) {
    state.confirming = false;
  }
  if (!state.confirming) {
    CloseModal(kConfirmId);
    return;
  }
  // La boîte se demande au niveau de la fenêtre : demandée depuis un volet, elle
  // se nommerait dans une autre pile d'identités et ne paraîtrait jamais.
  if (!ImGui::IsPopupOpen(kConfirmId)) {
    AskModal(kConfirmId);
  }
  const std::string question = DealConfirmationText(view);
  switch (
      DrawModalBox(kConfirmId, question.c_str(), core::Tr("Échanger"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      action.execute = true;
      action.execute_revision = state.confirming_revision;
      state.confirming = false;
      break;
    case ModalAnswer::kNo:
      state.confirming = false;
      break;
    case ModalAnswer::kWaiting:
      break;
  }
}

void DrawBody(DealWindowState& state, const DealView& view, const SkinArt& art,
              ItemLinkFacts link_facts, DealWindowAction& action) {
  const float scale = Scale();
  const float gap = kColumnGapPixels * scale;
  const float column = std::max(0.0F, (ImGui::GetContentRegionAvail().x - gap) * 0.5F);
  DrawMine(state, view, art, column, action);
  ImGui::SameLine(0.0F, gap);
  DrawPartner(view, column, state.item_menu, action);

  if (state.picking && !view.lock_sent) {
    DrawPicking(state, view, art, action);
  }
  if (state.asking_amount) {
    DrawAskAmount(state, view, art, action);
  }

  ImGui::Separator();
  DrawLockButton(state, view, art, action);
  ImGui::SameLine();
  DrawExecuteButton(state, view, art);
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(kActionButtonPixels * scale, 0.0F))) {
    state.open = false;
  }
  DrawStatus(state, view);
  ImGui::TextDisabled(
      "%s", core::Tr("Survol : aperçu   -   Clic droit : menu   -   Maj+clic : lien dans le chat"));
  DrawConfirmation(state, view, art, action);
  NoteMenuChoice(DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts), action);
}

}  // namespace

std::string DealStatusText(const DealView& view) {
  const std::string_view name = PartnerName(view);
  const char* pattern = nullptr;
  if (view.execute_sent) {
    pattern = core::Tr("Validé — en attente de %.*s.");
  } else if (view.my_locked && view.partner_locked) {
    return core::Tr("Les deux offres sont verrouillées : cliquez « Échanger ».");
  } else if (view.lock_sent || view.my_locked) {
    pattern = core::Tr("En attente du verrou de %.*s.");
  } else if (view.partner_locked) {
    pattern = core::Tr("%.*s a verrouillé : verrouillez votre offre quand elle est prête.");
  } else {
    return core::Tr("Verrouillez votre offre quand elle est prête.");
  }
  std::array<char, kSentenceBytes> line{};
  // Les trois gabarits sortent de NOTRE catalogue (`core::Tr` d'un littéral), et
  // portent chacun un seul `%.*s` : g++ ne peut pas le voir à travers le choix.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  std::snprintf(line.data(), line.size(), pattern, static_cast<int>(name.size()), name.data());
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return line.data();
}

DealZenyEntry ReadDealZeny(std::string_view typed, std::int64_t purse) {
  const std::string_view text = Trimmed(typed);
  if (text.empty()) {
    return {};
  }
  if (text.front() == '-') {
    return {.refusal = core::Tr("Un montant ne peut pas être négatif.")};
  }
  const std::optional<std::int64_t> amount = ParseTradeAmount(text);
  if (!amount.has_value()) {
    return {.refusal = core::Tr("Montant illisible : des chiffres, groupés par trois.")};
  }
  if (purse != kTradeNoPurse && *amount > purse) {
    return {.refusal = core::Tr("Vous n'avez pas autant de zeny.")};
  }
  return {.amount = amount};
}

void ChooseDealBagSlot(DealWindowState& state, const DealBagView& bag, DealWindowAction& action) {
  if (bag.most <= 0) {
    state.flash = bag.refusal.empty() ? std::string(core::Tr("Cet item ne peut pas s'offrir."))
                                      : std::string(bag.refusal);
    return;
  }
  state.flash.clear();
  if (bag.most == 1) {
    action.offer = DealOffer{.client_slot = bag.client_slot, .amount = 1};
    state.asking_amount = false;
    return;
  }
  state.asking_amount = true;
  state.asking_slot = bag.client_slot;
  state.asking_value = bag.most;
}

std::string DealConfirmationText(const DealView& view) {
  std::string text = core::Tr("Vous donnez :");
  AppendSide(text, view.mine, view.my_zeny);
  text.append("\n\n");
  text.append(core::Tr("Vous recevez :"));
  AppendSide(text, view.partner, view.partner_zeny);
  if (view.partner_changed_after_my_lock || view.partner_zeny_changed_after_my_lock) {
    const std::string_view name = PartnerName(view);
    std::array<char, kSentenceBytes> alert{};
    std::snprintf(alert.data(), alert.size(),
                  core::Tr("(!) %.*s a changé son offre après votre verrou."),
                  static_cast<int>(name.size()), name.data());
    text.append("\n\n");
    text.append(alert.data());
  }
  text.append("\n\n");
  text.append(core::Tr("Échanger ?"));
  return text;
}

void OpenDealOn(DealWindowState& state, std::uint32_t opening_seq) {
  state.open = true;
  state.opening_seen = opening_seq;
  state.zeny_text.clear();
  state.asking_amount = false;
  state.asking_slot = 0;
  state.asking_by_drop = false;
  state.asking_value = 1;
  state.picking = false;
  state.filter.Clear();
  state.flash.clear();
  state.confirming = false;
  state.confirming_revision = 0;
}

DealWindowAction DrawDealWindow(DealWindowState& state, const DealView& view, const SkinArt& art,
                                ItemLinkFacts link_facts) {
  DealWindowAction action;
  if (!state.open) {
    // Une question laissée en suspens ne survit pas à la fenêtre qui la posait.
    state.confirming = false;
    state.asking_amount = false;
    return action;
  }
  if (view.opening_seq != state.opening_seen) {
    OpenDealOn(state, view.opening_seq);
  }
  const float scale = Scale();
  ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  const std::string_view name =
      view.title.empty() ? std::string_view(core::Tr("Échange")) : view.title;
  const std::string title = std::string(name) + "###" + kDealWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                                             ImGuiWindowFlags_NoCollapse);
  if (body_shown && state.open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kChildRoundingPixels * scale);
    DrawBody(state, view, art, link_facts, action);
    ImGui::PopStyleVar();
  }
  EndSkinnedWindow();
  if (!state.open) {
    // 🔴 FERMER, C'EST ANNULER : rien d'autre ne part avec.
    action = DealWindowAction{};
    action.cancel = true;
    state.confirming = false;
    state.asking_amount = false;
  }
  return action;
}

}  // namespace ro::ui
