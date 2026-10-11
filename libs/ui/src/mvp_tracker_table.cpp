// Le carnet de chasse MVP : la table — filtre, colonnes, tri, rangs, cellules,
// plan de la carte et menu du nom.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "mvp_tracker_parts.h"

namespace ro::ui::mvp_tracker {

namespace {

// LE PLAN DE LA CARTE, en infobulle, avec la tombe pointée dessus.
//
// 🔴 LA PROJECTION EST CELLE DE LA MINIMAP : une fraction de carte, et
// `(cellules_y - y) / cellules_y` en hauteur, parce que le repère du jeu monte
// tandis que celui de l'image descend. Et la fraction se prend sur la taille EN
// CELLULES que le serveur envoie, jamais sur celle du bitmap : un plan de radar
// n'a aucune raison d'être à l'échelle de la carte.
//
// ⚠ LA BULLE N'A PAS LA BORNE DES AUTRES : le plan suit l'échelle de
// l'interface, et une largeur fixe le rognerait aux grandes échelles. Elle ne
// porte, à côté de lui, que des lignes courtes.
//
// ⚠ Le plan arrive à l'image qui SUIT la demande : la première image de survol
// ne montre rien.
void DrawMapPreview(MvpTrackerWindowState& state, const MvpTrackerView& view,
                    const MvpSlotView& slot) {
  state.wanted_map = slot.map;
  if (view.map_preview.map != slot.map || !BeginSkinTooltip(ImGui::GetIO().DisplaySize.x)) {
    return;
  }
  // Le nom montré dans la colonne, puis l'interne quand l'installation en donne
  // un lisible : c'est l'interne que le chat et les sites emploient.
  Unformatted(MvpMapShown(slot));
  if (!slot.map_label.empty()) {
    ImGui::TextDisabled("%s", std::string(slot.map).c_str());
  }
  const SkinImage& image = view.map_preview.image;
  if (!image.usable()) {
    ImGui::TextDisabled("%s", core::Tr("(pas de miniature)"));
    EndSkinTooltip();
    return;
  }
  // Les proportions gardées, le grand côté au côté de l'aperçu.
  const float side = Px(kMvpPreviewSidePixels);
  const float scale = image.width > image.height ? side / image.width : side / image.height;
  const ImVec2 drawn(image.width * scale, image.height * scale);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Image(image.id, drawn);
  // -1 dit « position inconnue » ; (0, 0) n'a jamais ce sens ici.
  if (slot.observed && slot.tomb_x >= 0 && slot.tomb_y >= 0 && slot.map_cells_x > 0 &&
      slot.map_cells_y > 0) {
    const float cells_x = static_cast<float>(slot.map_cells_x);
    const float cells_y = static_cast<float>(slot.map_cells_y);
    const float across = static_cast<float>(slot.tomb_x) / cells_x;
    const float down = (cells_y - static_cast<float>(slot.tomb_y)) / cells_y;
    if (across >= 0.0F && across <= 1.0F && down >= 0.0F && down <= 1.0F) {
      const ImVec2 at(origin.x + across * drawn.x, origin.y + down * drawn.y);
      ImDrawList* into = ImGui::GetWindowDrawList();
      // Doré quand l'instant a été mérité au Convex Mirror, orangé quand on n'a
      // qu'une fenêtre : le code de la minimap.
      into->AddCircleFilled(at, Px(kMvpTombDotRadiusPixels),
                            slot.exact_respawn_s != 0 ? kMvpTombExactColor : kMvpTombWindowColor);
      into->AddCircle(at, Px(kMvpTombDotRadiusPixels), kMvpTombRingColor, 0, kMvpTombRingWidth);
    }
    ImGui::Text(core::Tr("Tombe en (%d, %d)"), static_cast<int>(slot.tomb_x),
                static_cast<int>(slot.tomb_y));
  } else {
    ImGui::TextDisabled("%s", core::Tr("Aucune tombe connue."));
  }
  EndSkinTooltip();
}

// L'ÉTOILE DES FAVORIS, en art quand il est fourni ; sinon un petit bouton
// texte, pour qu'un favori se marque avant que l'art n'arrive.
//
// ⚠ L'IDENTIFIANT NE DÉPEND PAS DE L'ÉTAT, sans quoi le bouton changerait
// d'identité sous le doigt à l'instant du clic.
void DrawFavoriteButton(const MvpTrackerView& view, const MvpSlotView& slot, const SkinArt& art,
                        MvpTrackerAction& action) {
  const SkinImage& image = slot.favorite ? view.favorite_on : view.favorite_off;
  bool clicked = false;
  // Le favori se tient au serveur : hors liaison vérifiée, l'étoile se grise.
  ImGui::BeginDisabled(!view.link_ready);
  if (!image.usable()) {
    clicked = SkinSmallButton(slot.favorite ? "*###fav" : " ###fav", art);
  } else {
    const float side = Px(image.height > 0.0F ? image.height : kMvpFavoriteFallbackPixels);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    clicked = ImGui::InvisibleButton("##fav", ImVec2(side, side));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    // Une étoile éteinte survolée s'éclaircit, faute d'un troisième bitmap ;
    // grisée, elle pâlit comme les boutons.
    const ImU32 tint = (hovered && !slot.favorite) ? kMvpFavoriteHoverTint : kMvpFavoriteRestTint;
    ImGui::GetWindowDrawList()->AddImage(image.id, origin, ImVec2(origin.x + side, origin.y + side),
                                         ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(tint));
  }
  ImGui::EndDisabled();
  TellLinkPending(view);
  // L'étoile ne bascule qu'à l'arrivée des favoris du serveur.
  if (clicked) {
    action.favorite_slot = slot.slot_id;
    action.favorite_on = !slot.favorite;
  }
  SkinTooltip(core::Tr("Favori : alerte à l'ouverture de la fenêtre."));
}

// LA POIGNÉE DE DÉTACHEMENT : une colonne à elle, parce que le nom est un LIEN
// et que les deux gestes se disputeraient le même bouton.
void DrawGrip(MvpTrackerWindowState& state, const MvpSlotView& slot) {
  const float grip_height = ImGui::GetTextLineHeight();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##grip", ImVec2(Px(kMvpGripWidthPixels), grip_height));
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    state.drag_slot = slot.slot_id;
  }
  // Six points peints à la main : aucun glyphe de poignée ne traverse la police.
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImU32 dot = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  const float step = grip_height / static_cast<float>(kMvpGripDotRows + 1);
  for (int row = 1; row <= kMvpGripDotRows; ++row) {
    const float y = origin.y + step * static_cast<float>(row);
    into->AddCircleFilled(ImVec2(origin.x + Px(kMvpGripDotNearPixels), y), kMvpGripDotRadius, dot);
    into->AddCircleFilled(ImVec2(origin.x + Px(kMvpGripDotFarPixels), y), kMvpGripDotRadius, dot);
  }
  if (hovered) {
    SkinTooltip(core::Tr("Glisser hors du carnet pour détacher la ligne."));
  }
}

// LE NOM D'UN MVP : un lien de monstre complet quand le rang désigne un
// monstre — aperçu au survol, fiche au clic, menu au clic droit — et, sur un
// rang qui porte une heure, le Maj+clic partage le respawn plutôt que le
// monstre. Sinon, un libellé simple : un lien mort serait pire.
void DrawName(MvpTrackerWindowState& state, const MvpSlotView& slot, MvpTrackerAction& action) {
  if (slot.link_mob_id == 0) {
    Unformatted(slot.label);
    return;
  }
  const std::string shown = slot.label.empty() ? std::string("?") : std::string(slot.label);
  // Un GROUPE : survol et clics portent sur le lien entier.
  ImGui::BeginGroup();
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kMvpNameLinkColor), "%s", shown.c_str());
  ImGui::EndGroup();
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    const ImVec2 low = ImGui::GetItemRectMin();
    const ImVec2 high = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(low.x, high.y), ImVec2(high.x, high.y),
                                        kMvpNameLinkColor);
    action.name_link.hovered_id = slot.link_mob_id;
    action.name_link.hovered_kind = ChatLinkKind::kMonster;
    // Le créneau survolé : son aperçu annonce ce que fait le Maj+clic, qui
    // dépend de l'heure que porte le rang.
    action.name_link.hovered_token = slot.slot_id;
    action.name_link.hovering = true;
  }
  // Un rang lâché dans le carnet : rien, pas même sur le nom qu'il survole.
  if (state.row_dragged) {
    return;
  }
  ChatLinkGesture gesture = HitChatLink(hovered);
  if (gesture == ChatLinkGesture::kNothing) {
    return;
  }
  if (gesture == ChatLinkGesture::kMenu) {
    state.menu_slot = slot.slot_id;
    state.menu_requested = true;
    return;
  }
  if (gesture == ChatLinkGesture::kRepost && (slot.kill_time_s > 0 || slot.exact_respawn_s > 0)) {
    gesture = ChatLinkGesture::kShareRespawn;
  }
  action.name_link.gesture = gesture;
  action.name_link.kind = ChatLinkKind::kMonster;
  action.name_link.item_id = slot.link_mob_id;
  action.name_link.token = slot.slot_id;
}

