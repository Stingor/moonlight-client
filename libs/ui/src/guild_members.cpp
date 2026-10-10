// LE SOUS-ONGLET « MEMBRES » : le tableau triable et le menu d'un membre.

#include <algorithm>
#include <cctype>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "guild_panes.h"

namespace ro::ui {
namespace {

// Les colonnes, dans leur ordre d'affichage.
enum class MemberColumn : int { kName, kJob, kLevel, kPosition, kContribution, kLogin };
constexpr int kMemberColumnCount = 6;
constexpr float kJobColumnPixels = 88.0F;
constexpr float kLevelColumnPixels = 32.0F;
constexpr float kPositionColumnPixels = 84.0F;
constexpr float kContributionColumnPixels = 70.0F;
constexpr float kLoginColumnPixels = 104.0F;
// La ligne d'un membre : le texte, et de quoi y loger sa tête.
constexpr float kHeadRowSlackPixels = 6.0F;
constexpr float kHeadRisePixels = 2.0F;
// Le retrait du nom d'un membre connecté : la place de sa tête, peinte
// PAR-DESSUS en coordonnées d'écran.
constexpr std::string_view kHeadRoom = "      ";

[[nodiscard]] int Rank(MemberColumn column) { return static_cast<int>(column); }

// UN NOM DANS UN GABARIT TRADUIT : la clé n'a ainsi aucun blanc au bord, que le
// catalogue perdrait. Le « %s » est remplacé tel quel, jamais par printf — un
// nom de joueur n'est pas un format.
[[nodiscard]] std::string WithName(const char* pattern, std::string_view name) {
  constexpr std::string_view kNameMark = "%s";
  std::string text(pattern);
  if (const std::size_t at = text.find(kNameMark); at != std::string::npos) {
    text.replace(at, kNameMark.size(), name);
  }
  return text;
}

// Le nom, sans tenir compte de la casse — ASCII seulement, comme le client.
[[nodiscard]] int CompareNames(std::string_view left, std::string_view right) {
  const std::size_t common = std::min(left.size(), right.size());
  for (std::size_t index = 0; index < common; ++index) {
    const int a = std::tolower(static_cast<unsigned char>(left[index]));
    const int b = std::tolower(static_cast<unsigned char>(right[index]));
    if (a != b) {
      return a < b ? -1 : 1;
    }
  }
  return left.size() == right.size() ? 0 : (left.size() < right.size() ? -1 : 1);
}

[[nodiscard]] int CompareMembers(const GuildMemberView& a, const GuildMemberView& b, int column) {
  const auto by = [](auto left, auto right) { return left < right ? -1 : (left > right ? 1 : 0); };
  int order = 0;
  if (column == Rank(MemberColumn::kJob)) {
    order = a.job.compare(b.job);
  } else if (column == Rank(MemberColumn::kLevel)) {
    order = by(a.level, b.level);
  } else if (column == Rank(MemberColumn::kPosition)) {
    order = by(a.position, b.position);
  } else if (column == Rank(MemberColumn::kContribution)) {
    order = by(a.contribution, b.contribution);
  } else if (column == Rank(MemberColumn::kLogin)) {
    order = a.online != b.online ? (a.online ? 1 : -1) : by(a.last_login, b.last_login);
  } else {
    order = CompareNames(a.name, b.name);
  }
  return order != 0 ? order : CompareNames(a.name, b.name);
}

// ⚠ TRIÉ À CHAQUE IMAGE : soixante-seize membres au plus, et un ordre qui ne
// dépend que de ce que la vue porte — aucun souvenir à tenir juste.
void SortMembers(GuildTabState& state, std::span<const GuildMemberView> members) {
  ImGuiTableSortSpecs* const specs = ImGui::TableGetSortSpecs();
  int column = Rank(MemberColumn::kName);
  bool ascending = true;
  if (specs != nullptr && specs->SpecsCount > 0) {
    column = specs->Specs[0].ColumnIndex;
    ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
  }
  state.member_order.resize(members.size());
  std::iota(state.member_order.begin(), state.member_order.end(), 0);
  std::ranges::stable_sort(state.member_order, [&](int left, int right) {
    const int order = CompareMembers(members[static_cast<std::size_t>(left)],
                                     members[static_cast<std::size_t>(right)], column);
    return ascending ? order < 0 : order > 0;
  });
  if (specs != nullptr) {
    specs->SpecsDirty = false;
  }
}

// La tête d'un membre connecté, tenue dans sa case sans jamais l'agrandir :
// sur une ligne de liste on ne fait que RÉDUIRE.
void DrawHead(const SkinImage& head, ImVec2 at, float box) {
  if (!head.usable() || head.width <= 0.0F || head.height <= 0.0F) {
    return;
  }
  const float fit = std::min({box / head.width, box / head.height, 1.0F});
  const float width = head.width * fit;
  const float height = head.height * fit;
  const ImVec2 low(at.x + (box - width) * 0.5F, at.y + (box - height) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(head.id, low, ImVec2(low.x + width, low.y + height));
}

// Le menu d'un membre : ce que le joueur peut faire sur LUI.
void DrawMemberMenu(GuildTabState& state, const GuildView& view, const GuildMemberView& member,
                    std::span<const std::pair<int, std::string_view>> positions,
                    GuildAction& action) {
  if (!BeginSkinPopupContextItem("membre")) {
    return;
  }
  state.selected_char = member.char_id;
  const std::string name(member.name);
  GuildTextColored(kGuildLabelColor, name.c_str());
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Copier le nom"))) {
    ImGui::SetClipboardText(name.c_str());
  }
  // CHUCHOTER : réservé aux membres EN LIGNE — à un déconnecté, ce ne serait
  // qu'un message d'erreur. Le nom part dans la case du destinataire.
  if (member.online && !member.self && ImGui::MenuItem(core::Tr("Chuchoter…"))) {
    action.verb = GuildVerb::kWhisper;
    action.text = name;
    state.status = WithName(core::Tr("Chuchotement à %s"), name);
  }
  if (member.online && !member.self && ImGui::MenuItem(core::Tr("Chuchoter dans une fenêtre…"))) {
    action.verb = GuildVerb::kWhisperWindow;
    action.text = name;
  }
  // LE CARNET DE CHASSE MVP : la même entrée que dans les deux autres menus d'un
  // joueur, et le même grisage. Connecté ou non : l'invitation attend son retour.
  if (!member.self && !view.mvp_invites_withdrawn && MvpInviteMenuItem(member.mvp_refusal)) {
    action.verb = GuildVerb::kInviteMvp;
    action.text = name;
  }
  // ENVOYER UN COURRIER : le destinataire part déjà rempli, connecté ou non.
  // Jamais à soi-même — le serveur le refuse.
  if (!member.self && ImGui::MenuItem(core::Tr("Envoyer un courrier…"))) {
    action.verb = GuildVerb::kSendMail;
    action.text = name;
    state.status = WithName(core::Tr("Courrier à %s"), name);
  }
  // 🔴 LE POSTE ZÉRO N'EST PAS DANS LA LISTE : l'affecter est un TRANSFERT de
  // la direction, et il a son entrée à lui, plus bas.
  if (view.master && !positions.empty() && member.position != 0) {
    if (ImGui::BeginMenu(core::Tr("Changer de poste"))) {
      for (const auto& [position, label] : positions) {
        // ⚠ PAR IDENTIFIANT DE POSTE : deux postes peuvent porter le même nom.
        ImGui::PushID(position);
        const std::string shown(label);
        const bool current = position == member.position;
        if (ImGui::MenuItem(shown.c_str(), nullptr, current) && !current) {
          action.verb = GuildVerb::kChangePosition;
          action.account_id = member.account_id;
          action.char_id = member.char_id;
          action.position = position;
          state.status = name + " -> " + shown;
        }
        ImGui::PopID();
      }
      ImGui::EndMenu();
    }
  }
  // ⚠ LES BOÎTES S'OUVRENT À L'IMAGE SUIVANTE, au niveau de l'onglet : ici la
  // pile d'identifiants est celle du menu, et la boîte ne s'y trouverait pas.
  const auto aim = [&state, &member, &name]() {
    state.target_account = member.account_id;
    state.target_char = member.char_id;
    state.target_name = name;
  };
  if (view.master && !member.master && !member.self && member.position != 0) {
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Transférer la direction…"))) {
      aim();
      state.transfer_confirm.fill('\0');
      state.ask_transfer = true;
      ImGui::CloseCurrentPopup();
    }
  }
  // EXPULSER : selon le DROIT DU POSTE, comme le serveur — jamais le maître,
  // jamais soi-même (c'est « Quitter » qu'il faut).
  if (view.may_expel && !member.master && !member.self) {
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Expulser…"))) {
      aim();
      state.reason.fill('\0');
      state.ask_expel = true;
      ImGui::CloseCurrentPopup();
    }
  }
  EndSkinPopup();
}

