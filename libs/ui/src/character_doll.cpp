#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
// ⚠ POUR LE NOMBRE DE DIRECTIONS D'UN .ACT, et rien d'autre : la molette tourne
// dans la numérotation du fichier, et en redéclarer le compte ici en ferait une
// seconde vérité.
#include "ro/render/actor_layers.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/wheel_gate.h"

#include "character_panes.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Les quatre rangées de cases donnent leur hauteur à l'avatar. ⚠ LA MÊME SUR
// LES DEUX ONGLETS : l'onglet Costume ne montre que deux rangées, mais l'avatar
// ne rétrécit pas — ce sont ses deux rangées qui se centrent contre lui.
constexpr int kDollRows = 4;

// Ce que chaque ligne d'en-tête se remonte, pour les serrer comme le client.
constexpr float kHeaderLineLift = 5.0F;
// Le jour entre le haut du volet et la première rangée.
constexpr float kDollTopGap = 2.0F;

// ── L'AVATAR ────────────────────────────────────────────────────────────────

// Le nom de la zone de l'avatar, pour le portail de la molette : elle est
// dessinée à la main, et n'a donc pas d'item ImGui dont reprendre l'identifiant.
constexpr const char* kAvatarZone = "avatar";
// Un cran de molette en garde saute DEUX directions, les cardinales étant les
// seules que ce rang tienne.
constexpr int kCardinalStep = 2;
// Le jour entre le bas du mannequin et la liste des poses.
constexpr float kPoseComboGap = 8.0F;
// Le jour entre la liste des poses et la case qui la suit.
constexpr float kDollSettingGap = 4.0F;

// La colonne gauche et la colonne droite, de haut en bas.
//
// ⚠ CE SONT DES PLACES, PAS UN ORDRE DE LECTURE. La cape entre les coiffes et
// l'accessoire, les chaussures sous l'armure : c'est la disposition du client,
// et elle ne se déduit d'aucune règle.
constexpr std::array<EquipSlot, kDollRows> kLeftColumn = {
    EquipSlot::kHeadTop, EquipSlot::kHeadLow, EquipSlot::kGarment, EquipSlot::kAccessoryLeft};
constexpr std::array<EquipSlot, kDollRows> kRightColumn = {
    EquipSlot::kHeadMid, EquipSlot::kArmor, EquipSlot::kShoes, EquipSlot::kAccessoryRight};

[[nodiscard]] const EquipSlotView& At(std::span<const EquipSlotView> slots, EquipSlot slot) {
  return slots[static_cast<std::size_t>(slot)];
}

// Une ligne d'en-tête, centrée sur le volet et serrée contre la précédente.
void HeaderLine(const char* text, float pane_width, const SkinLook& look) {
  const float width = ImGui::CalcTextSize(text).x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (pane_width - width) * 0.5F));
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - kHeaderLineLift * Scale());
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(look.text), "%s", text);
}

void DrawHeader(const CharacterHeaderView& header, float pane_width, const SkinLook& look) {
  char line[128] = {};

  HeaderLine(header.name.empty() ? core::Tr("(personnage)") : std::string(header.name).c_str(),
             pane_width, look);

  // ⚠ LE MÉTIER NE S'ÉCRIT QUE S'IL EST CONNU : son nom vient d'une table que
  // le client ne lit pas encore. Écrire un numéro de classe à la place d'un
  // nom serait pire que ne rien écrire.
  if (header.job.empty()) {
    std::snprintf(line, sizeof(line), "%s %lld / %lld", core::Tr("Nv"),
                  static_cast<long long>(header.base_level),
                  static_cast<long long>(header.job_level));
  } else {
    std::snprintf(line, sizeof(line), "%.*s   %s %lld / %lld", static_cast<int>(header.job.size()),
                  header.job.data(), core::Tr("Nv"), static_cast<long long>(header.base_level),
                  static_cast<long long>(header.job_level));
  }
  HeaderLine(line, pane_width, look);

  std::snprintf(line, sizeof(line), "HP: %lld/%lld", static_cast<long long>(header.hp),
                static_cast<long long>(header.max_hp));
  HeaderLine(line, pane_width, look);

  std::snprintf(line, sizeof(line), "SP: %lld/%lld", static_cast<long long>(header.sp),
                static_cast<long long>(header.max_sp));
  HeaderLine(line, pane_width, look);
}

