#include "shortcut_slot_paint.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>

#include "ro/core/decimal_text.h"
#include "ro/ui/skin_text.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr ImU32 kHoverVeil = IM_COL32(255, 255, 255, 36);
// La case qu'un glisser survole : elle s'annonce, sinon on lâche à l'aveugle.
constexpr ImU32 kDropOutline = IM_COL32(255, 205, 105, 230);
constexpr float kDropThickness = 2.0F;
// La case éclairée par le mode clavier du grimoire : un voile et un cadre
// bleus, pour ne pas se confondre avec la cible d'un glisser.
constexpr ImU32 kHighlightVeil = IM_COL32(110, 180, 255, 70);
constexpr ImU32 kHighlightOutline = IM_COL32(120, 190, 255, 240);
// L'icône d'une compétence que le serveur ne donne plus.
constexpr ImU32 kUnknownTint = IM_COL32(105, 105, 115, 150);
// Le voile du temps d'attente : il MONTE du bas, comme au client.
constexpr ImU32 kCooldownVeil = IM_COL32(10, 10, 16, 165);
constexpr float kLabelMargin = 2.0F;
// En deçà, un libellé rétréci n'est plus qu'une tache : mieux vaut qu'il déborde
// un peu que de le réduire jusqu'à l'illisible.
constexpr float kSmallestLabelSize = 7.0F;
// La part de la case que prend le décompte.
constexpr float kCountdownShare = 0.42F;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;

// UN TEXTE DE CASE, avec l'ombre que la barre a choisie.
//
// ⚠ UN SEUL POINT DE PASSAGE pour les quatre textes d'une case — touche, rang,
// quantité, décompte. Le choix d'ombre répété sur chacun serait quatre endroits
// où l'un d'eux resterait en arrière au prochain réglage ajouté.
void PaintSlotText(ImDrawList* into, const ShortcutBarLayout& look, float size, ImVec2 at,
                   ImU32 color, const char* text) {
  const float scale = Scale();
  switch (static_cast<SlotTextShadow>(look.shadow_kind)) {
    case SlotTextShadow::kNone:
      if (size > 0.0F) {
        into->AddText(ImGui::GetFont(), size, at, color, text);
      } else {
        into->AddText(at, color, text);
      }
      break;
    case SlotTextShadow::kHalo:
      AddTextHalo(into, size, at, color, text, look.shadow_color);
      break;
    case SlotTextShadow::kDrop: {
      // ⚠ LE DÉCALAGE SUIT L'ÉCHELLE DE L'INTERFACE, contrairement au cerne : une
      // ombre est une DISTANCE que le joueur a choisie, là où un cerne n'est
      // qu'un contour qui doit rester d'un pixel pour ne pas fermer les lettres.
      const ImVec2 away(at.x + look.shadow_offset * scale, at.y + look.shadow_offset * scale);
      if (size > 0.0F) {
        into->AddText(ImGui::GetFont(), size, away, look.shadow_color, text);
        into->AddText(ImGui::GetFont(), size, at, color, text);
      } else {
        into->AddText(away, look.shadow_color, text);
        into->AddText(at, color, text);
      }
      break;
    }
  }
}

}  // namespace