void DrawMemberRow(GuildTabState& state, const GuildView& view, const GuildMemberView& member,
                   std::span<const std::pair<int, std::string_view>> positions, float row_height,
                   GuildAction& action) {
  ImGui::PushID(static_cast<int>(member.char_id != 0 ? member.char_id : member.account_id));
  ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
  // Comme le client : la ligne teintée ET la tête pour les membres CONNECTÉS,
  // sous le même test.
  if (member.online) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kGuildOnlineRowColor);
  }
  ImGui::TableSetColumnIndex(Rank(MemberColumn::kName));
  const ImVec2 cell = ImGui::GetCursorScreenPos();
  const bool selected = state.selected_char != 0 && state.selected_char == member.char_id;
  const std::string label =
      std::string(member.online ? kHeadRoom : std::string_view()) + std::string(member.name);
  if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
    state.selected_char = member.char_id;
  }
  if (member.online) {
    DrawHead(member.head, ImVec2(cell.x, cell.y - GuildPx(kHeadRisePixels)), row_height);
  }
  DrawMemberMenu(state, view, member, positions, action);

  ImGui::TableSetColumnIndex(Rank(MemberColumn::kJob));
  const std::string job(member.job);
  GuildTextColored(kGuildBlackColor, job.c_str());
  ImGui::TableSetColumnIndex(Rank(MemberColumn::kLevel));
  GuildTextColored(kGuildBlackColor, std::to_string(member.level).c_str());
  ImGui::TableSetColumnIndex(Rank(MemberColumn::kPosition));
  const std::string position(member.position_label);
  if (member.position == 0) {
    GuildTextColored(kGuildBlueColor, position.empty() ? core::Tr("Maître") : position.c_str());
  } else {
    GuildTextColored(kGuildBlackColor, position.empty() ? "—" : position.c_str());
  }
  ImGui::TableSetColumnIndex(Rank(MemberColumn::kContribution));
  GuildTextColored(kGuildBlackColor, std::to_string(member.contribution).c_str());
  ImGui::TableSetColumnIndex(Rank(MemberColumn::kLogin));
  if (member.online) {
    GuildTextColored(kGuildGreenColor, core::Tr("En ligne"));
  } else {
    const std::string seen = member.last_seen.empty() ? "—" : std::string(member.last_seen);
    GuildTextColored(kGuildLabelColor, seen.c_str());
  }
  ImGui::PopID();
}

}  // namespace