// L'ABRÉVIATION D'UNE CASE VIDE, rétrécie tant qu'elle déborde.
//
// ⚠ ELLE PEUT TENIR SUR DEUX LIGNES — « Head\ntop » — et c'est pour cela qu'on
// mesure avec `CalcTextSize` plutôt qu'en comptant des caractères.
void PaintEmptyLabel(ImDrawList* into, EquipSlot slot, ImVec2 top_left, float side) {
  const char* text = EquipSlotLabel(slot);
  const float room = side - kEquipSlotPadding * 2.0F * Scale();
  float font = ImGui::GetFontSize();
  ImVec2 size = ImGui::CalcTextSize(text);
  while (size.x > room && font > kEquipSlotEmptyMinimumFont * Scale()) {
    font -= 1.0F;
    size.x *= font / (font + 1.0F);
    size.y *= font / (font + 1.0F);
  }
  into->AddText(nullptr, font,
                ImVec2(top_left.x + (side - size.x) * 0.5F, top_left.y + (side - size.y) * 0.5F),
                kEquipSlotEmptyText, text);
}

// LE CÔTÉ DE L'ICÔNE DANS LE FANTÔME D'UN GLISSER. ⚠ LA MÊME QUE CELLE DU SAC :
// un objet traîné doit se reconnaître pareil d'où qu'il parte.
constexpr float kDragGhostIconPixels = 24.0F;

// `inset` est la marge que l'icône garde de chaque côté. ⚠ UNE CASE EN A UNE,
// UN FANTÔME DE GLISSER N'EN A PAS : celui-ci fait déjà la taille de l'icône, et
// lui retrancher la marge d'une case de quarante-quatre pixels la rétrécirait
// d'un quart par rapport à celui du sac.
void PaintIcon(ImDrawList* into, const EquipSlotView& piece, ImVec2 top_left, float side,
               float inset) {
  if (!piece.icon.usable()) {
    return;
  }
  const float room = side - inset * 2.0F;
  float width = piece.icon.width;
  float height = piece.icon.height;
  // ⚠ JAMAIS AGRANDIE : une icône de vingt-quatre pixels étirée sur
  // quarante-quatre devient floue, et le client ne le fait pas non plus.
  if (width > room || height > room) {
    const float shrink = room / std::max(width, height);
    width *= shrink;
    height *= shrink;
  }
  const ImVec2 at(top_left.x + (side - width) * 0.5F, top_left.y + (side - height) * 0.5F);
  into->AddImage(piece.icon.id, at, ImVec2(at.x + width, at.y + height), ImVec2(0, 0), ImVec2(1, 1),
                 piece.damaged ? kItemDamagedTint : IM_COL32_WHITE);
}

// Le raffinement, ou la quantité d'une pile de munitions : en bas à droite,
// cerné de blanc pour rester lisible sur n'importe quelle icône.
void PaintBadge(ImDrawList* into, const EquipSlotView& piece, ImVec2 top_left, float side) {
  char text[16] = {};
  if (piece.refine_level > 0) {
    std::snprintf(text, sizeof(text), "+%d", piece.refine_level);
  } else if (piece.amount > 1) {
    std::snprintf(text, sizeof(text), "%d", piece.amount);
  } else {
    return;
  }
  const ImVec2 size = ImGui::CalcTextSize(text);
  const ImVec2 at(top_left.x + side - size.x - kEquipSlotPadding * Scale(),
                  top_left.y + side - size.y - Scale());
  AddTextHalo(into, at, IM_COL32_BLACK, text, IM_COL32_WHITE);
}

