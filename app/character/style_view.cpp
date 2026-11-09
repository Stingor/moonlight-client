#include "character/style_view.h"

#include <array>
#include <cstddef>
#include <utility>

#include "ro/net/player_styles.h"
#include "ro/render/palette_ramps.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"
#include "ro/ui/character_window.h"

#include "character/character_styles.h"
#include "character/style_recipe.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

constexpr int kFullValue = 255;
constexpr int kPercent = 100;
// COMBIEN DE VIGNETTES NEUVES PAR IMAGE. 🔴 UN BUDGET, PAS UNE LIMITE DE
// TAILLE : une grille ouverte demande jusqu'à cent soixante cases, et composer
// les cent soixante dans la même image se verrait — chacune ouvre un sprite,
// le compose et téléverse une texture. Douze les étale sur une dizaine
// d'images, ce qui est plus court que le temps de parcourir la grille du
// regard.
constexpr int kThumbnailBudget = 12;
// Un collage refusé se dit quelques secondes : un collage raté qui ne dit rien
// laisse croire à un défaut de l'éditeur.
constexpr std::uint32_t kCodeRefusedNoticeMs = 4000;

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], kFullValue);
}

// LA RECETTE TELLE QU'ELLE DOIT VOYAGER SANS SON PORTEUR : code copié,
// préréglage rangé.
//
// 🔴 CE QUE LA RECETTE LAISSE OUVERT CHANGE DE SENS EN QUITTANT SON
// PERSONNAGE. Sur un joueur EN VUE, moins un est exactement juste : son acteur
// porte déjà la couleur que le serveur lui a donnée, et ne rien imposer la
// laisse paraître. Dans un CODE, le même moins un devient « la tienne » chez
// celui qui le colle : le style arrive dans les cheveux du destinataire, qui se
// croit copié à tort. Un code voyage sans son porteur, il doit donc être
// COMPLET.
//
// ⛔ LA TEINTE DE BASE N'EST PAS COMBLÉE, elle. C'est la couleur de vêtement
// que le serveur assigne au destinataire : la lui imposer changerait la BASE
// sur laquelle ses rampes se détectent, donc le découpage de ses pièces.
[[nodiscard]] ro::render::PaletteRecipe Shareable(const ro::render::PaletteRecipe& recipe,
                                                  const ro::render::ActorLook& worn) {
  ro::render::PaletteRecipe out = recipe;
  if (out.hair_palette <= 0) {
    out.hair_palette = worn.hair_palette;
  }
  if (out.hair_style <= 0) {
    out.hair_style = static_cast<std::int16_t>(worn.hair);
  }
  return out;
}

// Ce que la partie dit de nous, pour la session.
[[nodiscard]] StyleSession::Worn WornOf(const ro::sim::World& world, std::uint32_t body_key) {
  return StyleSession::Worn{.body_key = body_key,
                            .mine = world.styles().Find(world.player_id()),
                            .book_revision = world.styles().RevisionOf(world.player_id())};
}

[[nodiscard]] ro::ui::PaletteSendStatus ToView(StyleSendState state) {
  switch (state) {
    case StyleSendState::kNone:
      return ro::ui::PaletteSendStatus::kNone;
    case StyleSendState::kWaiting:
      return ro::ui::PaletteSendStatus::kWaiting;
    case StyleSendState::kShared:
      return ro::ui::PaletteSendStatus::kShared;
    case StyleSendState::kRefused:
      return ro::ui::PaletteSendStatus::kRefused;
  }
  return ro::ui::PaletteSendStatus::kNone;
}

}  // namespace

StyleView::StyleView(ro::render::ActorWardrobe* wardrobe, std::filesystem::path client_root)
    : wardrobe_(wardrobe), client_root_(std::move(client_root)) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
    thumbnails_.emplace(*wardrobe_);
  }
  LoadStylePresets(client_root_, presets_);
  StyleDrafts drafts;
  LoadStyleDrafts(client_root_, drafts);
  session_ = StyleSession(std::move(drafts));
}

void StyleView::RefreshPieces(const ro::render::ActorWardrobe::BodyRamps& body,
                              const ro::render::ActorLook& worn) {
  const PiecesKey key{.revision = session_.revision(),
                      .generation = doll_->generation(),
                      .worn_hair = worn.hair,
                      .worn_hair_palette = worn.hair_palette};
  if (key == pieces_key_) {
    return;
  }
  pieces_key_ = key;
  colors_.clear();
  ceilings_.clear();
  if (!body.ramps.empty()) {
    const ro::formats::Palette painted =
        ro::render::ApplyRecipe(body.base, body.ramps, session_.recipe());
    for (const ro::render::PaletteRamp& ramp : body.ramps) {
      // La pastille se lit sur la palette PEINTE : ce que la fenêtre montre
      // est ce que le joueur verra sur lui.
      colors_.push_back(ToColor(ro::render::RampColor(painted, ramp)));
      // Le plafond, lui, se lit sur la BASE : c'est le sprite qui le fixe, et
      // aucun réglage ne le déplace.
      ceilings_.push_back(ro::render::RampValueCeiling(body.base, ramp));
    }
  }
  // 🔴 LE CODE EST CELUI QUI VOYAGE : il comble ce que la recette laisse
  // ouvert, parce qu'un code quitte son porteur — voir `Shareable`.
  share_code_ = ro::sim::EncodeStyleCode(ToWire(Shareable(session_.recipe(), worn)));
}

