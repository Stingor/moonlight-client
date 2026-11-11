// LES INTENTIONS DE L'ÉDITEUR DE STYLE, traduites en gestes de la session et
// en demandes au serveur.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/player_style_layers.h"
#include "ro/net/player_styles.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"

#include "character/style_recipe.h"
#include "character/style_view.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// Un mélange pour la graine de l'inspiration : deux tirages rapprochés dans le
// temps ne doivent pas se ressembler.
constexpr std::uint32_t kSeedMix = 0x9E3779B9U;
// Les bornes de l'ASCII imprimable, pour écrire un chemin en JSON.
constexpr unsigned char kFirstPrintable = 0x20;
constexpr unsigned char kLastAscii = 0x7E;

[[nodiscard]] Harmony ToHarmony(ro::ui::PaletteHarmony harmony) {
  switch (harmony) {
    case ro::ui::PaletteHarmony::kAnalogous:
      return Harmony::kAnalogous;
    case ro::ui::PaletteHarmony::kComplementary:
      return Harmony::kComplementary;
    case ro::ui::PaletteHarmony::kTriadic:
      return Harmony::kTriadic;
  }
  return Harmony::kAnalogous;
}

[[nodiscard]] bool InRange(int rank, std::size_t count) {
  return rank >= 0 && static_cast<std::size_t>(rank) < count;
}

}  // namespace

void StyleView::Apply(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                      const ro::sim::World& world, std::uint32_t now_ms) {
  // 🔴 LES PIÈCES SONT CELLES DE L'ONGLET DONT LA VUE EST FAITE, pas de celui
  // que la barre vient de choisir : c'est sur elles que le joueur a cliqué.
  if (const LayerTab* tab = OpenLayer()) {
    ApplyLayerPieces(asked, *tab, ToHarmony(asked.harmony), now_ms);
    if (asked.clear_item_dye && (tab->shown.item || tab->shown.companion)) {
      ClearItemDye(*tab, link, now_ms);
    }
  } else {
    ApplyPieces(asked, now_ms);
  }
  // La ligne de peau de l'onglet de la vue : le visage sous la Tête, le corps
  // sous le Corps.
  ApplySkinLine(asked);
  switch (asked.chosen_in) {
    case ro::ui::PaletteEditorPicker::kNone:
      break;
    case ro::ui::PaletteEditorPicker::kHairStyle:
      session_.ChooseHairStyle(static_cast<std::int16_t>(asked.chosen));
      break;
    case ro::ui::PaletteEditorPicker::kHairColor:
      session_.ChooseHairPalette(static_cast<std::int16_t>(asked.chosen));
      break;
    case ro::ui::PaletteEditorPicker::kBodyPalette:
      // 🔴 LA TEINTE DE BASE CHANGE LA FUSION, donc le DÉCOUPAGE des rampes : le
      // pantin se remesure à l'image suivante, et les réglages en place se
      // ré-appliquent par rang.
      session_.ChooseBodyPalette(static_cast<std::int16_t>(asked.chosen));
      break;
  }
  // Un autre corps à régler : la session le prendra à l'image suivante.
  if (InRange(asked.edit_body, bodies_.size())) {
    edited_body_ = asked.edit_body;
  }
  ApplyVerb(asked, link, world, now_ms);
  ApplyOutfits(asked, link, world, now_ms);
}