// UNE CASE. Elle rend vrai quand le joueur vient de demander à la vider.
[[nodiscard]] bool DrawSlot(EquipSlot slot, const EquipSlotView& piece, ImVec2 at, float side,
                            bool preview, const SkinLook& look, CharacterWindowAction& action) {
  ImGui::PushID(static_cast<int>(slot));
  ImGui::SetCursorScreenPos(at);
  ImGui::InvisibleButton("case", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 corner(at.x + side, at.y + side);
  const float rounding = kEquipSlotRounding * Scale();
  into->AddRectFilled(at, corner, look.slot, rounding);
  into->AddRect(at, corner, kEquipSlotBorder, rounding);

  // LE LÂCHER, SUR PLEINE COMME SUR VIDE. Une case occupée reste une cible :
  // le serveur remplace ce qui s'y trouve, exactement comme le sac le
  // demanderait par un double-clic.
  //
  // ⚠ SAUF CE QUI EST DÉJÀ SUR LE CORPS. La case visée ne décide de rien — le
  // serveur sait où la pièce va —, donc traîner un équipement d'une case à
  // l'autre ne demande rien de neuf, et l'enfiler une seconde fois enverrait un
  // paquet qui ne change rien.
  bool asked = false;
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
        dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ItemDrag))) {
      if (const auto& carried = *static_cast<const ItemDrag*>(dropped->Data); !carried.worn) {
        action.verb = CharacterVerb::kWear;
        action.slot = carried.client_slot;
        action.alternate = ImGui::GetIO().KeyCtrl;
        asked = true;
      }
    }
    ImGui::EndDragDropTarget();
  }

  // ET LA CASE SE TIRE : vers le sac pour ôter la pièce, vers la barre de
  // raccourcis pour s'en faire une touche.
  //
  // ⚠ AVANT L'INFOBULLE, et il faut que ce soit avant : une infobulle est une
  // fenêtre, et ImGui rattache la source au DERNIER objet posé.
  if (piece.filled() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    const ItemDrag carried{.item_id = piece.item_id,
                           .client_slot = piece.client_slot,
                           .from = ItemHolder::kEquipment,
                           .worn = true};
    ImGui::SetDragDropPayload(kItemDragPayload, &carried, sizeof(carried));
    // L'ICÔNE PUIS LE NOM, le même fantôme que celui du sac : c'est l'objet qu'on
    // TIENT qu'il faut reconnaître, et un nom seul ne se reconnaît pas d'un coup
    // d'œil — surtout en traversant une grille de cent cases.
    if (piece.icon.usable()) {
      const ImVec2 ghost = ImGui::GetCursorScreenPos();
      const float icon_side = kDragGhostIconPixels * Scale();
      ImGui::Dummy(ImVec2(icon_side, icon_side));
      PaintIcon(ImGui::GetWindowDrawList(), piece, ghost, icon_side, 0.0F);
      ImGui::SameLine();
    }
    ImGui::TextUnformatted(piece.name.data(), piece.name.data() + piece.name.size());
    ImGui::EndDragDropSource();
  }

  if (!piece.filled()) {
    PaintEmptyLabel(into, slot, at, side);
  } else {
    PaintIcon(into, piece, at, side, kEquipSlotPadding * Scale());
    PaintBadge(into, piece, at, side);
    if (hovered) {
      into->AddRectFilled(at, corner, kItemHoverVeil, rounding);
      // ⚠ LA MAIN, comme sur toute case du jeu : c'est ce qui dit qu'il y a un
      // geste ici.
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      // ⚠ RIEN TANT QUE LE BOUTON EST TENU, et c'est tout ce que cette garde
      // ajoute : `ItemHoverable` d'ImGui écarte déjà la case SOURCE d'un glisser
      // commencé (`DragDropPayload.SourceId == id`) et toutes les autres
      // (`ActiveId != id`). Ce qu'il ne couvre pas, c'est l'appui AVANT le seuil
      // de glisser — la case est alors l'élément actif, donc encore survolée, et
      // sa bulle s'ouvrirait juste avant de partir.
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        action.hovered_id = piece.item_id;
        action.hovered_slot = piece.client_slot;
        // ⚠ L'APERÇU REMPLACE CETTE INFOBULLE-CI quand il est demandé, il ne s'y
        // ajoute pas : deux bulles sur la même case se recouvriraient.
        if (!preview) {
          if (BeginSkinTooltip()) {
            ImGui::TextUnformatted(std::string(piece.name).c_str());
            EndSkinTooltip();
          }
        }
      }
    }
    if (hovered) {
      // MAJ + CLIC GAUCHE : le lien de la pièce dans la saisie du chat. Le même
      // geste que le sac, et celui du client.
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
        action.link_slot = piece.client_slot;
        asked = true;
      }
      // CLIC DROIT : la description.
      //
      // 🔴 AU RELÂCHEMENT : ouverte dès le clic, la description passerait
      // DERRIÈRE cette fenêtre sur un appui PROLONGÉ.
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        action.describe_id = piece.item_id;
        action.describe_slot = piece.client_slot;
        asked = true;
      }
      // LE GESTE DU CLIENT : on retire ce qu'on porte d'un double-clic.
      //
      // ⚠ PAS AVEC MAJ TENUE : sous cette touche le double-clic est un envoi de
      // lien, et déséquiper au passage serait une pièce ôtée par surprise.
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyShift) {
        action.verb = CharacterVerb::kTakeOff;
        action.slot = piece.client_slot;
        asked = true;
      }
    }
  }
  ImGui::PopID();
  return asked;
}