void StyleView::RefreshDoll(const ro::render::ActorLook& look, ro::ui::PaletteEditorState& state,
                            std::uint32_t now_ms) {
  state.doll.image = ro::ui::SkinImage{};
  const StyleDoll::Shot shot{
      .look = look,
      .recipe = &session_.recipe(),
      .revision = session_.revision(),
      .highlight = state.hovered_ramp,
      .now_ms = now_ms,
      .direction = state.doll.direction,
      .box = ro::render::VignetteBox{
          .width = static_cast<std::int32_t>(state.doll.box_width),
          .height = static_cast<std::int32_t>(state.doll.box_height),
          .min_body_height = state.doll.box_height * ro::ui::kCharacterAvatarBodyFloor}};
  const ro::render::IconTexture* drawn = doll_->Draw(shot);
  if (drawn == nullptr) {
    return;
  }
  state.doll.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn->texture.name()),
                                       .width = state.doll.box_width,
                                       .height = state.doll.box_height};
}

void StyleView::RefreshThumbnails(const ro::render::ActorLook& look,
                                  const ro::ui::PaletteEditorState& state) {
  thumbs_.clear();
  view_.thumbnails = {};
  view_.thumbnails_first = 0;
  const ro::ui::PaletteEditorPicker open = state.picker.open;
  if (open != shown_picker_) {
    // ⚠ LES TEXTURES DE LA GRILLE PRÉCÉDENTE SE RENDENT ICI, au changement, et
    // pas à chaque image : trois grilles gardées ensemble tiendraient huit
    // cent quatre-vingt-quatre images, une grille refermée n'en vaut aucune.
    if (thumbnails_.has_value()) {
      thumbnails_->Clear();
    }
    shown_picker_ = open;
  }
  if (!thumbnails_.has_value() || open == ro::ui::PaletteEditorPicker::kNone ||
      state.picker.count <= 0 || state.picker.cell <= 0.0F) {
    return;
  }

  thumbnails_->BeginFrame(kThumbnailBudget);
  const auto side = static_cast<std::int32_t>(state.picker.cell);
  ro::render::ActorThumbnails::Cell cell;
  cell.box.width = side;
  cell.box.height = side;
  cell.subject = open == ro::ui::PaletteEditorPicker::kBodyPalette
                     ? ro::render::ActorThumbnails::Subject::kBody
                     : ro::render::ActorThumbnails::Subject::kHead;
  thumbs_.reserve(static_cast<std::size_t>(state.picker.count));
  for (int rank = 0; rank < state.picker.count; ++rank) {
    const int value = state.picker.first + rank;
    cell.look = look;
    switch (open) {
      case ro::ui::PaletteEditorPicker::kHairStyle:
        cell.look.hair = static_cast<std::uint16_t>(value);
        break;
      case ro::ui::PaletteEditorPicker::kHairColor:
        // ⚠ CHAQUE CASE PORTE LA COUPE CHOISIE : on compare des teintes sur la
        // coiffure qu'on portera, pas sur une qu'on vient d'abandonner.
        cell.look.hair_palette = static_cast<std::int16_t>(value);
        break;
      case ro::ui::PaletteEditorPicker::kBodyPalette:
        cell.palette = static_cast<std::int16_t>(value);
        break;
      case ro::ui::PaletteEditorPicker::kNone:
        break;
    }
    const ro::render::IconTexture& drawn = thumbnails_->Get(cell);
    thumbs_.push_back(drawn.usable()
                          ? ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                              .width = state.picker.cell,
                                              .height = state.picker.cell}
                          : ro::ui::SkinImage{});
  }
  view_.thumbnails = thumbs_;
  view_.thumbnails_first = state.picker.first;
}

void StyleView::KeepForSelection(const ro::sim::World& world, std::uint32_t character_id) {
  const std::uint32_t revision = world.styles().RevisionOf(world.player_id());
  if (revision == 0 || revision == kept_revision_ || character_id == 0) {
    return;
  }
  kept_revision_ = revision;
  static_cast<void>(
      NoteCharacterStyles(client_root_, character_id, world.styles().Find(world.player_id())));
}