void StyleView::ApplyPieces(const ro::ui::PaletteEditorAction& asked, std::uint32_t now_ms) {
  if (!doll_.has_value()) {
    return;
  }
  const ro::render::ActorWardrobe::BodyRamps& body = doll_->body();
  PieceMarks& marks = Marks();

  // LA COULEUR DÉSIGNÉE, TRADUITE EN RÉGLAGE — pour la pièce, et pour celles
  // qui lui sont liées. 🔴 DEPUIS LA BASE, jamais depuis la palette peinte :
  // partir du résultat courant ferait s'additionner les réglages à chaque
  // clic, et la couleur dériverait sous la main du joueur.
  if (InRange(asked.picked_ramp, body.ramps.size())) {
    std::vector<PieceEdit> edits;
    for (const std::size_t rank : marks.Reached(static_cast<std::size_t>(asked.picked_ramp))) {
      if (rank < body.ramps.size()) {
        edits.push_back(PieceEdit{
            .rank = rank,
            .adjust = ro::render::AdjustToReach(body.base, body.ramps[rank], asked.picked_color)});
      }
    }
    session_.SetPieces(edits, asked.stroke);
  }
  // LE RÉGLAGE DIRECT du mode avancé : le même pour toutes les pièces liées.
  if (InRange(asked.adjusted_ramp, body.ramps.size())) {
    std::vector<PieceEdit> edits;
    for (const std::size_t rank : marks.Reached(static_cast<std::size_t>(asked.adjusted_ramp))) {
      edits.push_back(PieceEdit{.rank = rank, .adjust = asked.adjust});
    }
    session_.SetPieces(edits, asked.stroke);
  }
  // ⚠ REMETTRE UNE PIÈCE À NEUTRE N'EST PAS UN CALCUL : c'est l'absence de
  // réglage.
  // ⚠ AU-DELÀ DES PIÈCES V7, la suite et le reste : voir `ApplyBodyMore`.
  const std::size_t v7 = body.ramps.size();
  if (InRange(asked.reset_ramp, v7)) {
    session_.SetPiece(static_cast<std::size_t>(asked.reset_ramp), ro::render::RampAdjust{});
  }
  if (InRange(asked.link_from, v7) && InRange(asked.link_to, v7)) {
    marks.Link(static_cast<std::size_t>(asked.link_from), static_cast<std::size_t>(asked.link_to));
  }
  if (InRange(asked.unlink, v7)) {
    marks.Unlink(static_cast<std::size_t>(asked.unlink));
  }
  if (InRange(asked.toggle_lock, v7)) {
    marks.ToggleLock(static_cast<std::size_t>(asked.toggle_lock));
  }
  if (asked.verb == ro::ui::PaletteEditorVerb::kInspire && !body.ramps.empty()) {
    std::vector<std::array<std::uint8_t, 3>> origin;
    origin.reserve(body.ramps.size());
    for (const ro::render::PaletteRamp& ramp : body.ramps) {
      origin.push_back(ro::render::RampColor(body.base, ramp));
    }
    ++inspirations_;
    const auto inspired =
        Inspire(origin, marks, ToHarmony(asked.harmony), now_ms ^ (inspirations_ * kSeedMix));
    std::vector<PieceEdit> edits;
    for (std::size_t rank = 0; rank < inspired.size(); ++rank) {
      if (inspired[rank].has_value()) {
        edits.push_back(PieceEdit{
            .rank = rank,
            .adjust = ro::render::AdjustToReach(body.base, body.ramps[rank], *inspired[rank])});
      }
    }
    // ⚠ UN TIRAGE, UN PAS : Ctrl+Z rend la tenue d'avant d'un coup.
    session_.SetPieces(edits);
  }
  ApplyBodyMore(asked, ToHarmony(asked.harmony), now_ms);
}

void StyleView::LoadPreset(const ro::ui::PaletteEditorAction& asked, const ro::sim::World& world) {
  const StylePresets::Named* kept = presets_.FindNamed(asked.preset);
  const ro::sim::Actor* me = world.player();
  if (kept == nullptr || me == nullptr) {
    return;
  }
  const ro::render::PaletteRecipe preset = FromWire(kept->recipe);
  ro::render::PaletteRecipe next = session_.recipe();
  PresetLayers part = PresetLayers::kAll;
  switch (asked.preset_part) {
    case ro::ui::PresetPart::kAll:
      next = preset;
      break;
    case ro::ui::PresetPart::kColors:
      // ⚠ LA TEINTE DE BASE VA AVEC LES PIÈCES : elle décide de leur découpage.
      next.ramps = preset.ramps;
      next.body_palette = preset.body_palette;
      part = PresetLayers::kBody;
      break;
    case ro::ui::PresetPart::kHair:
      next.hair_style = preset.hair_style;
      next.hair_palette = preset.hair_palette;
      part = PresetLayers::kHead;
      break;
  }
  session_.Load(next);

  for (const ro::sim::StyleLayerRecipe& layer :
       RekeyPresetLayers(kept->layers, KeysAfter(next, world), part)) {
    if (const std::optional<ro::render::LayerDye> dye = FromWire(layer)) {
      layers_.Put(*dye);
    }
  }
}