// LA PLACE DE L'AVATAR, tenue et vide.
//
// 🔴 UN TROU QUI SE VOIT NE MENT PAS. Composer le personnage hors de la carte
// est son propre chantier — rien ici ne sait le faire aujourd'hui —, et remplir
// la place d'un décor quelconque ferait croire que c'est fini.
// UNE CASE DE COMPAGNON : le chariot, la monture, le faucon.
//
// 🔴 CE N'EST PAS UNE CASE DU MANNEQUIN, et c'est pour cela qu'elle a son propre
// dessin. Une case d'équipement montre ce qu'on PORTE et se remplit en y
// lâchant une pièce ; celle-ci est un INTERRUPTEUR — elle appelle et renvoie —,
// et rien ne s'y lâche. Leur donner la même apparence inviterait à y traîner un
// objet, qui ne partirait nulle part.
//
// Elle se tient donc comme un onglet ouvert quand le compagnon est là, et se
// grise quand il ne l'est pas.
[[nodiscard]] bool DrawCompanionCase(const CompanionCaseView& companion, ImVec2 at, float side,
                                     const SkinLook& look) {
  ImGui::PushID(static_cast<int>(companion.rank) + 0x100);
  ImGui::SetCursorScreenPos(at);
  ImGui::InvisibleButton("compagnon", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked();

  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 corner(at.x + side, at.y + side);
  const float rounding = kEquipSlotRounding * Scale();
  into->AddRectFilled(at, corner, companion.active ? look.active_tab : look.slot, rounding);
  into->AddRect(at, corner, kEquipSlotBorder, rounding);

  if (companion.icon.usable()) {
    const float inset = kEquipSlotPadding * Scale();
    const float room = side - inset * 2.0F;
    float width = companion.icon.width;
    float height = companion.icon.height;
    if (width > room || height > room) {
      const float shrink = room / std::max(width, height);
      width *= shrink;
      height *= shrink;
    }
    const ImVec2 icon_at(at.x + (side - width) * 0.5F, at.y + (side - height) * 0.5F);
    // ⚠ GRISÉE QUAND IL N'EST PAS LÀ, et non masquée : c'est l'icône qui dit
    // DE QUI il s'agit, et une case vide ne le dirait plus.
    into->AddImage(companion.icon.id, icon_at, ImVec2(icon_at.x + width, icon_at.y + height),
                   ImVec2(0, 0), ImVec2(1, 1),
                   companion.active ? IM_COL32_WHITE : kItemDamagedTint);
  }
  if (hovered) {
    into->AddRectFilled(at, corner, kItemHoverVeil, rounding);
    if (!companion.name.empty()) {
      SkinTooltip(std::string(companion.name).c_str());
    }
  }
  ImGui::PopID();
  return clicked;
}

// LA PLACE DE L'AVATAR : la silhouette, ou le mot qui dit qu'il n'y en a pas.
//
// ⚠ ELLE MESURE SA BOÎTE POUR `app`, qui composera à cette taille-là à l'image
// suivante. La mesure se fait ICI parce que l'échelle d'interface n'est connue
// qu'au dessin ; une image de retard sur un redimensionnement ne se voit pas.
//
// 🔴 ET LA MOLETTE TOURNE LE PERSONNAGE, en passant par le portail : sans lui,
// parcourir la fiche le ferait pivoter au passage. Huit directions, sauf en
// garde où le client n'en tient que quatre — les cardinales.
void DrawAvatarPlace(ImDrawList* into, ImVec2 at, float width, float height,
                     CharacterAvatarState& avatar, const SkinImage& image, const SkinLook& look) {
  const ImVec2 corner(at.x + width, at.y + height);
  into->AddRectFilled(at, corner, look.doll, kEquipSlotRounding * Scale());

  const float padding = kCharacterAvatarPadding * Scale();
  avatar.box_width = std::max(0.0F, width - 2.0F * padding);
  avatar.box_height = std::max(0.0F, height - 2.0F * padding);

  if (image.usable()) {
    // ⚠ LA TAILLE POSÉE EST CELLE DE LA BOÎTE, et la texture est plus petite :
    // elle est composée en pixels de SPRITE, et c'est ce quad-ci qui
    // l'agrandit — en bilinéaire, puisque la carte échantillonne. C'est le
    // seul endroit du mannequin où une image s'étire, et c'est voulu : au plus
    // proche, la silhouette se découperait en marches d'escalier.
    const ImVec2 image_at(at.x + (width - image.width) * 0.5F,
                          at.y + (height - image.height) * 0.5F);
    into->AddImage(image.id, image_at, ImVec2(image_at.x + image.width, image_at.y + image.height));
  } else {
    const char* text = core::Tr("Silhouette à venir");
    const ImVec2 size = ImGui::CalcTextSize(text);
    if (size.x > width) {
      return;
    }
    into->AddText(ImVec2(at.x + (width - size.x) * 0.5F, at.y + (height - size.y) * 0.5F),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
  }

  const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(at, corner);
  if (!hovered) {
    return;
  }
  ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  const float wheel = WheelOver(avatar.wheel, ImGui::GetID(kAvatarZone), hovered);
  if (wheel == 0.0F) {
    return;
  }
  const bool cardinal = CharacterPoseIsCardinal(avatar.pose);
  const int step = cardinal ? kCardinalStep : 1;
  const int from = cardinal ? (avatar.direction & ~1) : avatar.direction;
  const auto turns = static_cast<int>(render::kDirectionCount);
  avatar.direction =
      static_cast<std::uint8_t>((from + (wheel > 0.0F ? step : turns - step)) % turns);
}

// LA CASE D'UN RÉGLAGE, sous la liste des poses et centrée comme elle. Rend
// l'état que le clic demande, rien sans clic.
//
// 🔴 LA CASE SE DESSINE SUR L'ÉTAT QUE LE SERVEUR A DIT, jamais sur le sien :
// le clic ne fait que demander l'autre, et elle bascule à l'écho. Un réglage
// que le serveur tient ne doit pas paraître changé avant qu'il l'ait changé.
//
// ⚠ LE LIBELLÉ ET L'INFOBULLE ARRIVENT TRADUITS : c'est à l'appelant de les
// nommer, pour que chacun reste un texte à traduire.
[[nodiscard]] std::optional<bool> DrawDollSetting(const char* label, const char* tip, bool ticked,
                                                  float pane_width, const SkinArt& art) {
  // Le jour sous la liste des poses se compte depuis son BORD, pas depuis
  // l'espacement qu'ImGui laisse après elle.
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y +
                       kDollSettingGap * Scale());
  const float width = SkinCheckBoxWidth(label, art);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (pane_width - width) * 0.5F));
  std::optional<bool> asked;
  if (SkinCheckBox(label, &ticked, art)) {
    asked = ticked;
  }
  SkinTooltip(tip);
  return asked;
}

