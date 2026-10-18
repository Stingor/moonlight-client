// L'ALBUM DE CARTES : le classeur — deux pages, la reliure, et les pochettes.

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "card_album_parts.h"
#include "ui_popup.h"
#include "ui_scale.h"

namespace ro::ui::card_album {
namespace {

// Le côté, en pixels ENTIERS, de l'icône devant un nom : une icône d'objet est
// une texture de 24 sur 24, posée texel pour pixel.
[[nodiscard]] float NameIconSide() { return std::floor(Px(kNameIconPixels)); }

// La vignette que l'appelant a préparée pour cette pochette, ou rien.
[[nodiscard]] const CardAlbumArt* ArtFor(const CardAlbumView& view, std::uint32_t card_id,
                                         bool silhouette) {
  for (const CardAlbumArt& art : view.art) {
    if (art.card_id == card_id && art.silhouette == silhouette) {
      return &art;
    }
  }
  return nullptr;
}

// Un texte sur UNE ligne, centré dans [left, right], la police réduite pour
// tenir — jusqu'à un plancher, au-delà duquel on coupe : un nom illisible ne
// vaut pas mieux qu'un nom tronqué.
//
// L'icône (facultative) se pose DEVANT le texte, et l'ensemble se centre d'un
// bloc : c'est la place restante qui borne la police.
void DrawFittedText(ImDrawList* into, float left, float right, float top, ImU32 color,
                    const std::string& text, const SkinImage* icon, ImU32 icon_tint) {
  ImFont* font = ImGui::GetFont();
  const float base = ImGui::GetFontSize();
  const float room = right - left;
  if (room <= 1.0F) {
    return;
  }
  const bool with_icon = icon != nullptr && icon->usable();
  const float side = with_icon ? NameIconSide() : 0.0F;
  const float gap = with_icon ? Px(kNameIconGapPixels) : 0.0F;
  const float band = std::max(base, side);
  const float text_room = std::max(1.0F, room - side - gap);
  float width = font->CalcTextSizeA(base, FLT_MAX, 0.0F, text.c_str()).x;
  float size = base;
  if (width > text_room && width > 0.0F) {
    size = std::max(base * kNameLeastScale, base * text_room / width);
    width = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.c_str()).x;
  }
  float x = left + std::max(0.0F, (room - (side + gap + width)) * 0.5F);
  into->PushClipRect(ImVec2(left, top - 1.0F), ImVec2(right, top + band + 2.0F), true);
  if (with_icon) {
    // Sur des coordonnées entières : posée sur un demi-pixel, la petite texture
    // devient floue.
    const ImVec2 at(std::floor(x), std::floor(top + (band - side) * 0.5F));
    into->AddImage(icon->id, at, ImVec2(at.x + side, at.y + side), ImVec2(0, 0), ImVec2(1, 1),
                   icon_tint);
    x += side + gap;
  }
  into->AddText(font, size, ImVec2(x, top + (band - size) * 0.5F), color, text.c_str());
  into->PopClipRect();
}

// UNE PASTILLE : un fond soutenu et un texte dessus — c'est ainsi qu'une mention
// saillit sur un fond clair. `anchor` dit quel point de la pastille se pose sur
// `at` (0,0 = coin haut gauche ; 1,0 = haut droit ; 0.5,1 = bas centre).
void DrawPill(ImDrawList* into, ImVec2 at, ImVec2 anchor, const char* text, ImU32 fill,
              ImU32 text_color, float alpha) {
  constexpr float kPillFontScale = 0.9F;
  constexpr float kPillPadXPixels = 6.0F;
  constexpr float kPillPadYPixels = 1.5F;
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize() * kPillFontScale;
  const ImVec2 measured = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text);
  const float pad_x = Px(kPillPadXPixels);
  const float pad_y = Px(kPillPadYPixels);
  const ImVec2 box(measured.x + 2.0F * pad_x, measured.y + 2.0F * pad_y);
  const ImVec2 top_left(std::floor(at.x - box.x * anchor.x), std::floor(at.y - box.y * anchor.y));
  into->AddRectFilled(top_left, ImVec2(top_left.x + box.x, top_left.y + box.y),
                      WithAlpha(fill, alpha), box.y * 0.5F);
  into->AddText(font, size, ImVec2(top_left.x + pad_x, top_left.y + pad_y),
                WithAlpha(text_color, alpha), text);
}

// La carte du sac que le glisser en cours emporte, ou zéro.
[[nodiscard]] std::uint32_t CarriedHandCard() {
  const ImGuiPayload* payload = ImGui::GetDragDropPayload();
  if (payload == nullptr || !payload->IsDataType(kItemDragPayload) ||
      payload->DataSize != static_cast<int>(sizeof(ItemDrag))) {
    return 0;
  }
  const auto& carried = *static_cast<const ItemDrag*>(payload->Data);
  return carried.from == ItemHolder::kInventory && !carried.worn ? carried.item_id : 0;
}