void PaintShortcutSlot(ImDrawList* into, const ShortcutBarLayout& look,
                       const ShortcutSlotView& slot, ImVec2 top_left, float side,
                       SlotPaintMarks marks) {
  const float scale = Scale();
  const float rounding = kShortcutSlotRounding * scale;
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  // LE FOND DIT CE QUE LA CASE PORTE. ⚠ TROIS COULEURS ET NON UNE : huit barres
  // servent à séparer les familles, et c'est la teinte qui rend la séparation
  // lisible du coin de l'œil. Les deux fonds de contenu valent celui du vide par
  // défaut — une icône les recouvre, ils n'existent que pour qui veut teinter.
  const ImU32 background = slot.item_id != 0    ? look.item_color
                           : slot.skill_id != 0 ? look.skill_color
                                                : look.empty_color;
  into->AddRectFilled(top_left, bottom_right, background, rounding);

  if (slot.icon.usable()) {
    into->AddImage(slot.icon.id, top_left, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                   slot.known ? IM_COL32_WHITE : kUnknownTint);
  }
  // ⚠ LE VOILE MONTE DU BAS. Une case qui s'assombrit d'un coup ne dit pas
  // combien de temps il reste ; celle-ci se vide comme un sablier, et c'est ce
  // qu'on lit du coin de l'œil en combattant.
  if (slot.cooldown > 0.0F) {
    const float top = bottom_right.y - side * slot.cooldown;
    into->AddRectFilled(ImVec2(top_left.x, top), bottom_right, kCooldownVeil);
  }
  if (marks.hot) {
    into->AddRectFilled(top_left, bottom_right, kHoverVeil, rounding);
  }
  if (slot.highlighted) {
    into->AddRectFilled(top_left, bottom_right, kHighlightVeil, rounding);
  }
  // ⚠ ÉPAISSEUR ZÉRO = PAS DE CADRE, et c'est une valeur que des joueurs
  // choisiront : des cases jointives sans bordure est une disposition qui se
  // défend. La cible d'un glisser garde la sienne — elle doit s'annoncer même
  // sur une barre sans cadre, sinon on lâche à l'aveugle. La case éclairée
  // aussi, pour la même raison.
  const bool marked = marks.dropping || slot.highlighted;
  const float border = marked ? kDropThickness * scale : look.border_thickness * scale;
  if (border > 0.0F) {
    const ImU32 edge = marks.dropping     ? kDropOutline
                       : slot.highlighted ? kHighlightOutline
                                          : look.border_color;
    into->AddRect(top_left, bottom_right, edge, rounding, 0, border);
  }

  // COMBIEN IL EN RESTE, en bas à droite — la place où la grille du sac écrit
  // déjà ses quantités. Rien pour une compétence, ni pour un exemplaire unique.
  const float margin = kLabelMargin * scale;
  ImFont* const font = ImGui::GetFont();
  const float count_size =
      std::max(ImGui::GetFontSize() * look.count_scale, kSmallestLabelSize * scale);
  if (slot.item_id != 0 && slot.amount > 1) {
    std::array<char, ro::core::kDecimalTextBytes<int>> count{};
    std::snprintf(count.data(), count.size(), "%d", slot.amount);
    const ImVec2 size = font->CalcTextSizeA(count_size, FLT_MAX, 0.0F, count.data());
    PaintSlotText(into, look, count_size,
                  ImVec2(bottom_right.x - margin - size.x, bottom_right.y - margin - size.y),
                  look.count_color, count.data());
  }
  if (marks.show_key && !slot.key.empty()) {
    const std::array<char, 32> text = [&slot] {
      std::array<char, 32> copy{};
      std::snprintf(copy.data(), copy.size(), "%.*s", static_cast<int>(slot.key.size()),
                    slot.key.data());
      return copy;
    }();
    // 🔴 L'ÉTIQUETTE RÉTRÉCIT PLUTÔT QUE DE DÉBORDER : sept barres sur huit
    // n'ont de touches qu'avec un modificateur, donc « Ctrl+F1 » et
    // « Ctrl+Maj+F1 » sont la règle, pas l'exception. Tronquer donnerait
    // « Ctrl+ », qui désigne tout et rien.
    // ⚠ LE RÉTRÉCISSEMENT S'APPLIQUE APRÈS le réglage du joueur : c'est un
    // FILET, pas une taille. Qui choisit deux fois la taille d'étiquette sur une
    // petite case obtient ce qui tient, et non « Ctrl+ ».
    float size = std::max(ImGui::GetFontSize() * look.key_scale, kSmallestLabelSize * scale);
    const float room = side - margin * 2.0F;
    const float wide = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text.data()).x;
    if (wide > room && wide > 0.0F) {
      size = std::max(size * room / wide, kSmallestLabelSize * scale);
    }
    PaintSlotText(into, look, size, ImVec2(top_left.x + margin, top_left.y + margin),
                  look.key_color, text.data());
  }
  if (slot.level > 0) {
    std::array<char, ro::core::kDecimalTextBytes<int>> rank{};
    std::snprintf(rank.data(), rank.size(), "%d", slot.level);
    const ImVec2 size = font->CalcTextSizeA(count_size, FLT_MAX, 0.0F, rank.data());
    PaintSlotText(into, look, count_size,
                  ImVec2(bottom_right.x - size.x - margin, bottom_right.y - size.y - margin),
                  look.count_color, rank.data());
  }
  // LE DÉCOMPTE, PAR-DESSUS LE VOILE. En deçà de la minute, la seconde suffit ;
  // au-delà, on écrit m:ss — « 143 » ne se lit pas comme « 2:23 ».
  if (slot.cooldown_ms > 0) {
    std::array<char, 16> left{};
    if (slot.cooldown_ms >= kSecondsPerMinute * kMillisecondsPerSecond) {
      std::snprintf(left.data(), left.size(), "%u:%02u",
                    slot.cooldown_ms / (kSecondsPerMinute * kMillisecondsPerSecond),
                    (slot.cooldown_ms / kMillisecondsPerSecond) % kSecondsPerMinute);
    } else {
      // ⚠ ARRONDI AU-DESSUS : une attente qui affiche « 0 » pendant une seconde
      // entière se lit « c'est prêt », et le clic part pour rien.
      std::snprintf(left.data(), left.size(), "%u",
                    (slot.cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
    }
    // La taille suit la CASE et non la police de l'interface : un décompte
    // gravé à douze points disparaît sur une case de soixante-quatre. Le réglage
    // du joueur la multiplie, comme pour les autres textes.
    const float size =
        std::max(side * kCountdownShare * look.count_scale, kSmallestLabelSize * scale);
    const ImVec2 room = font->CalcTextSizeA(size, FLT_MAX, 0.0F, left.data());
    PaintSlotText(into, look, size,
                  ImVec2(top_left.x + (side - room.x) * 0.5F, top_left.y + (side - room.y) * 0.5F),
                  look.countdown_color, left.data());
  }
}

}  // namespace ro::ui