void StyleView::Refresh(const ro::sim::World& world, ro::ui::PaletteEditorState& state,
                        bool connected, std::uint32_t now_ms) {
  view_ = ro::ui::PaletteEditorView{};
  view_.connected = connected;
  const ro::sim::Actor* player = world.player();
  if (!doll_.has_value() || player == nullptr) {
    return;
  }
  shown_ = true;
  view_.bounds = ro::ui::PaletteChoiceBounds{.first = ro::sim::kStyleFirstChoice,
                                             .hair_style = ro::sim::kHairStyleMax,
                                             .hair_palette = ro::sim::kHairPaletteMax,
                                             .body_palette = ro::sim::kBodyPaletteMax};
  const ro::render::ActorLook worn = LookOf(*player);
  view_.worn_hair_style = worn.hair;

  // 🔴 LA SESSION D'ABORD, LA MESURE ENSUITE. La recette qu'on retrouve porte
  // sa propre teinte de base, donc une autre fusion et un autre découpage :
  // mesurer d'abord donnerait une fenêtre où les pastilles annoncent la bonne
  // palette pendant que le pantin porte encore les couleurs nues du sprite.
  //
  // ⚠ LA CLÉ SE PREND SUR L'APPARENCE NUE : c'est le corps qui décide de la
  // variante, et la variante ne doit pas décider du corps. Le pantin ne porte
  // que son corps : on en règle la palette.
  ro::render::ActorLook look = BodyOnlyLookOf(*player);
  session_.Observe(WornOf(world, wardrobe_->BodyKeyFor(look)), now_ms);
  Impose(session_.recipe(), look);
  const ro::render::ActorWardrobe::BodyRamps& body = doll_->Measure(look, session_.body_key());
  RefreshPieces(body, worn);

  view_.recipe = session_.recipe();
  view_.ramps = body.ramps;
  view_.colors = colors_;
  view_.ceilings = ceilings_;
  if (body.pixels_total > 0) {
    view_.coverage_percent = static_cast<int>(
        (static_cast<long long>(body.pixels_covered) * kPercent + body.pixels_total / 2) /
        body.pixels_total);
  }
  view_.body_style_count = session_.style_count();
  view_.body_has_own_style = session_.own_style();
  view_.has_edits = session_.has_edits();
  view_.send = ToView(session_.send_state());

  // ── CE QUI EST RANGÉ ─────────────────────────────────────────────────────
  preset_names_.clear();
  preset_names_.reserve(presets_.all().size());
  for (const StylePresets::Named& kept : presets_.all()) {
    preset_names_.push_back(kept.name);
  }
  view_.presets = preset_names_;
  view_.presets_full = presets_.full();
  // ⚠ LE BROUILLON NE SE PROPOSE QUE S'IL DIFFÈRE DE CE QU'ON VOIT : rouvrir
  // sur ce qu'on a déjà sous les yeux serait un bouton qui ment.
  view_.has_draft = session_.OffersDraft();
  view_.share_code = share_code_.c_str();
  view_.code_refused = refused_ms_ != 0 && now_ms - refused_ms_ < kCodeRefusedNoticeMs;

  RefreshDoll(look, state, now_ms);
  // ⚠ LA PIPETTE APRÈS LE PANTIN, sur le point relevé à l'image précédente :
  // c'est lui qui vient de poser le cadrage dont elle est l'inverse. `ui` ne
  // peut pas répondre elle-même — il lui faudrait le sprite et ses index par
  // pixel.
  if (state.doll.hovered) {
    if (const std::optional<std::size_t> rank =
            doll_->RampUnder(glm::vec2(state.doll.hover_x, state.doll.hover_y))) {
      view_.pipette_ramp = static_cast<int>(*rank);
    }
  }
  // ⚠ LES VIGNETTES EN DERNIER : elles se composent sur l'apparence du pantin,
  // teinte et coupe choisies comprises, et la grille des couleurs de cheveux
  // doit montrer la COUPE qu'on vient d'essayer.
  RefreshThumbnails(look, state);
}

void StyleView::Tick(const ro::sim::World& world, std::uint32_t now_ms) {
  // ⚠ LE CORPS GARDÉ : fenêtre fermée, on ne le remesure pas. Un changement de
  // corps se constate à la prochaine ouverture.
  session_.Observe(WornOf(world, session_.body_key()), now_ms);
  session_.TickDraft(now_ms);
  if (session_.TakeDraftsChanged()) {
    static_cast<void>(SaveStyleDrafts(client_root_, session_.drafts()));
  }
}