// Menu d'une pochette — les entrées et les mots du coffre : « (1) », « (tout :
// N) », « … » pour une quantité libre. S'y ajoute ce que le sac permet pour
// CETTE carte : la ranger, ou la sacrifier.
void DrawPocketMenu(CardAlbumWindowState& state, const CardAlbumView& view,
                    const CardAlbumPocketView& pocket, const std::string& shown,
                    CardAlbumWindowAction& action) {
  ImGui::TextDisabled("%s", shown.c_str());
  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Description"))) {
    action.describe_id = pocket.card_id;
  }
  if (ImGui::MenuItem(core::Tr("Voir dans la base de données"))) {
    action.database_id = pocket.card_id;
  }
  std::array<char, 96> label = {};
  if (pocket.unlocked && pocket.reserve > 0) {
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Vers l'inventory (1)"))) {
      ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket.card_id, 1, false, action);
    }
    if (pocket.reserve > 1) {
      std::snprintf(label.data(), label.size(), core::Tr("Vers l'inventory (tout : %d)"),
                    static_cast<int>(pocket.reserve));
      if (ImGui::MenuItem(label.data())) {
        ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket.card_id, pocket.reserve, false, action);
      }
      if (ImGui::MenuItem(core::Tr("Vers l'inventory..."))) {
        ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket.card_id, pocket.reserve, true, action);
      }
    }
  }
  // La carte est aussi dans le sac : proposer le geste inverse depuis la pochette.
  const CardAlbumHandCard* held = nullptr;
  for (const CardAlbumHandCard& card : view.hand) {
    if (card.card_id == pocket.card_id) {
      held = &card;
      break;
    }
  }
  if (held == nullptr) {
    return;
  }
  ImGui::Separator();
  if (!pocket.unlocked) {
    if (ImGui::MenuItem(core::Tr("Sacrifier une copie..."))) {
      AskSacrifice(state, held->client_slot, held->card_id, held->amount - 1, action);
    }
    return;
  }
  std::snprintf(label.data(), label.size(), core::Tr("Depuis l'inventory (1 sur %d)"),
                held->amount);
  if (ImGui::MenuItem(label.data())) {
    ArmMove(state, CardAlbumVerb::kDeposit, held->client_slot, held->card_id, 1, false, action);
  }
  if (held->amount > 1) {
    std::snprintf(label.data(), label.size(), core::Tr("Depuis l'inventory (tout : %d)"),
                  held->amount);
    if (ImGui::MenuItem(label.data())) {
      ArmMove(state, CardAlbumVerb::kDeposit, held->client_slot, held->card_id, held->amount, false,
              action);
    }
    if (ImGui::MenuItem(core::Tr("Depuis l'inventory..."))) {
      ArmMove(state, CardAlbumVerb::kDeposit, held->client_slot, held->card_id, held->amount, true,
              action);
    }
  }
}

// Ce qu'un macaron offre à cette image.
enum class ChipGesture : std::uint8_t { kOpen, kAdd, kRemove, kCancel };

