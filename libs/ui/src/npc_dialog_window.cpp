#include "ro/ui/npc_dialog_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// Ce que la fenêtre mesure au premier affichage. Le joueur la règle ensuite, et
// sa mesure est retenue d'une partie à l'autre.
constexpr float kFirstWidthPixels = 460.0F;
constexpr float kFirstHeightPixels = 300.0F;
// Ce qu'elle garde au moins : les boutons du pied côte à côte, et sous le titre
// quelques lignes de texte au-dessus d'eux.
constexpr float kLeastWidthPixels = 300.0F;
constexpr float kLeastHeightPixels = 180.0F;
// Où elle naît, tant que le joueur ne l'a pas déplacée.
constexpr float kFirstLeftPixels = 280.0F;
constexpr float kFirstTopPixels = 360.0F;

// LA COULEUR D'UN LIEN : celle des liens de l'habillage, imposée quel que soit
// le `^RRGGBB` du contexte. Dans une page de dialogue, ce qui est cliquable doit
// se voir avant d'être survolé.
[[nodiscard]] ImU32 LinkColor() { return ImGui::GetColorU32(ImGuiCol_TextLink); }

// La colonne du portrait de page, et sa hauteur maximale.
constexpr float kPortraitWidthPixels = 104.0F;
constexpr float kPortraitMaxHeightPixels = 200.0F;

// Ce qu'on garde de texte visible quand un long menu voudrait toute la place.
constexpr float kLeastTextPixels = 48.0F;

// Ce qu'une option de menu ajoute à la hauteur des glyphes. Volontairement plus
// serré qu'une ligne ordinaire : un menu est une liste, pas un formulaire.
constexpr float kMenuRowOverGlyphPixels = 3.0F;

// La marge sous les boutons du pied.
constexpr float kFooterBottomPixels = 8.0F;

// Ce qu'un média peut dépasser de la bande visible sans être coupé à tort : les
// lignes ne sont plus strictement ordonnées en hauteur depuis qu'elles se
// centrent, et une image haute déborde de la sienne.
constexpr float kClipSlackPixels = 260.0F;

[[nodiscard]] float MenuRowHeight() { return ImGui::GetFontSize() + kMenuRowOverGlyphPixels; }

// La mesure d'un morceau de texte avec la police courante.
[[nodiscard]] float MeasureHere(std::string_view piece, void* /*owner*/) {
  return ImGui::CalcTextSize(piece.data(), piece.data() + piece.size()).x;
}

// Les options que le filtre courant laisse passer. `lowered` est le libellé
// déjà en minuscules : voir `ReadMenu`.
[[nodiscard]] bool Matches(std::string_view lowered, std::string_view filter) {
  return filter.empty() || lowered.find(filter) != std::string_view::npos;
}

[[nodiscard]] std::size_t VisibleOptions(std::span<const std::string> lowered,
                                         std::string_view filter) {
  std::size_t shown = 0;
  for (const std::string& option : lowered) {
    shown += Matches(option, filter) ? 1U : 0U;
  }
  return shown;
}

// LIT LE MENU, une fois par liste. ⚠ LE NOMBRE D'OPTIONS ENTRE DANS LA CLÉ :
// une vue qui change de liste sans en changer le numéro ne doit rien laisser de
// périmé.
void ReadMenu(NpcDialogWindowState& state, const NpcDialogView& view,
              const DialogVocabulary& words) {
  if (state.menu_read && state.menu_read_page == view.page && state.menu_read_list == view.menu &&
      state.menu_runs.size() == view.options.size()) {
    return;
  }
  state.menu_runs.clear();
  state.menu_lowered.clear();
  state.menu_runs.reserve(view.options.size());
  state.menu_lowered.reserve(view.options.size());
  for (const std::string_view option : view.options) {
    state.menu_runs.push_back(ParseDialogLine(option, words));
    state.menu_lowered.push_back(core::AsciiLower(option));
  }
  state.menu_read_page = view.page;
  state.menu_read_list = view.menu;
  state.menu_read = true;
}

[[nodiscard]] bool MenuIsSearchable(const NpcDialogView& view) {
  return view.menu_search && view.options.size() > kNpcMenuSearchThreshold;
}

