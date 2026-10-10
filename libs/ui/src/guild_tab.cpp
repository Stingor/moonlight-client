#include "ro/ui/guild_tab.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "guild_panes.h"

namespace ro::ui {
namespace {

// ── LES MESURES DE L'EN-TÊTE ────────────────────────────────────────────────
constexpr float kRefreshButtonPixels = 90.0F;
constexpr float kEmblemPixels = 48.0F;
constexpr float kEmblemDropPixels = 2.0F;
constexpr float kEmblemGapPixels = 10.0F;
constexpr float kEmblemRounding = 4.0F;
constexpr float kEmblemBorderPixels = 1.5F;
constexpr float kEmblemHoverPixels = 2.0F;
constexpr ImU32 kEmblemBackdrop = IM_COL32(0, 0, 0, 30);
constexpr ImU32 kEmblemBorder = IM_COL32(90, 90, 110, 220);
constexpr ImU32 kEmblemHoverBorder = IM_COL32(255, 220, 120, 255);
constexpr float kExpGaugePixels = 14.0F;
constexpr float kPercent = 100.0F;
constexpr float kCreateFieldPixels = 220.0F;
constexpr float kInviteFieldPixels = 130.0F;
constexpr float kSaveNoticePixels = 110.0F;
constexpr float kCancelNoticePixels = 90.0F;
// Ce que la barre d'actions réserve sous les listes, en plus d'un bouton.
constexpr float kActionBarSlackPixels = 6.0F;
constexpr float kListLeastPixels = 80.0F;

// L'emblème, sur un fond léger et sous un liseré. ⚠ SANS EMBLÈME, RIEN : la
// place reste réservée à l'en-tête, vide.
void DrawEmblemBox(ImDrawList* into, const GuildView& view, ImVec2 low, ImVec2 high) {
  if (!view.emblem.usable()) {
    return;
  }
  into->AddRectFilled(low, high, kEmblemBackdrop, GuildPx(kEmblemRounding));
  into->AddImage(view.emblem.id, low, high);
  into->AddRect(low, high, kEmblemBorder, GuildPx(kEmblemRounding), 0,
                GuildPx(kEmblemBorderPixels));
}

// SANS GUILDE : la création directe, comme le « Guild Companion » du client.
void DrawNoGuild(GuildTabState& state, const GuildView& view, const SkinArt& art,
                 GuildAction& action) {
  GuildTextColored(kGuildLabelColor, core::Tr("Tu n'appartiens à aucune guilde."));
  ImGui::Spacing();
  ImGui::TextWrapped("%s",
                     core::Tr("Rejoins-en une (invitation d'un maître de guilde) ou crée la tienne "
                              "ici : cet onglet affichera ensuite membres, postes et relations."));
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  GuildTextColored(kGuildBlackColor, core::Tr("Créer une guilde"));
  GuildTextColored(kGuildLabelColor,
                   core::Tr("Un Emperium dans l'inventory est nécessaire, et la carte ne "
                            "doit pas interdire les guildes."));
  ImGui::SetNextItemWidth(GuildPx(kCreateFieldPixels));
  const bool submitted =
      ImGui::InputText("##guild_create", state.create_name.data(), state.create_name.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const bool clicked = SkinButton(core::Tr("Créer"), art);
  const std::string_view name = BufferText(state.create_name);
  if ((submitted || clicked) && !name.empty()) {
    action.verb = GuildVerb::kCreate;
    action.text = std::string(name);
    state.status = std::string(core::Tr("Demande envoyée : ")) + std::string(name);
  }
  // Le client affiche déjà sa propre boîte ; on double l'information ici pour
  // ne pas laisser l'onglet muet.
  if (!view.creation_answer.empty()) {
    const std::string answer(view.creation_answer);
    GuildTextColored(view.creation_succeeded ? kGuildGreenColor : kGuildRedColor, answer.c_str());
  } else if (!state.status.empty()) {
    GuildTextColored(kGuildLabelColor, state.status.c_str());
  }
}

// L'en-tête : l'emblème, l'identité, et la jauge d'expérience.
void DrawHeader(GuildTabState& state, const GuildView& view, const SkinArt& art,
                GuildAction& action) {
  const ImVec2 header = ImGui::GetCursorScreenPos();
  // « ACTUALISER », ANCRÉ EN HAUT À DROITE : posé avant l'en-tête, curseur
  // rendu ensuite, si bien que l'en-tête se dessine comme s'il n'y était pas.
  {
    const float width = GuildPx(kRefreshButtonPixels);
    const ImVec2 kept = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(kept.x + ImGui::GetContentRegionAvail().x - width);
    if (SkinButton(core::Tr("Actualiser"), art, ImVec2(width, 0.0F))) {
      action.verb = GuildVerb::kRefresh;
    }
    GuildTooltip(core::Tr("Redemande au serveur membres, postes et infos de guilde."));
    ImGui::SetCursorPos(kept);
  }
  const float side = GuildPx(kEmblemPixels);
  const ImVec2 low(header.x, header.y + GuildPx(kEmblemDropPixels));
  const ImVec2 high(low.x + side, low.y + side);
  ImDrawList* into = ImGui::GetWindowDrawList();
  DrawEmblemBox(into, view, low, high);
  // L'EMBLÈME OUVRE SON ÉDITEUR, pour le maître seul — comme chez le serveur.
  if (view.master) {
    const ImVec2 kept = ImGui::GetCursorPos();
    ImGui::SetCursorScreenPos(low);
    if (ImGui::InvisibleButton("##guild_emblem", ImVec2(side, side))) {
      state.emblem.ask_open = true;
      // Cliquer l'emblème, c'est vouloir MODIFIER celui-là : l'éditeur s'ouvre
      // dessus, pas sur une toile vide.
      state.emblem.load_current = true;
    }
    if (ImGui::IsItemHovered()) {
      into->AddRect(low, high, kEmblemHoverBorder, GuildPx(kEmblemRounding), 0,
                    GuildPx(kEmblemHoverPixels));
      SkinTooltipFormat("%s", core::Tr("Modifier cet emblème dans l'éditeur…"));
    }
    ImGui::SetCursorPos(kept);
  }

  const float indent = side + GuildPx(kEmblemGapPixels);
  ImGui::Indent(indent);
  const std::string name(view.name);
  GuildTextColored(kGuildBlackColor, name.c_str());
  const std::string master = view.master_name.empty() ? "?" : std::string(view.master_name);
  std::array<char, 256> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("Niveau %d  ·  Maître : %s"), view.level,
                master.c_str());
  GuildTextColored(kGuildLabelColor, line.data());
  std::snprintf(line.data(), line.size(),
                core::Tr("Membres : %d / %d  ·  En ligne : %d  ·  Niveau moyen : %d"),
                view.member_count, view.member_most, view.online, view.average_level);
  GuildTextColored(kGuildLabelColor, line.data());
  // LE POSTE ET SES DROITS : ils expliquent la présence — ou l'absence — des
  // gestes plus bas, au lieu de laisser le serveur refuser en silence.
  if (view.position_found) {
    std::string rights;
    if (view.rights_known) {
      const auto add = [&rights](bool granted, const char* word) {
        if (!granted) {
          return;
        }
        if (!rights.empty()) {
          rights += ' ';
        }
        rights += word;
      };
      add(view.may_invite, core::Tr("inviter"));
      add(view.may_expel, core::Tr("expulser"));
      add(view.may_storage, core::Tr("storage"));
      if (rights.empty()) {
        rights = core::Tr("aucun droit");
      }
    } else {
      rights = core::Tr("droits inconnus");
    }
    const std::string position = view.my_position.empty() ? "?" : std::string(view.my_position);
    std::snprintf(line.data(), line.size(), core::Tr("Ton poste : %s  ·  %s"), position.c_str(),
                  rights.c_str());
    GuildTextColored(kGuildLabelColor, line.data());
  }
  ImGui::Unindent(indent);
  if (!view.land.empty()) {
    const std::string land(view.land);
    std::snprintf(line.data(), line.size(), core::Tr("Territoire : %s"), land.c_str());
    GuildTextColored(kGuildLabelColor, line.data());
  }
  if (view.next_exp > 0) {
    const float ratio =
        std::clamp(static_cast<float>(view.exp) / static_cast<float>(view.next_exp), 0.0F, 1.0F);
    std::snprintf(line.data(), line.size(), "EXP %lld / %lld  (%.1f %%)",
                  static_cast<long long>(view.exp), static_cast<long long>(view.next_exp),
                  static_cast<double>(ratio * kPercent));
    ImGui::ProgressBar(ratio, ImVec2(-1.0F, GuildPx(kExpGaugePixels)), line.data());
  }
}

// À LA SUITE DU BOUTON PRÉCÉDENT, ou à la ligne quand la place manque : une
// fenêtre étroite ne doit pas pousser un bouton hors de son bord.
void FollowButton(const char* label, const SkinArt& art) {
  ImGui::SameLine();
  if (ImGui::GetContentRegionAvail().x < SkinButtonWidth(label, art)) {
    ImGui::NewLine();
  }
}

// L'annonce, éditable par le maître ; puis le storage de guilde.
void DrawNotice(GuildTabState& state, const GuildView& view, const SkinArt& art,
                GuildAction& action) {
  ImGui::Spacing();
  if (state.notice_editing) {
    // Le rappel « titre, puis message » vaut aussi en RÉÉDITION : les indices ne
    // s'affichent que sur un champ vide.
    GuildTextColored(kGuildBlackColor, core::Tr("Annonce de la guilde — titre, puis message"));
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##guild_subject", core::Tr("Titre de l'annonce"),
                             state.notice_subject.data(), state.notice_subject.size());
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##guild_body", core::Tr("Contenu du message"),
                             state.notice_body.data(), state.notice_body.size());
    if (SkinButton(core::Tr("Enregistrer"), art, ImVec2(GuildPx(kSaveNoticePixels), 0.0F))) {
      action.verb = GuildVerb::kSaveNotice;
      action.subject = std::string(BufferText(state.notice_subject));
      action.body = std::string(BufferText(state.notice_body));
      state.notice_editing = false;
      state.status = core::Tr("Annonce envoyée.");
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art, ImVec2(GuildPx(kCancelNoticePixels), 0.0F))) {
      state.notice_editing = false;
    }
  } else if (!view.notice_subject.empty() || !view.notice_body.empty()) {
    const std::string subject =
        view.notice_subject.empty() ? core::Tr("Annonce") : std::string(view.notice_subject);
    GuildTextColored(kGuildBlueColor, subject.c_str());
    if (!view.notice_body.empty()) {
      const std::string body(view.notice_body);
      ImGui::TextWrapped("%s", body.c_str());
    }
  }
  if (!state.notice_editing && view.master) {
    if (SkinButton(core::Tr("Modifier l'annonce"), art)) {
      CopyIntoBuffer(state.notice_subject, view.notice_subject);
      CopyIntoBuffer(state.notice_body, view.notice_body);
      state.notice_editing = true;
    }
    // Doublon VOLONTAIRE du clic sur l'emblème : personne ne devine qu'une
    // image est cliquable, et c'est ici que vivent les gestes de maître.
    const char* emblem_label = core::Tr("Changer l'emblème…");
    FollowButton(emblem_label, art);
    if (SkinButton(emblem_label, art)) {
      state.emblem.ask_open = true;
    }
  }
  // LE STORAGE DE GUILDE : un alias de `@guildstorage`, et rien d'autre — le
  // serveur n'a aucun paquet pour l'ouvrir. Comme la commande, il BASCULE.
  if (!state.notice_editing) {
    // Droits inconnus : on laisse cliquer, le serveur revérifie.
    const bool may_storage = !view.rights_known || view.may_storage;
    const char* storage_label = core::Tr("Storage de guilde");
    // 🔴 À LA SUITE DES BOUTONS DE MAÎTRE, JAMAIS DU TEXTE DE L'ANNONCE : collé
    // derrière sa dernière ligne, il partirait hors de la fenêtre.
    if (view.master) {
      FollowButton(storage_label, art);
    }
    ImGui::BeginDisabled(!may_storage);
    if (SkinButton(storage_label, art)) {
      action.verb = GuildVerb::kOpenStorage;
      state.status = core::Tr("Storage de guilde : ouverture demandée.");
    }
    ImGui::EndDisabled();
    GuildTooltip(may_storage
                     ? core::Tr("Ouvre — ou referme — le Storage de guilde (@guildstorage).")
                     : core::Tr("Ton poste n'a pas le droit « storage »."));
  }
}