// Une entrée de la liste des poses : une pose, figée ou animée.
struct PoseEntry {
  CharacterPose pose = CharacterPose::kIdle;
  bool animated = false;
};

void TakePose(CharacterAvatarState& avatar, const PoseEntry& entry) {
  avatar.pose = entry.pose;
  avatar.animated = entry.animated;
  // ⚠ RECALÉ SUR UN CRAN PAIR : une garde prise depuis une direction impaire
  // n'en montrerait aucune, et la molette sauterait ensuite de deux en deux sans
  // jamais y revenir.
  if (CharacterPoseIsCardinal(entry.pose)) {
    avatar.direction = static_cast<std::uint8_t>(avatar.direction & ~1);
  }
}

}  // namespace

float DollSettingWidth(const SkinArt& art) {
  return std::max(SkinCheckBoxWidth(core::Tr("Montrer mon équipement"), art),
                  SkinCheckBoxWidth(core::Tr("Voir les costumes"), art));
}

// LA LISTE DES POSES, sous le mannequin et centrée sur lui.
//
// ⚠ SIX ENTRÉES POUR QUATRE POSES : la marche et la garde y paraissent deux
// fois, figées puis animées. Ce sont deux choses différentes à l'œil — une pose
// tenue se regarde, une animation se joue — et les séparer en une case à cocher
// obligerait à la griser pour deux poses sur quatre.
void DrawPoseList(CharacterAvatarState& avatar, float pane_width, const SkinArt& art) {
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kPoseComboGap * Scale());
  const char* shown = CharacterPoseLabel(avatar.pose, avatar.animated);

  // Les entrées dans l'ordre de la liste : la molette les parcourt comme elle.
  std::array<PoseEntry, kCharacterPoseCount * 2> entries{};
  int count = 0;
  int current = -1;
  float widest = 0.0F;
  for (std::size_t rank = 0; rank < kCharacterPoseCount; ++rank) {
    const auto pose = static_cast<CharacterPose>(rank);
    for (const bool animated : {false, true}) {
      if (animated && !CharacterPoseAnimates(pose)) {
        continue;
      }
      if (avatar.pose == pose && avatar.animated == animated) {
        current = count;
      }
      entries[static_cast<std::size_t>(count++)] = PoseEntry{pose, animated};
      widest = std::max(widest, ImGui::CalcTextSize(CharacterPoseLabel(pose, animated)).x);
    }
  }
  const float width = std::min(pane_width, widest + SkinComboChromeWidth(art));
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, (pane_width - width) * 0.5F));
  ImGui::SetNextItemWidth(width);

  int step = 0;
  const bool open = BeginSkinCombo("##pose", shown, art, &step);
  if (const int stepped = SkinComboStepped(current, step, count); stepped != current) {
    TakePose(avatar, entries[static_cast<std::size_t>(stepped)]);
  }
  if (!open) {
    return;
  }
  for (int rank = 0; rank < count; ++rank) {
    const PoseEntry& entry = entries[static_cast<std::size_t>(rank)];
    if (ImGui::Selectable(CharacterPoseLabel(entry.pose, entry.animated), rank == current)) {
      TakePose(avatar, entry);
    }
  }
  EndSkinCombo();
}