// LA HAUTEUR QUE LE MENU VOUDRAIT, calculée AVANT de rien dessiner.
//
// 🔴 C'EST CE QUI PERMET DE DIMENSIONNER LE TEXTE : le menu se mesure d'abord,
// le texte prend le reste. Reprise de l'image précédente, cette hauteur
// rattrapait sa valeur avec un temps de retard et le texte sautait d'autant à
// chaque frappe dans le filtre.
[[nodiscard]] float MenuWantedHeight(const NpcDialogView& view,
                                     std::span<const std::string> lowered,
                                     std::string_view filter) {
  if (view.options.empty()) {
    return 0.0F;
  }
  std::size_t rows = VisibleOptions(lowered, filter);
  // « Aucun résultat » garde une ligne de haut : la boîte ne doit pas s'effondrer
  // sous la frappe.
  rows = std::clamp<std::size_t>(rows, 1, kNpcMenuMaxRows);
  const ImGuiStyle& style = ImGui::GetStyle();
  // La liste est bordée : son écart compte deux fois. Le pixel de rabiot absorbe
  // l'arrondi du cadre — l'oublier rogne la dernière option d'un cheveu, ce qui
  // fait apparaître un ascenseur sur un menu qui tenait.
  float height = static_cast<float>(rows) * MenuRowHeight() + style.WindowPadding.y * 2.0F + 2.0F;
  if (MenuIsSearchable(view)) {
    height += ImGui::GetFrameHeight() + style.ItemSpacing.y;
  }
  return height;
}

// ── LE CORPS ────────────────────────────────────────────────────────────────

// (Re)place la page si sa mise en page ne vaut plus.
void RelayOut(NpcDialogWindowState& state, const NpcDialogView& view, const NpcDialogArt& media,
              float wrap) {
  const float font_size = ImGui::GetFontSize();
  DialogMetrics metrics;
  metrics.wrap_width = wrap;
  metrics.line_height = ImGui::GetTextLineHeightWithSpacing();
  metrics.font_size = font_size;
  metrics.measure = MeasureHere;
  metrics.icon_aspect = media.icon_aspect;
  metrics.icon_owner = media.icon_owner;
  metrics.media_measure = media.media_measure;
  metrics.media_owner = media.media_owner;
  metrics.words = media.words;
  // ⚠ UNE IMAGE OU UNE ICÔNE ARRIVÉE APRÈS LA PAGE LA FAIT REFAIRE : sans ce
  // dernier test, elle restait déformée — ou absente — jusqu'à la suivante.
  if (state.laid_once && state.laid_page == view.page && state.laid.wrap_width == wrap &&
      state.laid.font_size == font_size && !DialogMeasuresChanged(state.laid, metrics)) {
    return;
  }
  state.laid = LayOutDialog(view.lines, metrics);
  state.laid_page = view.page;
  state.laid_once = true;
}

// Le nom du menu d'un lien, dans la pile de la FENÊTRE.
constexpr const char* kTargetMenuId = "##menu_lien";

// Les gestes sur une zone survolée qui porte une cible de la maison.
//
// ⚠ LES MÊMES QUE DANS LA CHATBOX, et par le même module : gauche décrit, Maj
// repose le lien dans la saisie, droite ouvre le menu. Une convention qui se
// réinvente par surface finit par diverger — c'est ce que `chat_link.h`
// raconte, et ce fichier serait le quatrième exemplaire.
void TargetGestures(const DialogTarget& target, int rank, bool hovered, NpcDialogWindowState& state,
                    NpcDialogAnswer* answer) {
  if (!hovered) {
    return;
  }
  answer->hovered = target;
  const ChatLinkGesture gesture = HitChatLink(hovered);
  if (gesture == ChatLinkGesture::kNothing) {
    return;
  }
  if (gesture == ChatLinkGesture::kMenu) {
    // Mis de côté : le menu s'ouvrira hors de la zone défilante.
    state.menu_target = rank;
    return;
  }
  answer->target = target;
  answer->gesture = gesture;
}