void StyleView::Apply(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                      const ro::sim::World& world, std::uint32_t now_ms) {
  // LA COULEUR DÉSIGNÉE, TRADUITE EN RÉGLAGE. 🔴 DEPUIS LA BASE, jamais depuis
  // la palette peinte : partir du résultat courant ferait s'additionner les
  // réglages à chaque clic, et la couleur dériverait sous la main du joueur.
  if (doll_.has_value()) {
    const ro::render::ActorWardrobe::BodyRamps& body = doll_->body();
    if (asked.picked_ramp >= 0 && asked.picked_ramp < static_cast<int>(body.ramps.size())) {
      const auto rank = static_cast<std::size_t>(asked.picked_ramp);
      session_.SetPiece(rank,
                        ro::render::AdjustToReach(body.base, body.ramps[rank], asked.picked_color));
    }
  }
  // ⚠ REMETTRE UNE PIÈCE À NEUTRE N'EST PAS UN CALCUL : c'est l'absence de
  // réglage.
  if (asked.reset_ramp >= 0) {
    session_.SetPiece(static_cast<std::size_t>(asked.reset_ramp), ro::render::RampAdjust{});
  }
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
  ApplyVerb(asked, link, world, now_ms);
}

void StyleView::ApplyVerb(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                          const ro::sim::World& world, std::uint32_t now_ms) {
  // ⚠ UNE CLÉ NULLE NE DÉSIGNE AUCUN CORPS : le serveur jetterait la demande.
  // On ne l'envoie pas, et l'éditeur dit qu'elle n'est pas retenue.
  const std::uint32_t body_key = session_.body_key();
  switch (asked.verb) {
    case ro::ui::PaletteEditorVerb::kNothing:
    case ro::ui::PaletteEditorVerb::kCopyCode:
      // Le presse-papiers est déjà rempli : la fenêtre seule parle à ImGui.
      break;
    case ro::ui::PaletteEditorVerb::kApply: {
      const ro::sim::Actor* me = world.player();
      const auto worn_hair = static_cast<std::int16_t>(me != nullptr ? LookOf(*me).hair : 0);
      if (const std::optional<ro::render::PaletteRecipe> sent =
              session_.Validate(worn_hair, now_ms)) {
        // ⛔ VALIDER NE FAIT PAS LE STYLE PRINCIPAL : le serveur garde pour
        // repli la première variante rangée. En changer est un geste à part.
        const bool left = body_key != 0 && ro::net::SendPlayerStyle(link, ToWire(*sent), body_key,
                                                                    /*make_main=*/false)
                                               .has_value();
        session_.Sent(StyleSendKind::kApply, left, now_ms);
      }
      break;
    }
    case ro::ui::PaletteEditorVerb::kForgetBody: {
      const bool left = body_key != 0 && ro::net::SendPlayerStyleClear(link, body_key).has_value();
      session_.Sent(StyleSendKind::kForgetBody, left, now_ms);
      break;
    }
    case ro::ui::PaletteEditorVerb::kDeleteAll:
      session_.Sent(StyleSendKind::kDeleteAll, ro::net::SendPlayerStyleClearAll(link).has_value(),
                    now_ms);
      break;
    case ro::ui::PaletteEditorVerb::kBackToWorn:
      session_.BackToWorn();
      break;
    case ro::ui::PaletteEditorVerb::kBlankSlate:
      session_.BlankSlate();
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
      // plus tard, éventuellement sur un autre personnage. Y laisser
      // « couleur d'origine » lui ferait prendre celle du moment.
      if (const ro::sim::Actor* me = world.player();
          me != nullptr &&
          presets_.Save(asked.preset, ToWire(Shareable(session_.recipe(), LookOf(*me))))) {
        static_cast<void>(SaveStylePresets(client_root_, presets_));
      }
      break;
    case ro::ui::PaletteEditorVerb::kLoadPreset:
      if (const ro::sim::StyleRecipe* kept = presets_.Find(asked.preset)) {
        session_.Load(FromWire(*kept));
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

StyleView::StyleToShare StyleView::TakeStyleToShare() {
  return std::exchange(to_share_, StyleToShare{});
}

std::string StyleView::ReportCode() const {
  return ro::sim::EncodeStyleCode(ToWire(session_.recipe()));
}

void StyleView::Close() {
  if (!shown_) {
    return;
  }
  shown_ = false;
  if (doll_.has_value()) {
    doll_->Close();
  }
  // ⚠ LES VIGNETTES AUSSI : une grille refermée ne vaut aucune texture.
  if (thumbnails_.has_value()) {
    thumbnails_->Clear();
  }
  thumbs_.clear();
  shown_picker_ = ro::ui::PaletteEditorPicker::kNone;
  // ⛔ LA SESSION NE SE REMET PAS À ZÉRO : refermer la fenêtre ne rend pas ses
  // couleurs au joueur. Il rouvre et retrouve ce qu'il composait.
}

}  // namespace moonclient