void DrawCharacterDoll(const CharacterHeaderView& header, std::span<const EquipSlotView> slots,
                       std::span<const CompanionCaseView> companions, CharacterAvatarState& avatar,
                       const SkinImage& avatar_image, const std::optional<bool>& equipment_shown,
                       const std::optional<bool>& costumes_shown, bool costume, bool preview,
                       float pane_width, const SkinArt& art, CharacterWindowAction& action) {
  DrawHeader(header, pane_width, art.look());

  const float scale = Scale();
  const float side = kEquipSlotPixels * scale;
  const float gap = kEquipSlotGapPixels * scale;
  const float avatar_width = kCharacterAvatarPixels * scale;
  const float block = kCharacterDollBlockPixels * scale;
  const float step = side + gap;
  const float avatar_height = static_cast<float>(kDollRows) * step - gap;

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  // LE BLOC EST CENTRÉ, et non collé à gauche : c'est ce qui le garde sous
  // l'en-tête quand la fenêtre s'élargit.
  const float left = origin.x + std::max(0.0F, (pane_width - block) * 0.5F);
  const float middle = left + step;
  const float right = middle + avatar_width + gap;
  const float top = origin.y + kDollTopGap * scale;

  ImDrawList* into = ImGui::GetWindowDrawList();
  DrawAvatarPlace(into, ImVec2(middle, top), avatar_width, avatar_height, avatar, avatar_image,
                  art.look());

  float used_height = avatar_height + kDollTopGap * scale;
  if (costume) {
    // DEUX RANGÉES, CENTRÉES CONTRE L'AVATAR. ⚠ ET AUCUNE CASE VIDE DE PLUS :
    // les six autres places n'existent pas pour un costume, et en dessiner les
    // cadres laisserait croire qu'on peut y poser quelque chose.
    const auto places = CostumeSlots();
    const float pairs = 2.0F;
    const float offset = (avatar_height - (pairs * step - gap)) * 0.5F;
    for (int row = 0; row < 2; ++row) {
      const float y = top + offset + static_cast<float>(row) * step;
      const EquipSlot on_left = places[static_cast<std::size_t>(row)];
      const EquipSlot on_right = places[static_cast<std::size_t>(row) + 2];
      static_cast<void>(DrawSlot(on_left, At(slots, on_left), ImVec2(left, y), side, preview,
                                 art.look(), action));
      static_cast<void>(DrawSlot(on_right, At(slots, on_right), ImVec2(right, y), side, preview,
                                 art.look(), action));
    }
  } else {
    for (int row = 0; row < kDollRows; ++row) {
      const float y = top + static_cast<float>(row) * step;
      const EquipSlot on_left = kLeftColumn[static_cast<std::size_t>(row)];
      const EquipSlot on_right = kRightColumn[static_cast<std::size_t>(row)];
      static_cast<void>(DrawSlot(on_left, At(slots, on_left), ImVec2(left, y), side, preview,
                                 art.look(), action));
      static_cast<void>(DrawSlot(on_right, At(slots, on_right), ImVec2(right, y), side, preview,
                                 art.look(), action));
    }
    // LA RANGÉE DES MAINS, sous l'avatar : l'arme à gauche de lui, le bouclier
    // à droite, et la munition sous la colonne de droite.
    const float hands_y = top + static_cast<float>(kDollRows) * step;
    static_cast<void>(DrawSlot(EquipSlot::kWeapon, At(slots, EquipSlot::kWeapon),
                               ImVec2(middle, hands_y), side, preview, art.look(), action));
    static_cast<void>(DrawSlot(EquipSlot::kShield, At(slots, EquipSlot::kShield),
                               ImVec2(middle + avatar_width - side, hands_y), side, preview,
                               art.look(), action));
    static_cast<void>(DrawSlot(EquipSlot::kAmmo, At(slots, EquipSlot::kAmmo),
                               ImVec2(right, hands_y), side, preview, art.look(), action));
    used_height += step;

    // LES COMPAGNONS, EMPILÉS SOUS LA COLONNE DE GAUCHE et non mêlés au
    // mannequin : ce sont des interrupteurs, pas des places où l'on porte
    // quelque chose. La colonne de gauche est libre sous la rangée des mains,
    // et c'est là que le client les met.
    //
    // ⚠ ELLE NE PREND DE LA HAUTEUR QUE POUR CE QU'ELLE MONTRE : la plupart des
    // métiers n'ont appris aucune des trois compétences, et leur fiche ne doit
    // pas garder trois cases de vide sous le mannequin.
    for (std::size_t rank = 0; rank < companions.size(); ++rank) {
      const float y = hands_y + static_cast<float>(rank) * step;
      if (DrawCompanionCase(companions[rank], ImVec2(left, y), side, art.look())) {
        action.toggled_companion = static_cast<int>(companions[rank].rank);
      }
    }
    if (!companions.empty()) {
      used_height += static_cast<float>(companions.size() - 1) * step;
    }
  }

  // ⚠ RÉSERVER LA PLACE CONSOMMÉE : tout ce qui précède est peint en
  // coordonnées absolues, et sans cela l'interface ne lui connaîtrait aucune
  // étendue — le volet croirait être vide.
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(pane_width, used_height));

  // LA LISTE DES POSES vient APRÈS la place réservée, et non par-dessus : c'est
  // le seul morceau du volet qui se pose au curseur, et il lui faut le bas du
  // mannequin pour savoir où tomber.
  DrawPoseList(avatar, pane_width, art);
  // UNE CASE PAR ONGLET, AU MÊME JOUR SOUS LES POSES : chacun dit le réglage de
  // ce qu'il montre. ⚠ SANS RÉPONSE DU SERVEUR, chacune montre le réglage d'un personnage
  // neuf — équipement caché, costumes affichés.
  if (costume) {
    action.show_costumes =
        DrawDollSetting(core::Tr("Voir les costumes"),
                        core::Tr("Affiche ou masque les costumes sur ton personnage"),
                        costumes_shown.value_or(true), pane_width, art);
  } else {
    action.show_equipment =
        DrawDollSetting(core::Tr("Montrer mon équipement"),
                        core::Tr("Rend ton équipement visible (ou non) aux autres joueurs"),
                        equipment_shown.value_or(false), pane_width, art);
  }
}

}  // namespace ro::ui