void DrawFragments(NpcDialogWindowState& state, const NpcDialogArt& media, ImVec2 origin,
                   NpcDialogAnswer* answer) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const float font_size = ImGui::GetFontSize();
  const float line_height = ImGui::GetTextLineHeightWithSpacing();
  const ImU32 plain = ImGui::GetColorU32(ImGuiCol_Text);
  // La bande réellement visible. Hors d'elle, un fragment ne produirait que des
  // sommets pour rien — et une liste de deux cents monstres en compte des
  // milliers, tous empilés dans la même liste de dessin à chaque image.
  const float view_top = ImGui::GetWindowPos().y;
  const float view_bottom = view_top + ImGui::GetWindowSize().y;

  for (const DialogFragment& fragment : state.laid.fragments) {
    const ImVec2 at(origin.x + fragment.left, origin.y + fragment.top);
    if (at.y > view_bottom + kClipSlackPixels) {
      break;
    }
    if (at.y + fragment.height + line_height < view_top) {
      continue;
    }
    const ImVec2 corner(at.x + fragment.width, at.y + fragment.height);
    const bool hovered = ImGui::IsMouseHoveringRect(at, corner);
    const DialogTarget* const target =
        fragment.target >= 0 && fragment.target < static_cast<int>(state.laid.targets.size())
            ? &state.laid.targets[static_cast<std::size_t>(fragment.target)]
            : nullptr;

    if (fragment.media != DialogMedia::kNone) {
      const std::string_view source =
          fragment.source >= 0 && fragment.source < static_cast<int>(state.laid.sources.size())
              ? std::string_view(state.laid.sources[static_cast<std::size_t>(fragment.source)])
              : std::string_view();
      const bool shown = media.draw_media != nullptr &&
                         media.draw_media(fragment, source, at, corner, media.media_owner);
      if (!shown) {
        // 🔴 ON NE SE TAIT PAS : une image manquante et muette est indiscernable
        // d'un script qui ne l'a jamais posée, et c'est le staff qui écrit ces
        // balises — il doit voir son erreur.
        into->AddRect(at, corner, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        into->AddText(ImVec2(at.x + 4.0F, at.y + 3.0F), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      core::Tr("[image absente]"));
      }
      if (target != nullptr) {
        TargetGestures(*target, fragment.target, hovered, state, answer);
      }
      continue;
    }

    if (fragment.icon_item != 0) {
      if (media.icon_image != nullptr) {
        const SkinImage icon = media.icon_image(fragment.icon_item, media.icon_owner);
        if (icon.usable()) {
          into->AddImage(icon.id, at, corner);
        }
      }
      if (target != nullptr) {
        TargetGestures(*target, fragment.target, hovered, state, answer);
      }
      continue;
    }

    const bool clickable = fragment.link != DialogLink::kNone || target != nullptr;
    const ImU32 color = clickable ? LinkColor() : (fragment.color != 0 ? fragment.color : plain);
    AddTextRelief(into, at, color, fragment.text.c_str(), 0, ImVec2(0.0F, 0.0F), fragment.bold);
    if (!clickable) {
      continue;
    }
    // Souligné : c'est ce qui distingue un lien d'un mot coloré.
    const float underline_left = origin.x + fragment.underline_left;
    const float underline_top = at.y + font_size;
    into->AddLine(ImVec2(underline_left, underline_top), ImVec2(corner.x, underline_top),
                  LinkColor());
    const bool over = ImGui::IsMouseHoveringRect(ImVec2(underline_left, at.y),
                                                 ImVec2(corner.x, underline_top + 2.0F));
    if (!over) {
      continue;
    }
    if (target != nullptr) {
      TargetGestures(*target, fragment.target, over, state, answer);
      continue;
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    // ⚠ AU RELÂCHEMENT, comme les cases du sac : ce qu'un lien ouvre naît à
    // l'image suivante, et parti de l'enfoncement il s'ouvrirait derrière la
    // fenêtre d'où le clic vient.
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      answer->followed = fragment.link;
      answer->followed_payload = fragment.payload;
    }
  }
}

void DrawBody(NpcDialogWindowState& state, const NpcDialogView& view, const NpcDialogArt& media,
              const SkinArt& art, float height, NpcDialogAnswer* answer) {
  // ⚠ `SetNextWindowScroll` ET PAS `SetScrollY` : le second ne pose qu'une
  // CIBLE, appliquée au `Begin` SUIVANT — la page neuve s'afficherait une image
  // au défilement de l'ancienne, et bornée par SA hauteur de contenu.
  if (state.scrolled_page != view.page) {
    ImGui::SetNextWindowScroll(ImVec2(-1.0F, 0.0F));
    state.scrolled_page = view.page;
  }
  ImGui::BeginChild("texte", ImVec2(0.0F, height), ImGuiChildFlags_None);
  const float wrap = std::max(ImGui::GetContentRegionAvail().x, 1.0F);
  RelayOut(state, view, media, wrap);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  DrawFragments(state, media, origin, answer);
  // Réserve la hauteur du contenu : c'est elle qui donne son ascenseur au
  // défilement, puisque rien n'est posé par des widgets.
  ImGui::Dummy(ImVec2(wrap, state.laid.height));
  // 🔴 L'ASCENSEUR DE L'ENFANT, PEINT ICI — tant qu'il est la fenêtre courante.
  // Sans cette ligne, la page défile SANS RIEN MONTRER : la palette rend les
  // pièces d'ImGui transparentes, puisqu'on les peint nous-mêmes.
  PaintWindowScrollbar(art);
  ImGui::EndChild();
}

// ── LE MENU ─────────────────────────────────────────────────────────────────