// Les MACARONS de provenance, au coin haut gauche de la pochette. Rend vrai
// quand la souris est sur l'un d'eux ; `right_clicked` dit si le clic DROIT y
// est tombé — il appartient alors à la POCHETTE.
bool DrawSourceChips(CardAlbumWindowState& state, const CardAlbumView& view,
                     const CardAlbumPocketView& pocket, ImDrawList* into, ImVec2 pocket_top_left,
                     float alpha, bool dragging, bool* right_clicked,
                     CardAlbumWindowAction& action) {
  constexpr float kChipFontScale = 0.85F;
  constexpr float kChipPadPixels = 8.0F;
  constexpr float kInsetPixels = 3.0F;
  constexpr float kFaded = 0.45F;
  constexpr float kFadedText = 0.8F;
  // ÉDITION STAFF, AU GESTE : Maj change les macarons présents en « -O » /
  // « -M » (demander le RETRAIT), Ctrl fait paraître « +O » / « +M » sur les
  // albums où la carte N'EST PAS (demander l'AJOUT). 🔴 RIEN NE CHANGE DANS LE
  // JEU : c'est un bon de travail qu'un script portera au serveur.
  const ImGuiIO& io = ImGui::GetIO();
  const bool want_remove = view.staff && io.KeyShift;
  const bool want_add = view.staff && io.KeyCtrl;
  const bool editing = want_remove || want_add;
  const float chip = Px(kSourceChipPixels);
  float x = pocket_top_left.x + Px(kInsetPixels);
  const float y = pocket_top_left.y + Px(kInsetPixels);
  bool on_chip = false;
  for (std::size_t rank = 0; rank < kCardAlbumSourceCount && rank < view.sources.size(); ++rank) {
    const CardAlbumSourceView& source = view.sources[rank];
    const CardAlbumSourceMark& mark = pocket.sources[rank];
    // L'intention posée sur ce couple ne se lit que chez le staff.
    const bool pending_add = view.staff && mark.intent == CardAlbumIntent::kAdd;
    const bool pending_remove = view.staff && mark.intent == CardAlbumIntent::kRemove;
    const bool pending = pending_add || pending_remove;
    // Une intention en attente se montre TOUJOURS — c'est du travail à faire ;
    // un fantôme « + » ne s'invite que sous Ctrl.
    const bool ghost = !mark.present && !pending_add;
    if (ghost && !want_add) {
      continue;
    }
    // Annuler prime : sur une intention posée, le modificateur sert à la
    // RETIRER, sinon il n'y aurait aucun moyen de revenir en arrière.
    ChipGesture gesture = ChipGesture::kOpen;
    if (editing && pending) {
      gesture = ChipGesture::kCancel;
    } else if (want_add && !mark.present) {
      gesture = ChipGesture::kAdd;
    } else if (want_remove && mark.present) {
      gesture = ChipGesture::kRemove;
    }
    // ⚠ LE VRAI SIGNE MOINS N'EST PAS DANS LA POLICE : trait d'union ASCII. Le
    // « × » (U+00D7), lui, y est.
    std::string glyph(source.letter);
    if (gesture == ChipGesture::kAdd) {
      glyph = "+" + glyph;
    } else if (gesture == ChipGesture::kRemove) {
      glyph = "-" + glyph;
    } else if (gesture == ChipGesture::kCancel) {
      glyph = "\xC3\x97" + glyph;
    }
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * kChipFontScale;
    const ImVec2 measured = font->CalcTextSizeA(size, FLT_MAX, 0.0F, glyph.c_str());
    const float width = std::max(chip, measured.x + Px(kChipPadPixels));
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::PushID(static_cast<int>(rank));
    const bool hit = ImGui::InvisibleButton("##source", ImVec2(width, chip));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    on_chip = on_chip || hovered;
    // Le macaron ne mange que le clic GAUCHE : un menu contextuel qui s'ouvre
    // partout sauf sur un coin serait un défaut.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      *right_clicked = true;
    }
    // Un fantôme est CREUX : il dit « elle pourrait », pas « elle tombe de là ».
    const ImVec2 a(x, y);
    const ImVec2 b(x + width, y + chip);
    const float round = chip * 0.5F;
    const float body = ghost || pending_remove ? kFaded : 1.0F;
    into->AddRectFilled(ImVec2(a.x, a.y + Px(1.0F)), ImVec2(b.x, b.y + Px(1.0F)),
                        WithAlpha(kPocketShadow, alpha * body), round);
    into->AddRectFilled(a, b, WithAlpha(kSourceFill[rank], alpha * body), round);
    if (hovered) {
      into->AddRectFilled(a, b, kHoverTint, round);
    }
    const ImU32 rim = pending_add || gesture == ChipGesture::kAdd         ? kSourcePendingAdd
                      : pending_remove || gesture == ChipGesture::kRemove ? kSourcePendingRemove
                                                                          : kSourceEdge;
    into->AddRect(a, b, WithAlpha(rim, alpha), round, 0, rim == kSourceEdge ? 1.0F : Px(2.0F));
    into->AddText(font, size,
                  ImVec2(std::floor((a.x + b.x - measured.x) * 0.5F),
                         std::floor((a.y + b.y - measured.y) * 0.5F)),
                  WithAlpha(kSourceText, alpha * (body < 1.0F ? kFadedText : 1.0F)), glyph.c_str());
    if (hovered && !dragging) {
      if (BeginSkinTooltip()) {
        const std::string name(source.name);
        if (mark.present) {
          ImGui::Text("%s : %.4f%%", name.c_str(), mark.percent);
          if (mark.total > 0) {
            ImGui::TextColored(kLabelColor, core::Tr("%d chances sur %d"), mark.weight, mark.total);
          }
        } else {
          ImGui::TextUnformatted(name.c_str());
          ImGui::TextColored(kLabelColor, "%s", core::Tr("Cette carte n'y est pas."));
        }
        // 🔴 DIRE QUE C'EST UN BON DE TRAVAIL, pas un changement : un macaron
        // d'attente qui se lirait comme un fait ferait croire l'album modifié.
        if (pending_add) {
          ImGui::TextColored(kWarningColor, core::Tr("À AJOUTER (poids %d) — en attente du script"),
                             mark.intent_rate);
        } else if (pending_remove) {
          ImGui::TextColored(kWarningColor, "%s", core::Tr("À RETIRER — en attente du script"));
        }
        const char* tip = core::Tr("Clic : ouvrir sa description et sa table de tirage");
        if (gesture == ChipGesture::kAdd) {
          tip = core::Tr("Clic : demander son AJOUT à cet album");
        } else if (gesture == ChipGesture::kRemove) {
          tip = core::Tr("Clic : demander son RETRAIT de cet album");
        } else if (gesture == ChipGesture::kCancel) {
          tip = core::Tr("Clic : annuler cette demande");
        }
        ImGui::TextColored(kLabelColor, "%s", tip);
        if (view.staff && !editing) {
          ImGui::TextColored(kLabelColor, "%s", core::Tr("Staff — Ctrl : ajouter · Maj : retirer"));
        }
        EndSkinTooltip();
      }
    }
    if (hit) {
      switch (gesture) {
        case ChipGesture::kAdd:
          // Le poids se demande : la boîte vit au niveau de la fenêtre.
          state.rate_album = source.item_id;
          state.rate_card = pocket.card_id;
          state.rate_value = source.typical_rate > 0 ? source.typical_rate : kCardAlbumRateLeast;
          state.rate_asked = true;
          break;
        case ChipGesture::kRemove:
          action.staff = CardAlbumStaffAsk{.album_id = source.item_id,
                                           .card_id = pocket.card_id,
                                           .intent = CardAlbumIntent::kRemove};
          break;
        case ChipGesture::kCancel:
          action.staff = CardAlbumStaffAsk{
              .album_id = source.item_id, .card_id = pocket.card_id, .cancel = true};
          break;
        case ChipGesture::kOpen:
          // La description de l'album porte sa table de tirage complète, dont ce
          // macaron n'est qu'un extrait.
          action.describe_id = source.item_id;
          break;
      }
    }
    x += width + Px(kSourceChipGapPixels);
  }
  return on_chip;
}