// LE DÉLAI HABITUEL — la loi du script de spawn, pas le tirage.
void DrawDelay(const MvpSlotView& slot) {
  const auto lowest = static_cast<int>(slot.delay1_ms / kMvpMsPerMinute);
  const auto highest = static_cast<int>((slot.delay1_ms + slot.delay2_ms) / kMvpMsPerMinute);
  if (lowest == highest) {
    ImGui::Text(core::Tr("%d min"), lowest);
  } else {
    ImGui::Text(core::Tr("%d-%d min"), lowest, highest);
  }
  SkinTooltip(
      core::Tr("Le délai écrit dans le script de spawn : au plus tôt, au plus tard.\n\nC'est une "
               "donnée publique, et la seule que le serveur ait le droit de publier — le tirage "
               "exact à l'intérieur de cette plage, lui, ne sort que si un membre l'a payé avec "
               "un Convex Mirror."));
}

// LE RETOUR, le même texte que la ligne détachée et le fantôme.
void DrawReturn(const MvpSlotView& slot, std::int64_t now_s) {
  const std::string text = MvpReturnText(slot, now_s);
  switch (MvpReturnToneOf(slot, now_s)) {
    case MvpReturnTone::kDim:
      ImGui::TextDisabled("%s", text.c_str());
      break;
    case MvpReturnTone::kPossible:
      // Vert FONCÉ : le corps d'une fenêtre est clair, un vert pastel y serait
      // presque blanc.
      ImGui::PushStyleColor(ImGuiCol_Text, kMvpPossibleTextColor);
      ImGui::TextUnformatted(text.c_str());
      ImGui::PopStyleColor();
      break;
    case MvpReturnTone::kPlain:
      ImGui::TextUnformatted(text.c_str());
      break;
  }
}

