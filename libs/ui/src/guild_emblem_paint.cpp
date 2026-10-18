// L'ONGLET « DESSINER » DE L'ÉDITEUR D'EMBLÈME : un canevas de 24 × 24.
//
// Clic gauche : la couleur courante ; clic droit : la gomme. Le canevas est une
// liste de rectangles plutôt qu'une texture — rien à invalider, et le damier de
// fond montre où l'emblème sera transparent.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "guild_panes.h"
#include "ui_scale.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// ── LE CANEVAS ──────────────────────────────────────────────────────────────
constexpr float kCellPixels = 14.0F;
// Un repère tous les quatre pixels, pour centrer un motif à la main.
constexpr int kGridEvery = 4;
constexpr float kPreviewGapPixels = 16.0F;
constexpr ImU32 kCheckerDark = IM_COL32(150, 150, 156, 255);
constexpr ImU32 kCheckerLight = IM_COL32(190, 190, 196, 255);
constexpr ImU32 kCheckerDarkVeiled = IM_COL32(150, 150, 156, 200);
constexpr ImU32 kCheckerLightVeiled = IM_COL32(190, 190, 196, 200);
constexpr ImU32 kGridLine = IM_COL32(0, 0, 0, 40);
constexpr ImU32 kCanvasBorder = IM_COL32(60, 60, 70, 220);
constexpr ImU32 kShapeEdge = IM_COL32(255, 255, 255, 120);
constexpr int kShapeAlpha = 170;
constexpr int kOpaque = 255;
constexpr unsigned kRedShift = 16;
constexpr unsigned kGreenShift = 8;
constexpr std::uint32_t kChannelMask = 0xFFU;

// ── LES OUTILS ET LE NUANCIER ───────────────────────────────────────────────
constexpr float kToolButtonPixels = 80.0F;
constexpr float kBrushSliderPixels = 140.0F;
// Les teintes franches passent mieux sur 24 × 24 qu'un dégradé. ⚠ LE MAGENTA
// N'Y EST PAS : il vaut « transparent », et a son bouton à lui.
constexpr std::array<std::uint32_t, 22> kSwatches = {
    0x000000, 0x404040, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x7F0000, 0xD92B2B, 0xFF7F27,
    0xFFC90E, 0xFFF200, 0x0F5A0F, 0x22B14C, 0x7FE817, 0x0B2C6B, 0x2B6FD9, 0x59C7F0,
    0x4B0082, 0x9B30FF, 0xD94BC0, 0x8B5A2B, 0xC69C6D, 0x5A3A1A,
};
constexpr std::size_t kSwatchesPerRow = 11;
constexpr float kSwatchInsetPixels = 2.0F;
constexpr ImU32 kSwatchBorder = IM_COL32(40, 40, 48, 220);
constexpr float kClearSwatchStretch = 1.6F;
constexpr ImU32 kActiveInkColor = IM_COL32(26, 89, 179, 255);
constexpr float kChannelMost = 255.0F;

// ── LA SUITE ────────────────────────────────────────────────────────────────
constexpr float kUndoPixels = 90.0F;
constexpr float kClearAllPixels = 110.0F;
constexpr float kCurrentPixels = 150.0F;
constexpr float kItemFieldPixels = 90.0F;
constexpr float kImportPixels = 110.0F;
constexpr float kGalleryTogglePixels = 110.0F;
constexpr float kGalleryCellPixels = 32.0F;
constexpr int kGalleryRows = 3;
constexpr ImU32 kGalleryHover = IM_COL32(255, 255, 255, 40);
constexpr ImU32 kGalleryEmpty = IM_COL32(120, 120, 120, 120);
constexpr float kSendPixels = 160.0F;
constexpr float kSaveNamePixels = 150.0F;
constexpr float kSavePixels = 190.0F;

// ⚠ DES AIGUILLAGES DE LITTÉRAUX, et non des tables : `core::Tr` veut voir sa
// chaîne à l'appel, sans quoi elle n'entrerait dans aucun catalogue.
[[nodiscard]] const char* ToolName(EmblemTool tool) {
  switch (tool) {
    case EmblemTool::kPencil:
      return core::Tr("Crayon");
    case EmblemTool::kEraser:
      return core::Tr("Gomme");
    case EmblemTool::kFill:
      return core::Tr("Remplir");
    case EmblemTool::kLine:
      return core::Tr("Ligne");
    case EmblemTool::kRect:
      return core::Tr("Rectangle");
    case EmblemTool::kEllipse:
      return core::Tr("Ellipse");
  }
  return "";
}