void DrawMenu(NpcDialogWindowState& state, const NpcDialogView& view, const NpcDialogArt& media,
              const SkinArt& art, float height, NpcDialogAnswer* answer) {
  ImGui::BeginChild("##menu_groupe", ImVec2(0.0F, height), false, ImGuiWindowFlags_NoScrollbar);

  bool validated_from_search = false;
  bool filtering = false;
  if (MenuIsSearchable(view)) {
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##filtre", core::Tr("Rechercher..."), state.menu_filter.data(),
                                 state.menu_filter.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
      validated_from_search = true;
    }
    filtering = ImGui::IsItemActive();
  }
  const std::string filter = core::AsciiLower(state.menu_filter.data());

  // Le clavier : la première option visée d'office, les flèches qui bouclent sur
  // le nombre d'options VISIBLES — connu avant de dessiner, donc sans le retard
  // d'une image.
  const int shown = static_cast<int>(VisibleOptions(state.menu_lowered, filter));
  if (state.menu_hot < 0) {
    state.menu_hot = 0;
  }
  bool moved = false;
  // ⚠ LES FLÈCHES N'APPARTIENNENT AU MENU QUE DEPUIS SON FILTRE OU HORS SAISIE :
  // dans la barre de chat, elles rappellent l'historique, et déplaçaient en plus
  // l'option visée.
  const bool arrows_ours = filtering || !ImGui::GetIO().WantTextInput;
  if (arrows_ours && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
    ++state.menu_hot;
    moved = true;
  }
  if (arrows_ours && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
    --state.menu_hot;
    moved = true;
  }
  if (shown > 0) {
    if (state.menu_hot < 0) {
      state.menu_hot = shown - 1;
    }
    if (state.menu_hot >= shown) {
      state.menu_hot = 0;
    }
  } else {
    state.menu_hot = 0;
  }

  // Les POSITIONS actuellement visibles, dans l'ordre d'affichage : sous filtre,
  // la place à l'écran n'a plus rien à voir avec le rang d'une option.
  std::vector<std::size_t> visible;
  visible.reserve(view.options.size());
  int chosen = -1;

  ImGui::BeginChild("##liste", ImVec2(0.0F, 0.0F), true);
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const float font_size = ImGui::GetFontSize();
  const ImU32 plain = ImGui::GetColorU32(ImGuiCol_Text);
  const float row_height = MenuRowHeight();
  const float pad_x = ImGui::GetStyle().ItemSpacing.x;
  // Aucun espace vertical entre les options : une liste, pas un formulaire.
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(pad_x, 0.0F));
  for (std::size_t rank = 0; rank < view.options.size(); ++rank) {
    if (!Matches(state.menu_lowered[rank], filter)) {
      continue;
    }
    const bool aimed = static_cast<int>(visible.size()) == state.menu_hot;
    visible.push_back(rank);

    // ⚠ L'IDENTITÉ EST LE RANG, PAS LE LIBELLÉ : un menu peut proposer deux fois
    // le même texte, et deux éléments de même identité ne se cliquent plus
    // séparément. Le libellé, lui, est peint par-dessus — chaque morceau garde
    // sa couleur.
    ImGui::PushID(static_cast<int>(rank));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (ImGui::Selectable("##option", aimed, ImGuiSelectableFlags_None, ImVec2(0.0F, row_height))) {
      chosen = static_cast<int>(rank);
    }
    ImGui::PopID();
    if (aimed && moved) {
      ImGui::SetScrollHereY(0.5F);
    }

    float pen_x = at.x + pad_x;
    const float pen_y = at.y + (row_height - font_size) * 0.5F;
    std::array<char, 16> number = {};
    std::snprintf(number.data(), number.size(), "%d. ", static_cast<int>(visible.size()));
    into->AddText(ImVec2(pen_x, pen_y), plain, number.data());
    pen_x += ImGui::CalcTextSize(number.data()).x;

    for (const DialogRun& run : state.menu_runs[rank]) {
      if (run.icon_item != 0) {
        if (media.icon_image == nullptr) {
          continue;
        }
        const SkinImage icon = media.icon_image(run.icon_item, media.icon_owner);
        if (!icon.usable()) {
          continue;
        }
        const float side = row_height - 1.0F;
        const float width = side * icon.width / icon.height;
        const float top = at.y + (row_height - side) * 0.5F;
        into->AddImage(icon.id, ImVec2(pen_x, top), ImVec2(pen_x + width, top + side));
        pen_x += width + 3.0F;
        continue;
      }
      if (run.media != DialogMedia::kNone) {
        // ⚠ UN MÉDIA DANS UNE OPTION RESTE À LA HAUTEUR DE SA LIGNE : une option
        // est une ligne cliquable d'un seul tenant, et y laisser grandir une
        // image donnerait une liste dont chaque entrée aurait sa taille. Un
        // sprite y devient une vignette, ce qui est l'usage attendu.
        const float side = row_height - 1.0F;
        const float top = at.y + (row_height - side) * 0.5F;
        DialogFragment cell;
        cell.width = side;
        cell.height = side;
        cell.media = run.media;
        cell.monster_id = run.monster_id;
        if (media.draw_media != nullptr) {
          static_cast<void>(media.draw_media(cell, run.source, ImVec2(pen_x, top),
                                             ImVec2(pen_x + side, top + side), media.media_owner));
        }
        pen_x += side + 3.0F;
        continue;
      }
      if (run.text.empty()) {
        continue;
      }
      // 🔴 UN LIEN DANS UNE OPTION SE VOIT MAIS NE SE CLIQUE PAS : le clic
      // appartient au choix. Le rectangle d'une ligne la couvre entière, et
      // disputer ce clic ferait qu'un joueur visant « 3. <Poring> » ouvrirait une
      // fiche au lieu de répondre.
      const bool clickable = run.link != DialogLink::kNone || run.target.valid();
      const ImU32 color = clickable ? LinkColor() : (run.color != 0 ? run.color : plain);
      AddTextRelief(into, ImVec2(pen_x, pen_y), color, run.text.c_str(), 0, ImVec2(0.0F, 0.0F),
                    run.bold);
      pen_x += ImGui::CalcTextSize(run.text.c_str()).x;
    }
  }
  ImGui::PopStyleVar();
  // La liste déborde dès qu'elle passe son plafond de lignes : son ascenseur se
  // peint comme celui du corps.
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  ImGui::EndChild();

  // Les chiffres : la N-ième option VISIBLE. Hors saisie — taper « 1 » dans le
  // filtre choisirait la première — et sans modificateur : Ctrl/Alt/Maj + chiffre
  // appartient aux raccourcis du jeu.
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !io.KeyShift) {
    for (int key = 1; key <= 9 && key <= static_cast<int>(visible.size()); ++key) {
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + (key - 1)), false)) {
        chosen = static_cast<int>(visible[static_cast<std::size_t>(key - 1)]);
      }
    }
  }
  // Entrée valide l'option visée — depuis le filtre, ou librement hors saisie.
  const bool validated = validated_from_search ||
                         (!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)));
  if (validated && state.menu_hot >= 0 && state.menu_hot < static_cast<int>(visible.size())) {
    chosen = static_cast<int>(visible[static_cast<std::size_t>(state.menu_hot)]);
  }
  // LA POSITION VISÉE, PUBLIÉE POUR LE BOUTON « OK » : il est dessiné APRÈS
  // cette fonction, et rien d'autre ne relie une ligne de l'écran à sa place
  // dans la liste.
  state.menu_aimed = (state.menu_hot >= 0 && state.menu_hot < static_cast<int>(visible.size()))
                         ? static_cast<int>(visible[static_cast<std::size_t>(state.menu_hot)])
                         : -1;
  // 🔴 EN ATTENTE DE LA RÉPONSE, RIEN NE PART. `BeginDisabled` rend les lignes
  // inertes à la souris, mais ne coupe pas `IsKeyPressed` : les chiffres et
  // Entrée émettaient un second choix, que le serveur refuse.
  if (chosen >= 0 && !view.awaiting_reply) {
    answer->intent = NpcDialogIntent::kChoose;
    answer->option_index = static_cast<std::size_t>(chosen);
  }
}