// LA SOURCE : un bouton, qui ouvre la saisie d'une heure. Une observation
// DÉPENSÉE — fenêtre refermée, instant exact dépassé — n'attend plus qu'on la
// lise mais qu'on la remplace : le bouton dit alors « saisir », et l'infobulle
// garde ce qu'elle fut.
void DrawSource(MvpTrackerWindowState& state, const MvpSlotView& slot, std::int64_t now_s,
                const SkinArt& art) {
  const bool spent =
      slot.window_known && (slot.exact ? slot.window_from_s <= now_s : slot.window_to_s <= now_s);
  const char* label =
      (slot.observed && !spent) ? MvpSourceLabel(slot.source_raw) : core::Tr("saisir");
  if (SkinSmallButton(label, art)) {
    state.manual_slot = slot.slot_id;
    state.manual_time.fill('\0');
    state.open_manual_popup = true;
  }
  if (!ImGui::IsItemHovered()) {
    return;
  }
  // QUI l'affirme, quand le serveur l'a dit : c'est ce qui sépare une
  // observation d'une rumeur.
  if (!slot.observed) {
    SkinTooltip(core::Tr("Saisir l'heure de mort observée."));
  } else if (!slot.by_name.empty()) {
    SkinTooltipFormat(core::Tr("%s — d'après %s.\nCliquer pour saisir une heure."),
                      MvpSourceLabel(slot.source_raw), std::string(slot.by_name).c_str());
  } else {
    SkinTooltipFormat(core::Tr("%s.\nCliquer pour saisir une heure."),
                      MvpSourceLabel(slot.source_raw));
  }
}

// LE FILTRE, s'il est montré. Éteint, son tampon est ignoré, pas vidé.
void DrawFilter(MvpTrackerWindowState& state, const SkinArt& art) {
  ImGui::SetNextItemWidth(Px(kMvpFilterWidthPixels));
  ImGui::InputTextWithHint("##mvp_filter", core::Tr("Filtrer : nom ou carte"), state.filter.data(),
                           state.filter.size());
  if (state.filter[0] != '\0') {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Effacer"), art)) {
      state.filter.fill('\0');
    }
  }
}