// La barre d'actions : inviter, quitter, dissoudre.
void DrawActionBar(GuildTabState& state, const GuildView& view, const SkinArt& art,
                   GuildAction& action) {
  if (view.may_invite) {
    ImGui::AlignTextToFramePadding();
    GuildTextColored(kGuildBlackColor, core::Tr("Inviter :"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(GuildPx(kInviteFieldPixels));
    ImGui::InputText("##guild_invite", state.invite_name.data(), state.invite_name.size());
    GuildTooltip(core::Tr("Nom exact du personnage à inviter (il doit être connecté)."));
    ImGui::SameLine();
    const std::string_view invited = BufferText(state.invite_name);
    if (SkinButton(core::Tr("Inviter"), art) && !invited.empty()) {
      action.verb = GuildVerb::kInvite;
      action.text = std::string(invited);
      state.status = std::string(core::Tr("Invitation envoyée à ")) + std::string(invited);
      state.invite_name.fill('\0');
    }
    ImGui::SameLine();
  }
  if (SkinButton(core::Tr("Quitter…"), art)) {
    state.reason.fill('\0');
    AskModal(GuildModalName(core::Tr("Quitter la guilde"), kGuildLeaveModalId).c_str());
  }
  // Dissoudre : réservé au maître, comme la commande.
  if (view.master) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Dissoudre…"), art)) {
      state.disband_confirm.fill('\0');
      AskModal(GuildModalName(core::Tr("Dissoudre la guilde"), kGuildDisbandModalId).c_str());
    }
    GuildTooltip(core::Tr("Supprime définitivement la guilde (@breakguild)."));
  }
  if (!state.status.empty()) {
    GuildTextColored(kGuildLabelColor, state.status.c_str());
  }
}

}  // namespace