PresetKeys StyleView::KeysAfter(const ro::render::PaletteRecipe& next,
                                const ro::sim::World& world) const {
  // 🔴 LA TÊTE SE RANGE SOUS LA COUPE ET LA COULEUR QU'ON VA MONTRER, celles
  // que le chargement vient de poser ou, à défaut, celles du personnage.
  const ro::sim::Actor* me = world.player();
  if (me == nullptr) {
    return PresetKeys{.body = session_.body_key()};
  }
  const ro::render::ActorLook look = LookOf(*me);
  const auto hair = static_cast<std::uint16_t>(next.hair_style >= 0 ? next.hair_style : look.hair);
  const std::int16_t hair_palette = next.hair_palette >= 0 ? next.hair_palette : look.hair_palette;
  return PresetKeys{.head = ro::render::HeadDyeKey(hair, hair_palette),
                    .body = session_.body_key()};
}

std::vector<ro::sim::StyleLayerRecipe> StyleView::PresetLayersNow() const {
  // CE QUE LE PANTIN PEINT, brouillons compris, pour ce qu'on porte en ce
  // moment : la tête de la coupe montrée, la suite et la peau du corps réglé.
  const LayerTab* head = TabOf(ro::ui::PaletteTabKind::kHair);
  const std::uint32_t head_key = head != nullptr ? head->at.key : 0U;
  const std::uint32_t body_key = session_.body_key();
  std::vector<ro::sim::StyleLayerRecipe> kept;
  for (const ro::render::LayerDye& dye : layers_.Composed()) {
    const ro::sim::StyleLayerRecipe wire = ToWire(dye);
    const bool on_body = wire.layer == ro::sim::StyleLayerKind::kBodyMore ||
                         wire.layer == ro::sim::StyleLayerKind::kBodySkin;
    const bool current =
        wire.layer == ro::sim::StyleLayerKind::kSkin ||
        (wire.layer == ro::sim::StyleLayerKind::kHead && head_key != 0U && wire.key == head_key) ||
        (on_body && body_key != 0U && wire.key == body_key);
    if (current) {
      kept.push_back(wire);
    }
  }
  return kept;
}

void StyleView::Validate(ro::net::Connection& link, const ro::sim::World& world,
                         std::uint32_t now_ms, bool make_main) {
  // ⚠ LA FENÊTRE GRISE DÉJÀ LE BOUTON ; on ne s'y fie pas : une demande dans
  // la cadence serait jetée, et annoncée refusée à tort.
  if (!session_.CanSend(now_ms)) {
    return;
  }
  // ⚠ UNE CLÉ NULLE NE DÉSIGNE AUCUN CORPS : le serveur jetterait la demande.
  const std::uint32_t body_key = session_.body_key();
  const ro::sim::Actor* me = world.player();
  const auto worn_hair = static_cast<std::int16_t>(me != nullptr ? LookOf(*me).hair : 0);
  if (const std::optional<ro::render::PaletteRecipe> sent =
          session_.Validate(worn_hair, now_ms, make_main)) {
    const bool left =
        body_key != 0 &&
        ro::net::SendPlayerStyle(link, ToWire(*sent), body_key, make_main).has_value();
    session_.Sent(make_main ? StyleSendKind::kMakeMain : StyleSendKind::kApply, left, now_ms);
  }
}

bool StyleView::SendLayers(ro::net::Connection& link,
                           std::span<const ro::net::StyleLayerOrder> orders, bool clear_character,
                           std::uint32_t now_ms) {
  // ⚠ UN SERVEUR QUI N'A PAS DIT SA CADENCE ne sait pas lire la trame.
  if (!session_.CanSendLayers(now_ms)) {
    return false;
  }
  const bool left = ro::net::SendStyleLayers(link, orders, clear_character).has_value();
  session_.SentLayers(left, now_ms);
  return left;
}