// LA COLONNE « DÉLAI » A DEUX MAÎTRES : le réglage, et le clic droit de
// l'en-tête. Un réglage qui change s'impose à la table ; une table qui change
// sans lui, c'est le clic droit, et il se rapporte pour que le réglage suive.
//
// ⚠ AVANT LA LIGNE D'EN-TÊTE : c'est elle qui met la table en page, et une
// demande posée avant prend effet dès cette image. L'état lu est donc celui de
// la mise en page PRÉCÉDENTE.
void SyncDelayColumn(MvpTrackerWindowState& state, MvpTrackerAction& action) {
  const bool wanted = state.look.show_delay;
  if (state.delay_applied != wanted) {
    ImGui::TableSetColumnEnabled(kMvpColumnDelay, wanted);
    state.delay_applied = wanted;
    return;
  }
  const bool enabled =
      (ImGui::TableGetColumnFlags(kMvpColumnDelay) & ImGuiTableColumnFlags_IsEnabled) != 0;
  if (enabled != wanted) {
    action.delay_shown = enabled;
    state.delay_applied = enabled;
  }
}

void SetupColumns(MvpTrackerWindowState& state, MvpTrackerAction& action) {
  ImGui::TableSetupScrollFreeze(0, 1);
  // La poignée et l'étoile restent en tête : déplacées, elles ne diraient plus
  // à quel nom elles appartiennent.
  ImGui::TableSetupColumn("##grip",
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed |
                              ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder,
                          Px(kMvpGripColumnPixels));
  // TRIABLE : son en-tête remonte les favoris.
  ImGui::TableSetupColumn("##fav",
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed |
                              ImGuiTableColumnFlags_NoReorder,
                          Px(kMvpFavoriteColumnPixels));
  ImGui::TableSetupColumn(core::Tr("MVP"),
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_DefaultSort);
  ImGui::TableSetupColumn(core::Tr("Carte"), ImGuiTableColumnFlags_NoHide);
  // La loi du script : une donnée de référence, pas une échéance. Montrée selon
  // le réglage, que le clic droit sur l'en-tête change aussi.
  ImGui::TableSetupColumn(core::Tr("Délai"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpDelayColumnPixels));
  // Une clé à part : le mot « Retour » sert ailleurs en d'autres sens, et se
  // traduit ici par celui d'une réapparition. L'en-tête coupe à `##`.
  ImGui::TableSetupColumn(core::Tr("Retour##mvp_respawn"),
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpReturnColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Âge"),
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpAgeColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Source"),
                          ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
                          Px(kMvpSourceColumnPixels));
  SyncDelayColumn(state, action);
  ImGui::TableHeadersRow();
}

// L'ORDRE D'AFFICHAGE, refait seulement quand le tri ou une empreinte change :
// le filtrage est refait à chaque image, il change la LISTE, comparée entière,
// empreintes ET places dans le catalogue. Une empreinte incomplète peut laisser
// un ordre périmé, jamais une place invalide.
void SortRows(MvpTrackerWindowState& state, const MvpTrackerView& view) {
  std::vector<std::size_t> rows;
  std::vector<MvpRowKey> keys;
  rows.reserve(view.slots.size());
  keys.reserve(view.slots.size());
  for (std::size_t index = 0; index < view.slots.size(); ++index) {
    const MvpSlotView& slot = view.slots[index];
    if (state.look.show_filter && !MvpMatchesFilter(slot, state.filter.data())) {
      continue;
    }
    rows.push_back(index);
    keys.push_back(RowKeyOf(slot));
  }
  const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  const bool sorted = specs != nullptr && specs->SpecsCount > 0;
  // Une clé par colonne ET par sens ; zéro : aucun tri.
  const int key = sorted
                      ? (specs->Specs[0].ColumnIndex + 1) * kMvpSortKeysPerColumn +
                            (specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending ? 1 : 0)
                      : 0;
  if (key == state.sort_key && keys == state.sorted_keys && rows == state.sorted_rows) {
    return;
  }
  state.sort_key = key;
  state.sorted_keys = std::move(keys);
  state.sorted_rows = rows;
  state.order = std::move(rows);
  if (!sorted) {
    return;
  }
  const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
  const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
  const int column = spec.ColumnIndex;
  std::ranges::stable_sort(state.order, [&](std::size_t left, std::size_t right) {
    return RowBefore(view.slots[left], view.slots[right], column, ascending);
  });
}