void DrawGuildMembers(GuildTabState& state, const GuildView& view, float list_height,
                      const SkinArt& art, GuildAction& action) {
  // LES POSTES QU'ON PEUT DONNER, par identifiant, le maître exclu.
  std::vector<std::pair<int, std::string_view>> positions;
  for (std::size_t index = 1; index < view.positions.size(); ++index) {
    if (!view.positions[index].name.empty()) {
      positions.emplace_back(static_cast<int>(index), view.positions[index].name);
    }
  }

  const auto flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                     ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                     ImGuiTableFlags_ScrollY;
  if (!ImGui::BeginTable("membres", kMemberColumnCount, flags, ImVec2(0.0F, list_height))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Nom"),
                          ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
  ImGui::TableSetupColumn(core::Tr("Classe"), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kJobColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Nv"), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kLevelColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Poste"), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kPositionColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Contrib."), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kContributionColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Connexion"), ImGuiTableColumnFlags_WidthFixed,
                          GuildPx(kLoginColumnPixels));
  ImGui::TableHeadersRow();
  SortMembers(state, view.members);
  const float row_height = ImGui::GetTextLineHeight() + GuildPx(kHeadRowSlackPixels);
  for (const int rank : state.member_order) {
    DrawMemberRow(state, view, view.members[static_cast<std::size_t>(rank)], positions, row_height,
                  action);
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

}  // namespace ro::ui