void DrawPocket(CardAlbumWindowState& state, const CardAlbumView& view, ImDrawList* into, ImVec2 at,
                const BookLayout& layout, int order_rank, float alpha, const SkinLook& look,
                CardAlbumWindowAction& action) {
  const int rank = state.order[static_cast<std::size_t>(order_rank)];
  const CardAlbumPocketView& pocket = view.pockets[static_cast<std::size_t>(rank)];
  const std::string shown =
      CardAlbumShortName(pocket.name.empty() ? std::to_string(pocket.card_id) : pocket.name);
  const float margin = Px(kPocketMarginPixels);
  ImGui::PushID(rank);

  // UN SEUL ÉLÉMENT pour toute la cellule : c'est lui qui porte survol, clics,
  // menu et glisser. ⚠ `SetNextItemAllowOverlap` : sans lui, les macarons soumis
  // PAR-DESSUS ne recevraient jamais le survol.
  ImGui::SetCursorScreenPos(at);
  if (state.look.sources) {
    ImGui::SetNextItemAllowOverlap();
  }
  const bool pressed =
      ImGui::InvisibleButton("##pochette", ImVec2(layout.cell_width, layout.cell_height));
  const bool hovered = ImGui::IsItemHovered();
  // 🔴 LE CLIC DROIT SE LIT ICI, tant que la pochette est le DERNIER élément
  // soumis : lu après les macarons, il interrogerait le macaron.
  const bool right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
  const bool dragging = ImGui::GetDragDropPayload() != nullptr;

  const ImVec2 p0 = at;
  const ImVec2 p1(at.x + layout.cell_width, at.y + layout.art_height + 2.0F * margin);
  const ImVec2 a0(p0.x + margin, p0.y + margin);
  const ImVec2 a1(a0.x + layout.art_width, a0.y + layout.art_height);
  const float pocket_round = Px(kPocketRoundingPixels);
  const float art_round = Px(kArtRoundingPixels);

  // La pochette : une ombre, le plastique sombre, son reflet.
  into->AddRectFilled(ImVec2(p0.x, p0.y + Px(2.0F)), ImVec2(p1.x, p1.y + Px(2.0F)),
                      WithAlpha(kPocketShadow, alpha), pocket_round);
  into->AddRectFilled(p0, p1, WithAlpha(kPocketColor, alpha), pocket_round);
  // Le liseré de nature. Le reflet ne se pose que sur une pochette ordinaire :
  // par-dessus un liseré de couleur, il en délaverait le milieu.
  if (state.look.dropper_rim && pocket.dropper_rank != 0) {
    constexpr std::uint8_t kMvpRank = 2;
    into->AddRect(p0, p1,
                  WithAlpha(pocket.dropper_rank == kMvpRank ? kRimMvp : kRimMiniBoss, alpha),
                  pocket_round, 0, Px(kRimPixels));
  } else {
    into->AddRect(p0, p1, WithAlpha(kRimOrdinary, alpha), pocket_round);
    into->AddRect(p0, p1, WithAlpha(kPocketGlint, alpha), pocket_round);
  }

  // L'illustration — en couleur si la pochette est ouverte, en silhouette sinon.
  const bool silhouette = !pocket.unlocked;
  state.wanted.push_back(CardAlbumArtAsk{.card_id = pocket.card_id, .silhouette = silhouette});
  const CardAlbumArt* art = ArtFor(view, pocket.card_id, silhouette);
  const bool picture_fits = art != nullptr && art->picture.usable() &&
                            view.art_width == static_cast<int>(layout.art_width) &&
                            view.art_height == static_cast<int>(layout.art_height);
  // ⚠ LA LUMINOSITÉ SEULE : une illustration ne prend pas la teinte des images
  // de la fenêtre, elle en serait dénaturée.
  const ImU32 tint = WithAlpha(BrightnessTint(look.art_brightness), alpha);
  if (picture_fits) {
    // Texel pour pixel, sur des coordonnées ENTIÈRES. Les quelques illustrations
    // plus courtes que la pochette se centrent.
    const float w = art->picture.width;
    const float h = art->picture.height;
    const ImVec2 f0(std::floor(a0.x + (layout.art_width - w) * 0.5F),
                    std::floor(a0.y + (layout.art_height - h) * 0.5F));
    into->AddImageRounded(art->picture.id, f0, ImVec2(f0.x + w, f0.y + h), ImVec2(0, 0),
                          ImVec2(1, 1), tint, art_round);
  } else {
    // Pas d'illustration, ou pas encore : un fond uni et l'icône agrandie.
    into->AddRectFilled(a0, a1, WithAlpha(kArtMissing, alpha), art_round);
    if (art != nullptr && art->icon.usable()) {
      const float side = std::min(layout.art_width, layout.art_height) * 0.5F;
      const ImVec2 middle((a0.x + a1.x) * 0.5F, (a0.y + a1.y) * 0.5F);
      into->AddImage(art->icon.id, ImVec2(middle.x - side * 0.5F, middle.y - side * 0.5F),
                     ImVec2(middle.x + side * 0.5F, middle.y + side * 0.5F), ImVec2(0, 0),
                     ImVec2(1, 1), pocket.unlocked ? tint : WithAlpha(kSealedIconTint, alpha));
    }
  }
  if (hovered && !dragging) {
    into->AddRectFilled(a0, a1, kHoverTint, art_round);
  }

  // La pastille de réserve, ou le cachet.
  constexpr float kBadgeInsetPixels = 3.0F;
  constexpr float kSealLiftPixels = 6.0F;
  if (pocket.unlocked) {
    const std::string reserve = "\xC3\x97" + std::to_string(pocket.reserve);
    DrawPill(into, ImVec2(p1.x - Px(kBadgeInsetPixels), p0.y + Px(kBadgeInsetPixels)),
             ImVec2(1.0F, 0.0F), reserve.c_str(), pocket.reserve > 0 ? kBadgeStocked : kBadgeEmpty,
             kBlack, alpha);
  } else {
    DrawPill(into, ImVec2((p0.x + p1.x) * 0.5F, p1.y - Px(kSealLiftPixels)), ImVec2(0.5F, 1.0F),
             core::Tr("scellée"), kSealFill, kSealText, alpha);
  }

  // La carte que le dernier ordre a touchée : un liseré vert qui bat.
  if (pocket.card_id == view.highlight_card && view.highlight_card != 0) {
    constexpr float kPulseBase = 0.55F;
    constexpr float kPulseSwing = 0.45F;
    constexpr double kPulsePerSecond = 8.0;
    const float pulse =
        kPulseBase + kPulseSwing * static_cast<float>(std::sin(ImGui::GetTime() * kPulsePerSecond));
    into->AddRect(ImVec2(p0.x - 1.0F, p0.y - 1.0F), ImVec2(p1.x + 1.0F, p1.y + 1.0F),
                  WithAlpha(kHighlight, pulse * alpha), pocket_round, 0, Px(2.5F));
  }

  // Le nom, sous la pochette — précédé de l'icône si le joueur l'a demandée.
  // Elle garde ses couleurs sous une pochette scellée : c'est la SILHOUETTE qui
  // dit ce qu'on n'a pas.
  const ImU32 name_color = pocket.unlocked ? ImGui::GetColorU32(ImGuiCol_Text)
                                           : ImGui::ColorConvertFloat4ToU32(kLabelColor);
  const SkinImage* icon = state.look.name_icon && art != nullptr ? &art->icon : nullptr;
  DrawFittedText(into, p0.x, p1.x, p1.y + Px(2.0F), WithAlpha(name_color, alpha), shown, icon,
                 tint);

  // Une carte du sac passe au-dessus de SA pochette : le dire, en vert, pendant
  // qu'il est encore temps. Le dépôt lui-même se prend sur toute la fenêtre.
  if (dragging && CarriedHandCard() == pocket.card_id &&
      ImGui::IsMouseHoveringRect(p0, ImVec2(p1.x, at.y + layout.cell_height))) {
    into->AddRect(ImVec2(p0.x - 1.0F, p0.y - 1.0F), ImVec2(p1.x + 1.0F, p1.y + 1.0F), kDropWelcome,
                  pocket_round, 0, Px(2.5F));
  }

  // La pochette part vers le sac : le retrait. Rien à emporter d'une pochette
  // scellée ou vide. ⚠ AVANT LES MACARONS : la source s'attache au DERNIER
  // élément soumis, et c'est la pochette qu'on promène, pas son coin.
  if (pocket.unlocked && pocket.reserve > 0 &&
      ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    ImGui::SetDragDropPayload(kCardAlbumDragPayload, &pocket.card_id, sizeof(pocket.card_id));
    constexpr float kGhostPixels = 64.0F;
    if (picture_fits) {
      const float h = Px(kGhostPixels);
      ImGui::Image(art->picture.id, ImVec2(h * art->picture.width / art->picture.height, h));
      ImGui::SameLine();
    }
    ImGui::TextUnformatted(shown.c_str());
    ImGui::TextColored(kLabelColor, "%s", core::Tr("Lâcher sur l'inventory pour reprendre"));
    ImGui::EndDragDropSource();
  }

  bool chip_right_click = false;
  bool on_chip = false;
  if (state.look.sources) {
    on_chip =
        DrawSourceChips(state, view, pocket, into, p0, alpha, dragging, &chip_right_click, action);
  }

  // Survol : la description, avec la carte en grand.
  if (hovered && !dragging && !on_chip) {
    action.hovered_id = pocket.card_id;
  }

  // Clic GAUCHE : Maj → tout reprendre ; une seule copie → retrait direct ;
  // sinon le menu. Une pochette scellée ou vide ouvre son menu.
  const ImGuiIO& io = ImGui::GetIO();
  const bool stocked = pocket.unlocked && pocket.reserve > 0;
  if (pressed) {
    if (stocked && (io.KeyShift || pocket.reserve <= 1)) {
      ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket.card_id, pocket.reserve, false, action);
    } else {
      ImGui::OpenPopup("##menu");
    }
  }
  // Clic DROIT : Ctrl → la description ; Alt ou Maj → tout reprendre ; sinon le
  // menu.
  if (right_clicked || chip_right_click) {
    if (io.KeyCtrl) {
      action.describe_id = pocket.card_id;
    } else if ((io.KeyAlt || io.KeyShift) && stocked) {
      ArmMove(state, CardAlbumVerb::kWithdraw, 0, pocket.card_id, pocket.reserve, false, action);
    } else {
      ImGui::OpenPopup("##menu");
    }
  }
  if (BeginSkinPopup("##menu")) {
    DrawPocketMenu(state, view, pocket, shown, action);
    EndSkinPopup();
  }
  ImGui::PopID();
}