float GuildScale() { return ImGui::GetStyle().FontScaleMain; }

float GuildPx(float pixels) { return pixels * GuildScale(); }

ImVec4 GuildInk(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }

void GuildTextColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

void GuildTooltip(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", text);
  }
}

std::string_view BufferText(std::span<const char> buffer) {
  const auto end = std::find(buffer.begin(), buffer.end(), '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

void CopyIntoBuffer(std::span<char> buffer, std::string_view text) {
  if (buffer.empty()) {
    return;
  }
  const std::string_view kept =
      core::WithoutDanglingUtf8(text.substr(0, std::min(text.size(), buffer.size() - 1)));
  std::fill(buffer.begin(), buffer.end(), '\0');
  std::copy(kept.begin(), kept.end(), buffer.begin());
}

std::string GuildModalName(const char* title, std::string_view id) {
  return std::string(title) + std::string(id);
}

GuildAction DrawGuildTab(GuildTabState& state, const GuildView& view, bool& smooth_icons,
                         const SkinArt& art) {
  GuildAction action;
  if (!view.in_guild) {
    DrawNoGuild(state, view, art, action);
    return action;
  }
  DrawHeader(state, view, art, action);
  DrawNotice(state, view, art, action);
  ImGui::Separator();

  // LA HAUTEUR LAISSÉE AUX LISTES : tout, sauf la barre d'actions du bas — un
  // bouton du jeu et la ligne du compte rendu.
  const auto list_height = [&art]() {
    const float actions = SkinFooterHeight(art) + ImGui::GetTextLineHeightWithSpacing() +
                          GuildPx(kActionBarSlackPixels);
    return std::max(GuildPx(kListLeastPixels), ImGui::GetContentRegionAvail().y - actions);
  };
  if (BeginSkinTabBar("guilde", art)) {
    if (view.members_tab && ImGui::BeginTabItem(core::Tr("Membres"))) {
      DrawGuildMembers(state, view, list_height(), art, action);
      ImGui::EndTabItem();
    }
    if (view.positions_tab && ImGui::BeginTabItem(core::Tr("Postes"))) {
      if (ImGui::BeginChild("postes", ImVec2(0.0F, list_height()), ImGuiChildFlags_Borders)) {
        DrawGuildPositions(state, view, art, action);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(core::Tr("Skills"))) {
      if (ImGui::BeginChild("competences", ImVec2(0.0F, list_height()), ImGuiChildFlags_Borders)) {
        DrawGuildSkills(state, view, smooth_icons, art, action);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(core::Tr("Relations"))) {
      if (ImGui::BeginChild("relations", ImVec2(0.0F, list_height()), ImGuiChildFlags_Borders)) {
        DrawGuildRelations(state, view, art);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (view.bans_tab && ImGui::BeginTabItem(core::Tr("Expulsions"))) {
      if (ImGui::BeginChild("expulsions", ImVec2(0.0F, list_height()), ImGuiChildFlags_Borders)) {
        DrawGuildBans(view);
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    EndSkinTabBar();
  }

  DrawActionBar(state, view, art, action);
  DrawGuildConfirmations(state, view, art, action);
  // ⚠ LE DOSSIER EST RELU À CHAQUE OUVERTURE : on y dépose justement un fichier
  // juste avant de venir le choisir. C'est `app` qui lit le disque.
  if (state.emblem.ask_open) {
    state.emblem.ask_open = false;
    state.emblem.chosen_file = -1;
    state.emblem.error.clear();
    if (action.verb == GuildVerb::kNothing) {
      action.verb = GuildVerb::kOpenEmblemEditor;
      action.from_current = state.emblem.load_current;
    }
    state.emblem.load_current = false;
    AskModal(GuildModalName(core::Tr("Changer l'emblème"), kGuildEmblemModalId).c_str());
  }
  DrawGuildEmblemModal(state, view, art, action);
  return action;
}

bool DrawGuildInvitation(std::string_view guild_name, bool& accepted, const SkinArt& art) {
  constexpr const char* kInvitationId = "###guild_invitation";
  if (!ImGui::IsPopupOpen(kInvitationId)) {
    AskModal(kInvitationId);
  }
  std::array<char, 256> question = {};
  const std::string name(guild_name);
  std::snprintf(question.data(), question.size(),
                core::Tr("La guilde « %s » t'invite à la rejoindre. Accepter ?"), name.c_str());
  const ModalAnswer answer =
      DrawModalBox(kInvitationId, question.data(), core::Tr("Accepter"), core::Tr("Refuser"), art);
  if (answer == ModalAnswer::kWaiting) {
    return false;
  }
  accepted = answer == ModalAnswer::kYes;
  return true;
}

}  // namespace ro::ui