[[nodiscard]] const char* ToolHint(EmblemTool tool) {
  switch (tool) {
    case EmblemTool::kPencil:
      return core::Tr("Peint à main levée.");
    case EmblemTool::kEraser:
      return core::Tr("Efface (rend transparent).");
    case EmblemTool::kFill:
      return core::Tr("Remplit la zone de même couleur.");
    case EmblemTool::kLine:
      return core::Tr("Tire une ligne d'un point à l'autre.");
    case EmblemTool::kRect:
      return core::Tr("Tire un rectangle entre deux coins.");
    case EmblemTool::kEllipse:
      return core::Tr("Tire une ellipse inscrite dans le rectangle tiré.");
  }
  return "";
}

[[nodiscard]] ImU32 Opaque(std::uint32_t color, int alpha = kOpaque) {
  return IM_COL32((color >> kRedShift) & kChannelMask, (color >> kGreenShift) & kChannelMask,
                  color & kChannelMask, alpha);
}

[[nodiscard]] bool IsShapeTool(EmblemTool tool) {
  return tool == EmblemTool::kLine || tool == EmblemTool::kRect || tool == EmblemTool::kEllipse;
}

// Une case du canevas : sa couleur, ou le damier du vide.
void PaintCell(ImDrawList* into, ImVec2 origin, float cell, int column, int row,
               std::uint32_t color, bool veiled) {
  const ImVec2 low(origin.x + static_cast<float>(column) * cell,
                   origin.y + static_cast<float>(row) * cell);
  const ImVec2 high(low.x + cell, low.y + cell);
  if (color == kEmblemCanvasClear) {
    const bool dark = ((column + row) & 1) != 0;
    into->AddRectFilled(low, high,
                        veiled ? (dark ? kCheckerDarkVeiled : kCheckerLightVeiled)
                               : (dark ? kCheckerDark : kCheckerLight));
  } else {
    into->AddRectFilled(low, high, Opaque(color, veiled ? kShapeAlpha : kOpaque));
  }
  if (veiled) {
    into->AddRect(low, high, kShapeEdge);
  }
}

// LE GESTE SUR LE CANEVAS. Un « coup » va de l'appui au relâchement : une
// seule entrée d'annulation. On peint tant que le bouton reste enfoncé APRÈS un
// appui dans le canevas — sortir d'un pixel en pleine ligne ne coupe pas le
// trait.
void PaintGesture(EmblemCanvas& canvas, ImDrawList* into, ImVec2 origin, float cell, bool hovered) {
  const bool left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool drawing = ImGui::IsItemActive() || (hovered && (left || right));
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const int column =
      std::clamp(static_cast<int>((mouse.x - origin.x) / cell), 0, kEmblemCanvasSide - 1);
  const int row =
      std::clamp(static_cast<int>((mouse.y - origin.y) / cell), 0, kEmblemCanvasSide - 1);
  const std::uint32_t ink = right ? kEmblemCanvasClear : EmblemInk(canvas);

  if (!IsShapeTool(canvas.tool)) {
    if (drawing && (left || right)) {
      if (!canvas.stroke_open) {
        PushEmblemUndo(canvas);
        canvas.stroke_open = true;
        canvas.last_x = column;
        canvas.last_y = row;
      }
      if (canvas.tool == EmblemTool::kFill && !right) {
        FillEmblemCanvas(canvas, column, row, ink);
      } else {
        const std::uint32_t stroke =
            canvas.tool == EmblemTool::kEraser || right ? kEmblemCanvasClear : ink;
        StrokeEmblemCanvas(canvas, canvas.last_x, canvas.last_y, column, row, stroke);
      }
      canvas.last_x = column;
      canvas.last_y = row;
    }
    if (!left && !right) {
      canvas.stroke_open = false;
    }
    return;
  }
  // LES FORMES : le point de départ, l'aperçu, et la peinture au relâchement.
  // Tant que le bouton tient, le dessin dessous est intact.
  if (!canvas.shape_active && drawing && (left || right)) {
    canvas.shape_active = true;
    canvas.shape_erase = right;
    canvas.shape_x0 = column;
    canvas.shape_y0 = row;
  }
  if (!canvas.shape_active) {
    return;
  }
  const EmblemMask mask = EmblemShapeMask(canvas, column, row);
  const std::uint32_t color = canvas.shape_erase ? kEmblemCanvasClear : EmblemInk(canvas);
  if (!left && !right) {
    PushEmblemUndo(canvas);
    ApplyEmblemMask(canvas, mask, color);
    canvas.shape_active = false;
    return;
  }
  // L'APERÇU : la couleur visée en semi-transparence et un liseré, lisible sur
  // un fond de même teinte ; une forme « vide » s'y montre en damier, comme le
  // sera le résultat — jamais en magenta.
  for (std::size_t index = 0; index < kEmblemCanvasPixels; ++index) {
    if (mask[index]) {
      PaintCell(into, origin, cell, static_cast<int>(index % kEmblemCanvasSide),
                static_cast<int>(index / kEmblemCanvasSide), color, /*veiled=*/true);
    }
  }
}