void DrawPage(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
              ImDrawList* into, ImVec2 top_left, const BookLayout& layout, int first, bool left,
              float alpha, CardAlbumWindowAction& action) {
  const float pad = Px(kPagePadPixels);
  const float gap = Px(kPocketGapPixels);
  const ImVec2 bottom_right(top_left.x + layout.page_width, top_left.y + layout.page_height);
  const float round = Px(3.0F);
  constexpr float kFoldPixels = 16.0F;
  // Le papier, puis l'ombre du pli côté reliure.
  into->AddRectFilled(top_left, bottom_right, art.look().card, round);
  const float fold = Px(kFoldPixels);
  if (left) {
    into->AddRectFilledMultiColor(ImVec2(bottom_right.x - fold, top_left.y), bottom_right, kClear,
                                  kFoldShadow, kFoldShadow, kClear);
  } else {
    into->AddRectFilledMultiColor(top_left, ImVec2(top_left.x + fold, bottom_right.y), kFoldShadow,
                                  kClear, kClear, kFoldShadow);
  }
  into->AddRect(top_left, bottom_right, kPageEdge, round);

  // Les pochettes, centrées dans la page.
  const float grid_width = static_cast<float>(layout.columns) * layout.cell_width +
                           static_cast<float>(layout.columns - 1) * gap;
  const float x0 =
      top_left.x + pad + std::max(0.0F, (layout.page_width - 2.0F * pad - grid_width) * 0.5F);
  const float y0 = top_left.y + pad;
  const int total = static_cast<int>(state.order.size());
  for (int slot = 0; slot < layout.per_page; ++slot) {
    const int rank = first + slot;
    if (rank >= total) {
      break;
    }
    const int column = slot % layout.columns;
    const int row = slot / layout.columns;
    const ImVec2 at(std::floor(x0 + static_cast<float>(column) * (layout.cell_width + gap)),
                    std::floor(y0 + static_cast<float>(row) * (layout.cell_height + gap)));
    DrawPocket(state, view, into, at, layout, rank, alpha, art.look(), action);
  }

  // Le pied : la flèche du côté extérieur, le numéro au centre. « < » et « > »
  // en ASCII : les triangles ne sont pas dans la police.
  constexpr float kFooterLiftPixels = 4.0F;
  constexpr float kArrowLeastPixels = 26.0F;
  constexpr float kArrowDropPixels = 2.0F;
  const float footer_top = bottom_right.y - ImGui::GetFrameHeight() - Px(kFooterLiftPixels);
  const int page = layout.per_page > 0 ? first / layout.per_page + 1 : 1;
  const std::string number = std::to_string(page);
  const ImVec2 measured = ImGui::CalcTextSize(number.c_str());
  into->AddText(ImVec2(top_left.x + (layout.page_width - measured.x) * 0.5F,
                       footer_top + (ImGui::GetFrameHeight() - measured.y) * 0.5F),
                ImGui::ColorConvertFloat4ToU32(kLabelColor), number.c_str());
  const float arrow = std::max(Px(kArrowLeastPixels), SkinSmallButtonWidth("<", art));
  if (left && state.first > 0) {
    ImGui::SetCursorScreenPos(ImVec2(top_left.x + pad, footer_top + Px(kArrowDropPixels)));
    if (SkinSmallButton("<", art, ImVec2(arrow, 0.0F))) {
      Flip(state, -1);
    }
  }
  if (!left && first + layout.per_page < total) {
    ImGui::SetCursorScreenPos(
        ImVec2(bottom_right.x - pad - arrow, footer_top + Px(kArrowDropPixels)));
    if (SkinSmallButton(">", art, ImVec2(arrow, 0.0F))) {
      Flip(state, 1);
    }
  }
}