// UN RANG. ⚠ POUSSÉ SOUS UN NOM AVANT SON NUMÉRO : à la racine de la table, le
// numéro seul rendrait l'identifiant que l'en-tête pose déjà sur la colonne du
// même rang — l'étoile du créneau 1 entrerait en collision avec l'en-tête des
// favoris.
void DrawRow(MvpTrackerWindowState& state, const MvpTrackerView& view, const MvpSlotView& slot,
             float row_height, const SkinArt& art, MvpTrackerAction& action) {
  const std::int64_t now_s = view.server_now_s;
  ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
  ImGui::PushID("slot");
  ImGui::PushID(static_cast<int>(slot.slot_id));

  // LE RANG VISÉ : amené à l'écran une seule fois, puis teinté quelques
  // secondes. ⚠ L'ÂGE SE TESTE AVANT DE DÉFILER : une demande périmée ne fait
  // plus bouger la table.
  if (state.focus_slot != kMvpNoSlotView && slot.slot_id == state.focus_slot) {
    if (view.now_ms - state.focus_ms >= kMvpFocusMs) {
      state.focus_slot = kMvpNoSlotView;
    } else {
      if (!state.focus_scrolled) {
        ImGui::SetScrollHereY(kMvpFocusScrollRatio);
        state.focus_scrolled = true;
      }
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(kMvpFocusRowColor));
    }
  }

  ImGui::TableSetColumnIndex(kMvpColumnGrip);
  DrawGrip(state, slot);

  ImGui::TableSetColumnIndex(kMvpColumnFavorite);
  DrawFavoriteButton(view, slot, art, action);

  ImGui::TableSetColumnIndex(kMvpColumnName);
  if (state.look.show_sprites && slot.sprite_mob_id != 0) {
    DrawRowSprite(state, view, slot.sprite_mob_id);
    ImGui::SameLine();
  }
  DrawName(state, slot, action);

  ImGui::TableSetColumnIndex(kMvpColumnMap);
  Unformatted(MvpMapShown(slot));
  if (ImGui::IsItemHovered()) {
    DrawMapPreview(state, view, slot);
  }

  // Masquée, la colonne rend faux, et l'on ne dessine rien.
  if (ImGui::TableSetColumnIndex(kMvpColumnDelay)) {
    DrawDelay(slot);
  }

  ImGui::TableSetColumnIndex(kMvpColumnReturn);
  DrawReturn(slot, now_s);

  ImGui::TableSetColumnIndex(kMvpColumnAge);
  if (slot.observed) {
    ImGui::TextDisabled("%s", FormatMvpDuration(now_s - slot.reported_at_s).c_str());
  }

  ImGui::TableSetColumnIndex(kMvpColumnSource);
  DrawSource(state, slot, now_s, art);

  ImGui::PopID();
  ImGui::PopID();
}

// LE MENU DU NOM ET L'OUVERTURE DE LA SAISIE, après la table et HORS des
// identifiants de rang : l'identifiant d'un surgissant se hache avec la pile
// courante, et ouvert sous un rang il ne serait jamais retrouvé.
void DrawAfterTable(MvpTrackerWindowState& state, const MvpTrackerView& view,
                    MvpTrackerAction& action) {
  if (state.menu_requested) {
    ImGui::OpenPopup("##mvp_link_menu");
    state.menu_requested = false;
  }
  const MvpSlotView* slot = FindSlotView(view.slots, state.menu_slot);
  const MvpLinkMenuFacts facts{
      .tomb_known = slot != nullptr && slot->observed && slot->tomb_x >= 0 && slot->tomb_y >= 0 &&
                    !slot->map.empty(),
      .carries_respawn = slot != nullptr && (slot->kill_time_s > 0 || slot->exact_respawn_s > 0),
      .named = slot != nullptr && !slot->label.empty(),
      .map_known = slot != nullptr && !slot->map.empty()};
  const ChatLinkAsk asked = DrawChatLinkMenu(
      "##mvp_link_menu", ChatLinkKind::kMonster, slot != nullptr ? slot->link_mob_id : 0U,
      state.menu_slot, slot != nullptr ? slot->label : std::string_view(), {}, nullptr, &facts);
  if (asked.gesture != ChatLinkGesture::kNothing) {
    action.name_link = asked;
  }
  if (state.open_manual_popup) {
    ImGui::OpenPopup("##mvp_manual");
    state.open_manual_popup = false;
  }
}

}  // namespace