// ── LA SAISIE ───────────────────────────────────────────────────────────────

}  // namespace

std::int32_t ParsedNumber(std::string_view typed) {
  // ⚠ LU EN 64 BITS PUIS BORNÉ, et non par `atoi` : seize chiffres dépassent
  // un entier 32 bits, et `atoi` sur un tel nombre est un comportement indéfini.
  // Un champ vide ou illisible vaut zéro, comme au client d'origine.
  std::int64_t read = 0;
  const auto [end, error] = std::from_chars(typed.data(), typed.data() + typed.size(), read);
  static_cast<void>(end);
  if (error == std::errc::result_out_of_range) {
    read = typed.starts_with('-') ? std::numeric_limits<std::int64_t>::min()
                                  : std::numeric_limits<std::int64_t>::max();
  } else if (error != std::errc{}) {
    return 0;
  }
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(
      read, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}

namespace {

void DrawInput(NpcDialogWindowState& state, const NpcDialogView& view, const SkinArt& art,
               NpcDialogAnswer* answer) {
  if (state.focus_input) {
    ImGui::SetKeyboardFocusHere();
    state.focus_input = false;
  }
  const float button = SkinButtonWidth(core::Tr("OK"), art) + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetNextItemWidth(-button);
  bool validated = false;
  if (view.prompt == NpcDialogPrompt::kNumber) {
    validated =
        ImGui::InputText("##nombre", state.number.data(), state.number.size(),
                         ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
  } else {
    // ⚠ LE CHAMP NE PREND QUE CE QUE LE SERVEUR GARDERA : au-delà, il tronque
    // sans le dire, et le joueur croit avoir écrit ce qu'il voit.
    const std::size_t room =
        view.text_limit == 0 ? state.text.size() : std::min(state.text.size(), view.text_limit + 1);
    validated =
        ImGui::InputText("##texte", state.text.data(), room, ImGuiInputTextFlags_EnterReturnsTrue);
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("OK"), art) || validated) {
    if (view.prompt == NpcDialogPrompt::kNumber) {
      answer->intent = NpcDialogIntent::kNumber;
      answer->number = ParsedNumber(state.number.data());
    } else {
      answer->intent = NpcDialogIntent::kText;
      answer->text = state.text.data();
    }
  }
}

// ── LE TITRE ────────────────────────────────────────────────────────────────

// Recomposé seulement quand il le faut : c'est un nom de PNJ, pas un littéral du
// catalogue, et le refaire à chaque image coûterait une allocation par image.
void ComposeTitle(NpcDialogWindowState& state, const NpcDialogView& view) {
  const std::size_t language = core::LanguageSerial();
  if (state.title_for == view.name && state.title_language == language && !state.title.empty()) {
    return;
  }
  state.title_for = view.name;
  state.title_language = language;
  // ⚠ LE LITTÉRAL ET NON LA CONSTANTE : une chaîne construite n'entre dans aucun
  // catalogue, et c'est la lecture qui traduit — voir `core::TrText`.
  state.title = view.name.empty() ? std::string(core::Tr("Dialogue")) : std::string(view.name);
  state.title += "###";
  state.title += kNpcDialogWindowId;
}

}  // namespace

NpcDialogAnswer DrawNpcDialogWindow(NpcDialogWindowState& state, const NpcDialogView& view,
                                    const SkinArt& art, const NpcDialogArt& media) {
  NpcDialogAnswer answer;
  ComposeTitle(state, view);

  // UNE LISTE NEUVE REPART À BLANC : le filtre d'un menu ne doit pas masquer les
  // options du suivant, et la visée du clavier retombe sur la première.
  if (state.menu_page != view.page || state.menu_list != view.menu) {
    state.menu_page = view.page;
    state.menu_list = view.menu;
    state.menu_filter[0] = '\0';
    state.menu_hot = -1;
    state.menu_aimed = -1;
  }
  // 🔴 C'EST LE CHANGEMENT DE DEMANDE QUI VIDE LE CHAMP, pas sa présence :
  // remettre à zéro à chaque image effacerait ce que le joueur est en train de
  // taper.
  if (state.shown_prompt != view.prompt) {
    state.shown_prompt = view.prompt;
    if (view.prompt == NpcDialogPrompt::kNumber || view.prompt == NpcDialogPrompt::kText) {
      state.number[0] = '\0';
      state.text[0] = '\0';
      state.focus_input = true;
    }
  }

  if (state.place) {
    ImGui::SetNextWindowPos(ImVec2(kFirstLeftPixels, kFirstTopPixels), ImGuiCond_FirstUseEver);
    // ⚠ DEVANT, ET SEULEMENT À L'OUVERTURE : une fenêtre à laquelle il FAUT
    // répondre ne doit pas naître sous le sac, et se remettre devant à chaque
    // image la rendrait impossible à quitter des yeux.
    ImGui::SetNextWindowFocus();
    state.place = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kLeastWidthPixels, kLeastHeightPixels),
                                      ImVec2(FLT_MAX, FLT_MAX));

  // ⚠ ON NE FERME QUE LÀ OÙ CELA A UN SENS POUR LE SERVEUR : un « fermer »
  // (fermeture réelle) ou un menu (annulation convenue). PAS pendant un
  // « suivant » : y envoyer une fermeture ferait AVANCER le script au lieu de le
  // quitter, et le client officiel n'offre d'ailleurs que « Suivant » à ce
  // moment.
  const bool closable =
      view.prompt == NpcDialogPrompt::kClose || view.prompt == NpcDialogPrompt::kMenu;
  // ÉCHAP EST PASSÉ PAR LÀ : la pile l'a baissé après le dessin de l'image
  // précédente, et c'est une fermeture. Le drapeau se relève aussitôt — la
  // fenêtre suit le monde, et c'est le serveur qui la fera disparaître.
  if (!state.open) {
    state.open = true;
    if (closable) {
      answer.intent = NpcDialogIntent::kClose;
    }
  }
  bool open = true;
  // ⛔ PAS DE MINIMISER, ET `NoCollapse` EST CE QUI LE RETIRE : une fenêtre à
  // laquelle il FAUT répondre n'a rien à faire repliée en barre de titre. Le
  // joueur l'y laisserait sans savoir que le script l'attend, et rien à l'écran
  // ne le lui dirait. Le client d'origine n'en donne pas non plus.
  PoseBugReportButton(state.bug_report);
  if (!BeginSkinnedWindow(state.title.c_str(), closable ? &open : nullptr, art,
                          SkinnedTitle::kDescription,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                              ImGuiWindowFlags_NoCollapse)) {
    EndSkinnedWindow();
    if (!open) {
      answer.intent = NpcDialogIntent::kClose;
    }
    return answer;
  }
  answer.report_bug = state.bug_report && TitleActionClicked(0);

  // ── La disposition à PIED FIXE ────────────────────────────────────────────
  // Texte (souple, défilement interne), menu (borné, défilement interne),
  // saisie, puis les boutons ÉPINGLÉS en bas. La fenêtre n'a pas d'ascenseur :
  // celui d'une fenêtre habillée passerait par-dessus la barre qu'on peint.
  const ImGuiStyle& style = ImGui::GetStyle();
  const float available = ImGui::GetContentRegionAvail().y;
  const float separator = style.ItemSpacing.y + 1.0F;
  // Le trait au-dessus des boutons n'est dessiné que SANS menu : la liste porte
  // déjà son cadre, et deux traits à quelques pixels l'un de l'autre se lisent
  // comme un défaut d'alignement.
  const bool footer_rule = view.options.empty();
  const float footer =
      (footer_rule ? separator : 0.0F) + SkinButtonHeight(art) + kFooterBottomPixels;
  const bool asking =
      view.prompt == NpcDialogPrompt::kNumber || view.prompt == NpcDialogPrompt::kText;
  const float input = asking ? (separator + ImGui::GetFrameHeightWithSpacing()) : 0.0F;

  float menu = 0.0F;
  if (!view.options.empty()) {
    const float body = available - footer - input;
    ReadMenu(state, view, media.words);
    menu = MenuWantedHeight(view, state.menu_lowered, core::AsciiLower(state.menu_filter.data()));
    // ⚠ UN PIXEL AU MOINS, JAMAIS ZÉRO : une hauteur nulle fait prendre à un
    // enfant d'ImGui toute la place restante, et le menu mangerait la fenêtre au
    // lieu de disparaître. Le cas arrive dès qu'on rétrécit la fenêtre sous ce
    // qu'il faudrait pour montrer les deux.
    menu = std::clamp(menu, 1.0F, std::max(body - kLeastTextPixels, 1.0F));
  }
  const float menu_total = view.options.empty() ? 0.0F : (separator + menu);
  const float text_height = std::max(available - footer - input - menu_total, 1.0F);

  // Le portrait de page, dans sa colonne, RÉSERVÉE AVANT le texte pour que le
  // repli voie la largeur réellement disponible. Rien n'est réservé si le sprite
  // manque : mieux vaut le texte pleine largeur qu'une colonne vide.
  const std::uint32_t portrait = PageMonsterPortrait(view.lines);
  if (portrait != 0 && media.draw_portrait != nullptr) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float height = std::min(text_height, kPortraitMaxHeightPixels);
    if (media.draw_portrait(portrait, at, ImVec2(at.x + kPortraitWidthPixels, at.y + height),
                            media.media_owner)) {
      ImGui::Dummy(ImVec2(kPortraitWidthPixels, text_height));
      ImGui::SameLine();
    }
  }

  DrawBody(state, view, media, art, text_height, &answer);

  // LE MENU D'UN LIEN, ouvert ICI : le nom d'un popup se hache avec la pile
  // d'identifiants de sa fenêtre, et le clic droit s'est produit dans la zone
  // défilante du texte. Même mécanique que le fil de la conversation.
  //
  // ⚠ LE MENU DE LA CHATBOX NE VAUT QUE POUR LES GENRES DONT LES ENTRÉES NE
  // PARLENT QUE D'EUX : voir `DialogTargetMenuKind`.
  if (state.menu_target >= 0 && state.menu_target < static_cast<int>(state.laid.targets.size())) {
    if (DialogTargetMenuKind(state.laid.targets[static_cast<std::size_t>(state.menu_target)].kind)
            .has_value()) {
      state.menu_shown = state.menu_target;
      ImGui::OpenPopup(kTargetMenuId);
    }
    state.menu_target = -1;
  }
  // ⚠ LA CIBLE SE RELIT DANS LA MISE EN PAGE DE CETTE IMAGE-CI : le popup survit
  // à plusieurs images, la mise en page aussi tant que la page ne change pas. Si
  // elle change, le rang ne désigne plus rien et le menu s'en va — ce qui est
  // exactement ce qu'on veut d'un menu ouvert sur un texte qui n'est plus là.
  if (state.menu_shown >= 0 && state.menu_shown < static_cast<int>(state.laid.targets.size())) {
    const DialogTarget& aimed = state.laid.targets[static_cast<std::size_t>(state.menu_shown)];
    // ⚠ PAS DE JETON ICI, CONTRAIREMENT AU FIL : un lien de script désigne
    // l'objet de CATALOGUE, sans raffinement ni cartes. Son identifiant le dit
    // en entier.
    const ChatLinkKind menu_kind = DialogTargetMenuKind(aimed.kind).value_or(ChatLinkKind::kItem);
    if (const ChatLinkAsk chosen = DrawChatLinkMenu(kTargetMenuId, menu_kind, aimed.id, aimed.id,
                                                    aimed.label, view.vote_shop);
        chosen.gesture != ChatLinkGesture::kNothing) {
      answer.target = aimed;
      answer.gesture = chosen.gesture;
    }
  }

  // 🔴 EN ATTENTE DE LA RÉPONSE, LA PAGE RESTE ENTIÈRE MAIS INERTE : les widgets
  // gardent leur place — aucun saut de pied de fenêtre — et le joueur ne peut pas
  // répondre deux fois. Sans cela, un second clic sur « Suivant » fait avancer le
  // script d'une page de plus.
  ImGui::BeginDisabled(view.awaiting_reply);
  if (!view.options.empty()) {
    ImGui::Separator();
    DrawMenu(state, view, media, art, menu, &answer);
  }
  if (asking) {
    ImGui::Separator();
    DrawInput(state, view, art, &answer);
  }

  if (footer_rule) {
    ImGui::Separator();
  }
  // Entrée et Espace valident le bouton principal — seulement hors menu, qui
  // consomme Entrée pour l'option visée, et hors saisie, qui a son propre OK.
  //
  // 🔴 `WantTextInput` EST CE QUI TIENT LA FRONTIÈRE : n'importe quelle zone de
  // texte à l'écran — la barre de chat, le filtre d'une autre fenêtre — verrait
  // sinon sa frappe faire AUSSI avancer le script, et la moindre espace tapée
  // vaudrait un « Suivant ».
  const ImGuiIO& io = ImGui::GetIO();
  const bool keyboard = view.options.empty() && !asking && !view.awaiting_reply &&
                        !io.WantTextInput &&
                        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                         ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
                         ImGui::IsKeyPressed(ImGuiKey_Space, false));
  bool placed = false;
  if (view.prompt == NpcDialogPrompt::kNext) {
    if (SkinButton(core::Tr("Suivant"), art) || keyboard) {
      answer.intent = NpcDialogIntent::kNext;
    }
    placed = true;
  }
  // UN MENU SE RÉPOND DÉJÀ AU CLIC ET À ENTRÉE, et le client officiel offre
  // quand même « OK ». Sans lui, un joueur qui a navigué aux flèches ne voit
  // qu'« Annuler » et croit la liste inerte : rien d'autre ne dit que la ligne
  // surlignée est un choix en attente de validation.
  //
  // ⚠ GRISÉ QUAND LE FILTRE NE LAISSE AUCUNE LIGNE, plutôt que retiré : un
  // bouton qui va et vient au rythme de la frappe ferait glisser « Annuler »
  // sous le curseur, où le clic suivant l'atteindrait.
  //
  // ⚠ LE CLAVIER NE PASSE PAS PAR ICI : Entrée est lue dans la liste, seule à
  // savoir ce que le filtre laisse voir.
  if (view.prompt == NpcDialogPrompt::kMenu) {
    if (placed) {
      ImGui::SameLine();
    }
    ImGui::BeginDisabled(state.menu_aimed < 0);
    if (SkinButton(core::Tr("OK"), art)) {
      answer.intent = NpcDialogIntent::kChoose;
      answer.option_index = static_cast<std::size_t>(state.menu_aimed);
    }
    ImGui::EndDisabled();
    placed = true;
  }
  if (closable) {
    if (placed) {
      ImGui::SameLine();
    }
    const char* const label =
        view.prompt == NpcDialogPrompt::kClose ? core::Tr("Fermer") : core::Tr("Annuler");
    if (SkinButton(label, art) || (keyboard && !placed)) {
      answer.intent = NpcDialogIntent::kClose;
    }
  }
  ImGui::EndDisabled();

  EndSkinnedWindow();
  if (!open && answer.intent == NpcDialogIntent::kNothing) {
    answer.intent = NpcDialogIntent::kClose;
  }
  return answer;
}

}  // namespace ro::ui