// Une carte du sac lâchée n'importe où sur la fenêtre : l'offrir à l'album.
// ⚠ TOUTE LA FENÊTRE EST CIBLE, pas seulement la pochette : c'est l'album qui
// sait où la carte va — dépôt ou sacrifice.
void TakeHandDrop(CardAlbumWindowState& state, const CardAlbumView& view,
                  CardAlbumWindowAction& action) {
  ImGuiWindow* window = ImGui::GetCurrentWindow()->RootWindow;
  if (!ImGui::BeginDragDropTargetCustom(window->Rect(), window->ID)) {
    return;
  }
  // Pendant le survol, un objet qui n'est pas une carte se dit refusé : c'est le
  // seul moment où l'on peut encore renoncer.
  if (const std::uint32_t carried = CarriedHandCard(); carried != 0) {
    const bool known =
        std::any_of(view.pockets.begin(), view.pockets.end(),
                    [carried](const CardAlbumPocketView& p) { return p.card_id == carried; });
    if (!known && !view.pockets.empty()) {
      SkinTooltipFormat("%s", core::Tr("Seules les cartes entrent dans l'album"));
    }
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ItemDrag))) {
    const auto& carried = *static_cast<const ItemDrag*>(dropped->Data);
    if (carried.from == ItemHolder::kInventory && !carried.worn) {
      OfferFromHand(state, view, carried.client_slot, action);
    }
  }
  ImGui::EndDragDropTarget();
}

}  // namespace