void DrawRowSprite(MvpTrackerWindowState& state, const MvpTrackerView& view, std::uint16_t mob_id) {
  const float side = Px(kMvpSpritePixels);
  state.wanted_sprites.push_back(mob_id);
  const auto found = std::ranges::find(view.sprites, mob_id, &MvpSpriteArt::mob_id);
  if (found != view.sprites.end() && found->image.usable()) {
    ImGui::Image(found->image.id, ImVec2(side, side));
  } else {
    // La case est réservée même vide : un monstre sans sprite — un modèle 3D —
    // garde sa place, et le rang sa hauteur.
    ImGui::Dummy(ImVec2(side, side));
  }
}

void DrawTable(MvpTrackerWindowState& state, const MvpTrackerView& view, const SkinArt& art,
               MvpTrackerAction& action) {
  if (!view.catalog_known) {
    // Une table vide voudrait dire « rien à chasser » : on dit plutôt la vérité.
    ImGui::TextDisabled("%s", core::Tr("Catalogue non reçu — en attente du serveur."));
    return;
  }
  // 🔴 SANS GROUPE, PAS DE TABLE : les observations appartiennent à un groupe,
  // et quatre-vingts lignes « jamais vu » promettraient un outil qui marche
  // seul. On dit plutôt ce qu'il faut faire.
  if (view.group_id == 0) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", core::Tr("Le carnet se remplit de ce que VOTRE GROUPE observe : ses "
                                      "kills, les tombes qu'il lit, les Convex Mirror qu'il "
                                      "porte. Sans groupe, aucune ligne ne se remplira."));
    ImGui::Spacing();
    ImGui::TextWrapped("%s", core::Tr("Créez-en un ci-dessus — vous pouvez y chasser seul, et "
                                      "inviter plus tard vos amis et vos compagnons de guilde."));
    return;
  }
  if (state.look.show_filter) {
    DrawFilter(state, art);
    // Une table vide sous un filtre semblerait cassée : on dit qu'elle est vide.
    const std::string_view filter = state.filter.data();
    if (!filter.empty() && std::ranges::none_of(view.slots, [&](const MvpSlotView& slot) {
          return MvpMatchesFilter(slot, filter);
        })) {
      ImGui::TextDisabled("%s", core::Tr("Aucun résultat."));
    }
  }
  // `Hideable` : le clic droit sur l'en-tête donne la liste des colonnes à
  // cocher, dont « Délai ».
  constexpr ImGuiTableFlags kSlotTableFlags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Sortable |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##mvp_slots", kMvpColumnCount, kSlotTableFlags)) {
    return;
  }
  SetupColumns(state, action);
  SortRows(state, view);

  // 🔴 LE DÉCOUPAGE EST OBLIGATOIRE : il borne les sprites demandés au visible.
  // La hauteur donnée est aussi un PLANCHER de chaque rang, si bien qu'un rang
  // sans sprite ne raccourcit pas et que le défilement ne dérive pas.
  const float row_height = MvpTableRowHeight(state.look.show_sprites, view, art);
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(state.order.size()), row_height);
  // Le rang visé doit être SOUMIS même hors écran : on ne défile pas vers un
  // rang qui n'a pas été dessiné.
  if (state.focus_slot != kMvpNoSlotView && !state.focus_scrolled) {
    const auto found = std::ranges::find_if(state.order, [&](std::size_t index) {
      return view.slots[index].slot_id == state.focus_slot;
    });
    if (found != state.order.end()) {
      clipper.IncludeItemByIndex(static_cast<int>(found - state.order.begin()));
    } else {
      // Introuvable — filtre actif, catalogue plus court : on n'insiste pas.
      state.focus_slot = kMvpNoSlotView;
    }
  }
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const MvpSlotView& slot = view.slots[state.order[static_cast<std::size_t>(row)]];
      DrawRow(state, view, slot, row_height, art, action);
    }
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
  DrawAfterTable(state, view, action);
}

}  // namespace ro::ui::mvp_tracker