void StyleView::ApplyVerb(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                          const ro::sim::World& world, std::uint32_t now_ms) {
  const std::uint32_t body_key = session_.body_key();
  switch (asked.verb) {
    case ro::ui::PaletteEditorVerb::kNothing:
    case ro::ui::PaletteEditorVerb::kCopyCode:
    case ro::ui::PaletteEditorVerb::kInspire:
      // Le presse-papiers est déjà rempli ; l'inspiration est une pièce.
      break;
    case ro::ui::PaletteEditorVerb::kPublishOutfit:
    case ro::ui::PaletteEditorVerb::kFetchOutfit:
    case ro::ui::PaletteEditorVerb::kListWardrobe:
    case ro::ui::PaletteEditorVerb::kWearOutfit:
    case ro::ui::PaletteEditorVerb::kStoreOutfit:
    case ro::ui::PaletteEditorVerb::kClearOutfit:
      // Les tenues ont leur propre guichet : voir `ApplyOutfits`.
      break;
    case ro::ui::PaletteEditorVerb::kApply:
      // ⛔ VALIDER NE FAIT PAS LE STYLE PRINCIPAL : le serveur garde pour
      // repli la première variante rangée. En changer est un geste à part.
      //
      // 🔴 LE CORPS ET LES CALQUES DU MÊME CLIC, chacun sur sa cadence et
      // jugé par son verdict.
      if (session_.HasSomethingToSend()) {
        Validate(link, world, now_ms, /*make_main=*/false);
      }
      ValidateLayers(link, now_ms);
      break;
    case ro::ui::PaletteEditorVerb::kMakeMain:
      Validate(link, world, now_ms, /*make_main=*/true);
      break;
    case ro::ui::PaletteEditorVerb::kForgetBody: {
      const bool left = body_key != 0 && ro::net::SendPlayerStyleClear(link, body_key).has_value();
      session_.Sent(StyleSendKind::kForgetBody, left, now_ms);
      break;
    }
    case ro::ui::PaletteEditorVerb::kDeleteAll:
      session_.Sent(StyleSendKind::kDeleteAll, ro::net::SendPlayerStyleClearAll(link).has_value(),
                    now_ms);
      // D17 : la tête et la peau partent avec le corps. Les teintures d'objets
      // appartiennent aux objets, et ne se retirent qu'une à une.
      static_cast<void>(SendLayers(link, {}, /*clear_character=*/true, now_ms));
      for (const LayerTab& tab : tabs_) {
        if (!tab.shown.item) {
          layers_.BackToWorn(tab.at);
        }
      }
      layers_.BackToWorn(SkinAt(SkinLine::kFace));
      layers_.BackToWorn(SkinAt(SkinLine::kBody));
      break;
    // ⚠ CES GESTES-LÀ VALENT POUR L'ONGLET OUVERT : le corps a son
    // historique, les calques le leur.
    case ro::ui::PaletteEditorVerb::kBackToWorn:
      if (const LayerTab* tab = OpenLayer()) {
        layers_.BackToWorn(tab->at);
      } else {
        session_.BackToWorn();
      }
      // La peau de l'onglet aussi : celle du visage sous la Tête, celle du corps
      // sous le Corps.
      if (const std::optional<SkinLine> line = OpenSkinLine()) {
        layers_.BackToWorn(SkinAt(*line));
      }
      break;
    case ro::ui::PaletteEditorVerb::kBlankSlate:
      if (const LayerTab* tab = OpenLayer()) {
        layers_.Blank(tab->at, tab->profile);
      } else {
        session_.BlankSlate();
        // ⚠ LE CORPS MONTRE AUSSI SA SUITE ET SON RESTE : ils repartent de zéro
        // avec ses pièces.
        if (body_more_.at.key != 0) {
          layers_.Blank(body_more_.at, body_more_.profile);
        }
      }
      break;
    case ro::ui::PaletteEditorVerb::kUndo:
      static_cast<void>(OpenLayer() != nullptr ? layers_.Undo() : session_.Undo());
      break;
    case ro::ui::PaletteEditorVerb::kRedo:
      static_cast<void>(OpenLayer() != nullptr ? layers_.Redo() : session_.Redo());
      break;

    // ── LES PRÉRÉGLAGES ET LES CODES ────────────────────────────────────
    //
    // ⚠ AUCUN NE TOUCHE AU PERSONNAGE : charger un préréglage, coller un code,
    // reprendre un brouillon ne font que poser un APERÇU. Il faudra valider —
    // un code vient d'un autre joueur, et il ne doit pas pouvoir changer une
    // apparence d'un clic.
    case ro::ui::PaletteEditorVerb::kLoadDraft:
      static_cast<void>(session_.LoadDraft());
      break;
    case ro::ui::PaletteEditorVerb::kSavePreset:
      // 🔴 LA RECETTE PARTAGEABLE : un préréglage se recharge des semaines
      // plus tard, éventuellement sur un autre personnage.
      if (const ro::sim::Actor* me = world.player();
          me != nullptr &&
          presets_.Save(asked.preset, ToWire(Shareable(session_.recipe(), LookOf(*me))),
                        PresetLayersNow())) {
        static_cast<void>(SaveStylePresets(client_root_, presets_));
      }
      break;
    case ro::ui::PaletteEditorVerb::kLoadPreset:
      LoadPreset(asked, world);
      break;
    case ro::ui::PaletteEditorVerb::kRenamePreset:
      if (presets_.Rename(asked.preset, asked.renamed)) {
        static_cast<void>(SaveStylePresets(client_root_, presets_));
      }
      break;
    case ro::ui::PaletteEditorVerb::kDeletePreset:
      presets_.Erase(asked.preset);
      static_cast<void>(SaveStylePresets(client_root_, presets_));
      break;
    case ro::ui::PaletteEditorVerb::kSharePreset:
      // 🔴 LE CODE DU PRÉRÉGLAGE, sous son nom : le joueur désigne une ligne,
      // et c'est elle qu'il partage — même s'il a réglé autre chose depuis.
      if (const ro::sim::StyleRecipe* kept = presets_.Find(asked.preset)) {
        to_share_ = StyleToShare{.code = ro::sim::EncodeStyleCode(*kept), .label = asked.preset};
      }
      break;
    case ro::ui::PaletteEditorVerb::kShareCode:
      // Étiquette vide : c'est notre style, et `Game` y mettra notre pseudo.
      to_share_ = StyleToShare{.code = share_code_};
      break;
    case ro::ui::PaletteEditorVerb::kPasteCode:
      static_cast<void>(TryCode(asked.pasted, now_ms));
      break;
  }
}