void DrawBook(CardAlbumWindowState& state, const CardAlbumView& view, const SkinArt& art,
              float height, CardAlbumWindowAction& action) {
  ImGui::BeginChild("##classeur", ImVec2(0, height), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 room = ImGui::GetContentRegionAvail();

  // ── La géométrie : la pochette a UNE taille, la fenêtre s'y plie ──────────
  BookLayout layout;
  const float spine = Px(kSpinePixels);
  const float pad = Px(kPagePadPixels);
  const float gap = Px(kPocketGapPixels);
  const float margin = Px(kPocketMarginPixels);
  constexpr float kFooterExtraPixels = 6.0F;
  constexpr float kNameExtraPixels = 3.0F;
  const float footer = ImGui::GetFrameHeight() + Px(kFooterExtraPixels);
  layout.page_width = std::floor((room.x - spine) * 0.5F);
  layout.page_height = room.y;
  // L'icône devant le nom est plus haute qu'une ligne : la bande la loge.
  layout.name_height =
      std::max(ImGui::GetTextLineHeight(), state.look.name_icon ? NameIconSide() : 0.0F) +
      Px(kNameExtraPixels);
  const float inner_width = layout.page_width - 2.0F * pad;
  const float inner_height = layout.page_height - 2.0F * pad - footer;
  ArtSize(&layout.art_width, &layout.art_height);
  layout.cell_width = layout.art_width + 2.0F * margin;
  layout.cell_height = layout.art_height + 2.0F * margin + layout.name_height;
  layout.columns =
      std::max(1, static_cast<int>(std::floor((inner_width + gap) / (layout.cell_width + gap))));
  layout.rows =
      std::max(1, static_cast<int>(std::floor((inner_height + gap) / (layout.cell_height + gap))));
  layout.per_page = layout.columns * layout.rows;
  layout.per_spread = 2 * layout.per_page;
  state.per_spread = layout.per_spread;
  state.wanted_width = static_cast<int>(layout.art_width);
  state.wanted_height = static_cast<int>(layout.art_height);

  // Ce que la fenêtre porte AUTOUR de la grille, pour le calage de l'image
  // suivante : tout ce qui n'est ni pochette ni interstice.
  const ImVec2 window_size = ImGui::GetCurrentWindow()->ParentWindow->Size;
  state.cell_step = ImVec2(layout.cell_width + gap, layout.cell_height + gap);
  state.chrome = ImVec2(window_size.x - room.x + 4.0F * pad + spine,
                        window_size.y - room.y + 2.0F * pad + footer);
  state.snap_ready = true;

  // ── La pagination ─────────────────────────────────────────────────────────
  const int total = static_cast<int>(state.order.size());
  const int spreads = std::max(1, (total + layout.per_spread - 1) / layout.per_spread);
  if (view.highlight_serial != state.highlight_seen) {
    state.highlight_seen = view.highlight_serial;
    for (int rank = 0; rank < total; ++rank) {
      if (view.pockets[static_cast<std::size_t>(state.order[static_cast<std::size_t>(rank)])]
              .card_id == view.highlight_card) {
        const int first = (rank / layout.per_spread) * layout.per_spread;
        if (first != state.first) {
          state.flip_time = ImGui::GetTime();
        }
        state.first = first;
        break;
      }
    }
  }
  // Recalé sur une double page entière : un redimensionnement change le nombre
  // de pochettes par page.
  state.first = std::clamp((state.first / layout.per_spread) * layout.per_spread, 0,
                           (spreads - 1) * layout.per_spread);

  // La molette tourne les pages ; les flèches et Page↑/↓ aussi, quand la
  // fenêtre a le clavier et qu'aucun champ n'est en saisie.
  const ImGuiIO& io = ImGui::GetIO();
  const bool popup = AnyPopupOpen();
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.0F && !popup) {
    Flip(state, io.MouseWheel < 0.0F ? 1 : -1);
  }
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput &&
      !popup) {
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
      Flip(state, -1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
      Flip(state, 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
      Flip(state, -spreads);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) {
      Flip(state, spreads);
    }
  }
  float alpha = 1.0F;
  if (state.flip_time >= 0.0) {
    const double elapsed = ImGui::GetTime() - state.flip_time;
    alpha = elapsed >= kFlipFadeSeconds ? 1.0F : static_cast<float>(elapsed / kFlipFadeSeconds);
    if (elapsed >= kFlipFadeSeconds) {
      state.flip_time = -1.0;
    }
  }

  // ── Les deux pages et la reliure ──────────────────────────────────────────
  DrawPage(state, view, art, into, origin, layout, state.first, true, alpha, action);
  DrawPage(state, view, art, into, ImVec2(origin.x + layout.page_width + spine, origin.y), layout,
           state.first + layout.per_page, false, alpha, action);
  const float spine_left = origin.x + layout.page_width;
  const float spine_middle = spine_left + spine * 0.5F;
  into->AddRectFilledMultiColor(ImVec2(spine_left, origin.y),
                                ImVec2(spine_middle, origin.y + layout.page_height), kClear,
                                kSpineDark, kSpineDark, kClear);
  into->AddRectFilledMultiColor(ImVec2(spine_middle, origin.y),
                                ImVec2(spine_left + spine, origin.y + layout.page_height),
                                kSpineDark, kClear, kClear, kSpineDark);

  // Le classeur est vide : dire pourquoi, au milieu.
  if (view.pockets.empty() || total == 0) {
    const char* why = view.blocked ? core::Tr(
                                         "L'album est déjà ouvert sur un autre de vos comptes "
                                         "de jeu. Fermez-le là-bas d'abord.")
                      : view.pockets.empty()
                          ? (view.loading ? core::Tr("Chargement…")
                                          : core::Tr("Aucune donnée. Cliquez sur Rafraîchir."))
                          : core::Tr("Aucune carte ne correspond.");
    const ImVec2 measured = ImGui::CalcTextSize(why);
    into->AddText(
        ImVec2(origin.x + (room.x - measured.x) * 0.5F, origin.y + (room.y - measured.y) * 0.5F),
        ImGui::ColorConvertFloat4ToU32(kLabelColor), why);
  }
  ImGui::EndChild();
  TakeHandDrop(state, view, action);
}

}  // namespace ro::ui::card_album