void DrawCanvas(EmblemCanvas& canvas) {
  const float cell = Px(kCellPixels);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float side = cell * static_cast<float>(kEmblemCanvasSide);
  ImGui::InvisibleButton("##canevas", ImVec2(side, side),
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* into = ImGui::GetWindowDrawList();
  for (int row = 0; row < kEmblemCanvasSide; ++row) {
    for (int column = 0; column < kEmblemCanvasSide; ++column) {
      PaintCell(into, origin, cell, column, row,
                canvas.pixel[static_cast<std::size_t>(row) * kEmblemCanvasSide +
                             static_cast<std::size_t>(column)],
                /*veiled=*/false);
    }
  }
  for (int line = 0; line <= kEmblemCanvasSide; line += kGridEvery) {
    const float at = static_cast<float>(line) * cell;
    into->AddLine(ImVec2(origin.x + at, origin.y), ImVec2(origin.x + at, origin.y + side),
                  kGridLine);
    into->AddLine(ImVec2(origin.x, origin.y + at), ImVec2(origin.x + side, origin.y + at),
                  kGridLine);
  }
  into->AddRect(origin, ImVec2(origin.x + side, origin.y + side), kCanvasBorder);
  PaintGesture(canvas, into, origin, cell, hovered);

  // L'APERÇU À L'ÉCHELLE UN, ce que verront les autres joueurs.
  const ImVec2 preview(origin.x + side + Px(kPreviewGapPixels), origin.y);
  for (std::size_t index = 0; index < kEmblemCanvasPixels; ++index) {
    const std::uint32_t color = canvas.pixel[index];
    if (color == kEmblemCanvasClear) {
      continue;
    }
    const float x = preview.x + static_cast<float>(index % kEmblemCanvasSide);
    const float y = preview.y + static_cast<float>(index / kEmblemCanvasSide);
    into->AddRectFilled(ImVec2(x, y), ImVec2(x + 1.0F, y + 1.0F), Opaque(color));
  }
  into->AddRect(ImVec2(preview.x - 1.0F, preview.y - 1.0F),
                ImVec2(preview.x + static_cast<float>(kEmblemCanvasSide) + 1.0F,
                       preview.y + static_cast<float>(kEmblemCanvasSide) + 1.0F),
                kCanvasBorder);
}

void DrawTools(EmblemCanvas& canvas, const SkinArt& art) {
  ImGui::Spacing();
  ImGui::AlignTextToFramePadding();
  ColoredText(kGuildLabelColor, core::Tr("Outil :"));
  ImGui::SameLine();
  for (std::size_t rank = 0; rank < kEmblemToolCount; ++rank) {
    const auto tool = static_cast<EmblemTool>(rank);
    ImGui::PushID(static_cast<int>(rank));
    // L'outil courant reste enfoncé, son libellé en gras.
    if (SkinToggleButton(ToolName(tool), canvas.tool == tool, art,
                         ImVec2(Px(kToolButtonPixels), 0.0F))) {
      canvas.tool = tool;
    }
    SkinTooltip(ToolHint(tool));
    ImGui::PopID();
    // Deux rangées de trois : ce qui peint au fil du geste, puis ce qui se tire.
    if (tool != EmblemTool::kFill && tool != EmblemTool::kEllipse) {
      ImGui::SameLine();
    }
  }
  ImGui::SameLine();
  ColoredText(kGuildLabelColor, core::Tr("(clic droit = efface)"));

  ImGui::SetNextItemWidth(Px(kBrushSliderPixels));
  static_cast<void>(
      SkinSliderInt(core::Tr("Épaisseur"), &canvas.brush, 1, kEmblemBrushMost, art, "%d px"));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Symétrie"), &canvas.mirror, art));
  SkinTooltip(core::Tr("Chaque trait est répété en miroir gauche/droite."));
  ImGui::SameLine();
  // Grisée plutôt que cachée ailleurs que sur le rectangle et l'ellipse : la
  // barre d'outils ne saute pas d'un outil à l'autre.
  ImGui::BeginDisabled(canvas.tool != EmblemTool::kRect && canvas.tool != EmblemTool::kEllipse);
  static_cast<void>(SkinCheckBox(core::Tr("Forme pleine"), &canvas.filled, art));
  ImGui::EndDisabled();
}

// Choisir une teinte, c'est quitter le « vide », et la gomme avec lui.
void TakeInk(EmblemCanvas& canvas) {
  canvas.color_clear = false;
  if (canvas.tool == EmblemTool::kEraser) {
    canvas.tool = EmblemTool::kPencil;
  }
}

void DrawColors(EmblemCanvas& canvas, const SkinArt& art) {
  ImGui::AlignTextToFramePadding();
  ColoredText(kGuildLabelColor, core::Tr("Couleur :"));
  ImGui::SameLine();
  if (SkinColorSwatch("##couleur", canvas.color, art)) {
    TakeInk(canvas);
  }
  ImGui::SameLine();
  // LA TRANSPARENCE EST UNE COULEUR : elle laisse l'outil tel quel, pour qu'on
  // puisse en remplir une zone ou en tracer une forme.
  ImGui::AlignTextToFramePadding();
  ColoredText(kGuildLabelColor, core::Tr("Transparence :"));
  ImGui::SameLine();
  const float swatch = ImGui::GetFrameHeight();
  if (ImGui::ColorButton("##transparent", GuildInk(Opaque(kEmblemCanvasClear)),
                         ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                         ImVec2(swatch * kClearSwatchStretch, swatch))) {
    canvas.color_clear = true;
  }
  SkinTooltip(
      core::Tr("Magenta pur (255, 0, 255) : ces pixels sont transparents en jeu.\n"
               "S'utilise avec n'importe quel outil (remplissage, formes…)."));
  if (canvas.color_clear) {
    ImGui::SameLine();
    ColoredText(kActiveInkColor, core::Tr("couleur active"));
  }
  ColoredText(kGuildLabelColor, core::Tr("Palette :"));
  ImGui::SameLine();
  const float side = ImGui::GetFrameHeight() - Px(kSwatchInsetPixels);
  for (std::size_t index = 0; index < kSwatches.size(); ++index) {
    const std::uint32_t color = kSwatches[index];
    ImGui::PushID(static_cast<int>(index));
    if (index % kSwatchesPerRow != 0) {
      ImGui::SameLine();
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##teinte", ImVec2(side, side))) {
      TakeInk(canvas);
      canvas.color = {static_cast<float>((color >> kRedShift) & kChannelMask) / kChannelMost,
                      static_cast<float>((color >> kGreenShift) & kChannelMask) / kChannelMost,
                      static_cast<float>(color & kChannelMask) / kChannelMost};
    }
    ImDrawList* into = ImGui::GetWindowDrawList();
    into->AddRectFilled(at, ImVec2(at.x + side, at.y + side), Opaque(color));
    into->AddRect(at, ImVec2(at.x + side, at.y + side), kSwatchBorder);
    ImGui::PopID();
  }
}

void DrawGallery(GuildTabState& state, const GuildView& view, GuildAction& action) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float cell = Px(kGalleryCellPixels);
  const float inner = ImGui::GetContentRegionAvail().x - style.WindowPadding.x * 2.0F -
                      style.ChildBorderSize * 2.0F;
  const std::size_t per_row = static_cast<std::size_t>(
      std::max(1.0F, (inner + style.ItemSpacing.x) / (cell + style.ItemSpacing.x)));
  const float height = static_cast<float>(kGalleryRows) * cell +
                       static_cast<float>(kGalleryRows - 1) * style.ItemSpacing.y +
                       style.WindowPadding.y * 2.0F + style.ChildBorderSize * 2.0F;
  if (ImGui::BeginChild("##galerie", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    if (view.gallery.empty()) {
      ColoredText(kGuildLabelColor, core::Tr("Aucun item dans l'inventory."));
    }
    ImDrawList* into = ImGui::GetWindowDrawList();
    for (std::size_t index = 0; index < view.gallery.size(); ++index) {
      const EmblemItemView& item = view.gallery[index];
      if (index % per_row != 0) {
        ImGui::SameLine();
      }
      ImGui::PushID(static_cast<int>(item.item_id));
      const ImVec2 at = ImGui::GetCursorScreenPos();
      const ImVec2 end(at.x + cell, at.y + cell);
      const bool clicked = ImGui::InvisibleButton("##icone", ImVec2(cell, cell));
      const bool hovered = ImGui::IsItemHovered();
      if (hovered) {
        into->AddRectFilled(at, end, kGalleryHover);
      }
      if (item.icon.usable()) {
        into->AddImage(item.icon.id, at, end);
      } else {
        into->AddRect(at, end, kGalleryEmpty);
      }
      if (hovered) {
        const std::string name = item.name.empty() ? "?" : std::string(item.name);
        SkinTooltipFormat("%s (%u)", name.c_str(), item.item_id);
      }
      if (clicked) {
        state.emblem.item_id = static_cast<int>(item.item_id);
        action.verb = GuildVerb::kImportItemIcon;
        action.item_id = item.item_id;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
}

// LE POINT DE DÉPART : une icône d'objet, qui fait 24 × 24 — la taille exacte
// d'un emblème, et d'excellentes bases à retoucher.
void DrawItemImport(GuildTabState& state, const GuildView& view, const SkinArt& art,
                    GuildAction& action) {
  ImGui::AlignTextToFramePadding();
  ColoredText(kGuildLabelColor, core::Tr("Partir d'une icône d'item :"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kItemFieldPixels));
  ImGui::InputInt("##objet", &state.emblem.item_id, 0, 0);
  state.emblem.item_id = std::max(state.emblem.item_id, 0);
  // L'aperçu à côté du champ : on voit ce qu'on va importer avant de perdre le
  // dessin.
  ImGui::SameLine();
  const float box = ImGui::GetFrameHeight();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(box, box));
  if (view.typed_item_icon.usable()) {
    ImGui::GetWindowDrawList()->AddImage(view.typed_item_icon.id, at,
                                         ImVec2(at.x + box, at.y + box));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.typed_item_icon.usable());
  if (SkinButton(core::Tr("Importer"), art, ImVec2(Px(kImportPixels), 0.0F))) {
    action.verb = GuildVerb::kImportItemIcon;
    action.item_id = static_cast<std::uint32_t>(state.emblem.item_id);
  }
  ImGui::EndDisabled();
  SkinTooltip(core::Tr("Remplace le dessin par l'icône de l'item (annulable)."));
  if (state.emblem.item_id > 0 && !view.typed_item_icon.usable()) {
    ImGui::SameLine();
    ColoredText(kGuildRedColor, core::Tr("aucune icône"));
  }
  ImGui::SameLine();
  if (SkinToggleButton(core::Tr("Inventory"), state.emblem.gallery_open, art,
                       ImVec2(Px(kGalleryTogglePixels), 0.0F))) {
    state.emblem.gallery_open = !state.emblem.gallery_open;
  }
  SkinTooltip(core::Tr("Choisir l'icône dans son inventory, sans connaître les numéros."));
  if (state.emblem.gallery_open) {
    DrawGallery(state, view, action);
  }
}

// L'ENVOI ET L'ENREGISTREMENT, sous un même nom de fichier.
void DrawSend(GuildTabState& state, const GuildView& view, const SkinArt& art,
              GuildAction& action) {
  std::array<char, 192> line = {};
  std::snprintf(line.data(), line.size(), core::Tr("%zu octets (BMP %dx%d, 24 bits)."),
                view.canvas_bytes, kEmblemCanvasSide, kEmblemCanvasSide);
  ColoredText(kGuildLabelColor, line.data());
  // Le serveur refuse un emblème trop vide : mieux vaut le dire pendant qu'on
  // dessine qu'après un envoi rejeté.
  if (view.canvas_transparency > view.transparency_limit) {
    std::snprintf(line.data(), line.size(),
                  core::Tr("Transparence ~%d %% : au-delà de %d %% le serveur refuse — "
                           "remplis davantage le fond."),
                  view.canvas_transparency, view.transparency_limit);
    ColoredText(kGuildRedColor, line.data());
  }
  const bool has_ink = EmblemCanvasHasInk(state.emblem.canvas);
  const bool may_send = has_ink && view.guild_id > 0;
  // 🔴 UN NOM DE FICHIER, PAS UN LIBELLÉ D'ÉCRAN : sa traduction reste en ASCII
  // pur, parce qu'il s'écrit dans le dossier de l'installation.
  const std::string_view typed = TypedText(state.emblem.save_name);
  const std::string file =
      (typed.empty() ? std::string(core::Tr("mon_embleme")) : std::string(typed)) + ".bmp";
  ImGui::BeginDisabled(!may_send);
  if (SkinButton(core::Tr("Envoyer ce dessin"), art, ImVec2(Px(kSendPixels), 0.0F))) {
    action.verb = GuildVerb::kUploadCanvas;
    action.text = file;
  }
  ImGui::EndDisabled();
  // Un bouton grisé dit POURQUOI.
  if (!may_send) {
    ImGui::SameLine();
    std::snprintf(line.data(), line.size(), core::Tr("Envoi impossible : %s."),
                  has_ink ? core::Tr("guilde inconnue du client") : core::Tr("dessin vide"));
    ColoredText(kGuildRedColor, line.data());
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kSaveNamePixels));
  ImGui::InputTextWithHint("##nom_fichier", core::Tr("mon_embleme"), state.emblem.save_name.data(),
                           state.emblem.save_name.size());
  ImGui::SameLine();
  // Garder le .bmp sous la main : il rejoint le dossier que lit aussi le client.
  if (SkinButton(core::Tr("Enregistrer dans emblem"), art, ImVec2(Px(kSavePixels), 0.0F))) {
    action.verb = GuildVerb::kSaveCanvas;
    action.text = file;
  }
}

}  // namespace

void DrawEmblemPaintTab(GuildTabState& state, const GuildView& view, const SkinArt& art,
                        GuildAction& action) {
  EmblemCanvas& canvas = state.emblem.canvas;
  if (!canvas.started) {
    ClearEmblemCanvas(canvas);
  }
  DrawCanvas(canvas);
  DrawTools(canvas, art);
  DrawColors(canvas, art);

  ImGui::Spacing();
  ImGui::BeginDisabled(canvas.undo.empty());
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(Px(kUndoPixels), 0.0F))) {
    static_cast<void>(UndoEmblemCanvas(canvas));
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Tout effacer"), art, ImVec2(Px(kClearAllPixels), 0.0F))) {
    PushEmblemUndo(canvas);
    ClearEmblemCanvas(canvas);
  }
  ImGui::SameLine();
  // REPARTIR DE L'EMBLÈME EN PLACE sans refermer la boîte.
  ImGui::BeginDisabled(view.guild_id <= 0);
  if (SkinButton(core::Tr("Emblème actuel"), art, ImVec2(Px(kCurrentPixels), 0.0F))) {
    action.verb = GuildVerb::kLoadCurrentEmblem;
  }
  ImGui::EndDisabled();
  SkinTooltip(core::Tr("Repart de l'emblème que porte la guilde (annulable)."));

  DrawItemImport(state, view, art, action);
  DrawSend(state, view, art, action);
}

}  // namespace ro::ui