bool StyleView::TryCode(std::string_view code, std::uint32_t now_ms) {
  // UN CODE DE CALQUE se pose sur l'onglet ouvert, s'il en est un du même
  // calque : le code d'un chapeau va au chapeau qu'on porte.
  if (TryLayerCode(code)) {
    refused_ms_ = 0;
    return true;
  }
  const std::optional<ro::sim::StyleCode> read = ro::sim::DecodeStyleCode(code);
  if (!read.has_value()) {
    // ⚠ ET ON LE DIT. ⚠ JAMAIS ZÉRO, qui est la sentinelle.
    refused_ms_ = now_ms != 0 ? now_ms : 1;
    return false;
  }
  session_.Load(FromWire(read->recipe));
  refused_ms_ = 0;
  return true;
}

std::string StyleView::ReportCode() const {
  return ro::sim::EncodeStyleCode(ToWire(session_.recipe()));
}

std::string StyleView::ReportFields() const {
  std::string fields;
  char line[96] = {};
  std::snprintf(line, sizeof(line), R"("body_key":"%08X")",
                static_cast<unsigned>(session_.body_key()));
  fields += line;
  if (!bodies_.empty() && wardrobe_ != nullptr) {
    const BodyChoice& edited = bodies_[static_cast<std::size_t>(
        std::clamp(edited_body_, 0, static_cast<int>(bodies_.size()) - 1))];
    const std::vector<std::string> paths =
        ro::render::BodySpritePaths(wardrobe_->catalog(), edited.look);
    // ⚠ LE CHEMIN EST EN CORÉEN, dans l'encodage de l'archive et non en UTF-8 :
    // chaque octet hors de l'ASCII s'écrit `\u00XX`, ce qui garde le JSON
    // valide et rend les octets à qui les relit.
    fields += R"(,"body_sprite":")";
    for (const char letter : paths.empty() ? std::string() : paths.front()) {
      const auto byte = static_cast<unsigned char>(letter);
      if (letter == '"' || letter == '\\') {
        fields += '\\';
        fields += letter;
      } else if (byte < kFirstPrintable || byte > kLastAscii) {
        std::snprintf(line, sizeof(line), "\\u%04X", static_cast<unsigned>(byte));
        fields += line;
      } else {
        fields += letter;
      }
    }
    fields += '"';
  }
  if (doll_.has_value()) {
    const ro::render::ActorWardrobe::BodyRamps& body = doll_->body();
    fields += R"(,"ramps":")";
    for (std::size_t rank = 0; rank < body.ramps.size(); ++rank) {
      std::snprintf(line, sizeof(line), "%s%u+%u", rank == 0 ? "" : " ",
                    static_cast<unsigned>(body.ramps[rank].start),
                    static_cast<unsigned>(body.ramps[rank].length));
      fields += line;
    }
    std::snprintf(line, sizeof(line), R"(","pixels_covered":%d,"pixels_total":%d)",
                  body.pixels_covered, body.pixels_total);
    fields += line;
  }
  return fields;
}

}  // namespace moonclient
