#include "character/style_view.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "ro/net/player_styles.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/ground_skill_log.h"
#include "ro/sim/inventory.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"

#include "character/character_styles.h"
#include "character/style_recipe.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// ── LA PULSATION DE LA PIÈCE SURVOLÉE ───────────────────────────────────────
//
// 🔴 ON PULSE LA LUMINOSITÉ, PAS LA TRANSPARENCE. Un sprite RO n'a qu'un index
// transparent — le zéro —, et toucher l'alpha des autres les ferait
// disparaître. Un éclaircissement se lit de toute façon mieux sur un fond
// chargé.
//
// 🔴🔴 ET LE NIVEAU EST QUANTIFIÉ, ce qui commande tout le reste. L'aperçu
// garde une texture par palette : une pulsation continue en fabriquerait une
// par image, indéfiniment. Quatre paliers suffisent à l'œil et bornent le coût
// à quatre palettes par pièce.
constexpr int kPulseSteps = 4;
constexpr int kPulseFloor = 60;
constexpr int kPulseStride = 55;
// Deux allers-retours par seconde.
constexpr float kPulsePerSecond = 2.0F;
constexpr float kMillisecondsPerSecond = 1000.0F;
constexpr float kPi = 3.14159265F;

constexpr int kFullValue = 255;
constexpr int kPercent = 100;
// COMBIEN DE VIGNETTES NEUVES PAR IMAGE. 🔴 UN BUDGET, PAS UNE LIMITE DE
// TAILLE : une grille ouverte demande jusqu'à cent soixante cases, et composer
// les cent soixante dans la même image se verrait — chacune ouvre un sprite,
// le compose et téléverse une texture. Douze les étale sur une dizaine
// d'images, ce qui est plus court que le temps de parcourir la grille du
// regard.
constexpr int kThumbnailBudget = 12;
// L'image que le pantin TIENT : la première, comme le cadrage la dessine. ⚠ LA
// MÊME QUE CELLE DU DESSIN, sinon la pipette interrogerait une pose que
// personne ne regarde.
constexpr std::size_t kHeldFrame = 0;
// L'accusé local « Envoyé. » reste quelques secondes. ⚠ IL NE DIT PAS QUE LE
// SERVEUR A ACCEPTÉ — il n'accuse rien — seulement que la demande est partie.
constexpr std::uint32_t kSentNoticeMs = 4000;
// Un collage refusé se dit aussi quelques secondes : un collage raté qui ne
// dit rien laisse croire à un défaut de l'éditeur.
constexpr std::uint32_t kRefusedNoticeMs = 4000;
// LE DÉLAI D'IMMOBILITÉ AVANT D'ÉCRIRE LE BROUILLON. ⚠ ASSEZ LONG POUR QU'UN
// GLISSEMENT DANS LE NUANCIER NE PRODUISE QU'UNE ÉCRITURE, assez court pour
// que ce qui se perd tienne dans une seconde et demie de réglages.
constexpr std::uint32_t kDraftDelayMs = 1500;

[[nodiscard]] int PulseStep(std::uint32_t now_ms) {
  const float seconds = static_cast<float>(now_ms) / kMillisecondsPerSecond * kPulsePerSecond;
  // Une onde douce de zéro à un, puis quatre paliers.
  const float wave = 0.5F - 0.5F * std::cos(seconds * kPi);
  return static_cast<int>(wave * static_cast<float>(kPulseSteps - 1) + 0.5F);
}

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], 255);
}

// LA RECETTE TELLE QU'ELLE DOIT VOYAGER SANS SON PORTEUR : code copié,
// préréglage rangé, brouillon écrit.
//
// 🔴 CE QUE LA RECETTE LAISSE OUVERT CHANGE DE SENS EN QUITTANT SON
// PERSONNAGE. Sur un joueur EN VUE, moins un est exactement juste : son acteur
// porte déjà la couleur que le serveur lui a donnée, et ne rien imposer la
// laisse paraître — mieux, un passage chez le styliste se voit alors tout
// seul. Dans un CODE, le même moins un devient « la tienne » chez celui qui le
// colle : le style arrive dans les cheveux du destinataire, qui se croit copié
// à tort. Un code voyage sans son porteur, il doit donc être COMPLET.
//
// ⛔ ET SURTOUT PAS À L'AMORÇAGE. Combler là transformerait « rien d'imposé »
// en CHOIX EXPLICITE, et ça se voit : la fenêtre annoncerait « Couleur de
// cheveux (22) » là où le joueur n'a rien demandé, sans que rien ne change ni
// sur lui ni sur le pantin.
//
// ⛔ LA TEINTE DE BASE N'EST PAS COMBLÉE, elle. C'est la couleur de vêtement
// que le serveur assigne au destinataire : la lui imposer changerait la BASE
// sur laquelle ses rampes se détectent, donc le découpage de ses pièces — le
// style arriverait sur d'autres zones que celles qu'il montre.
[[nodiscard]] ro::render::PaletteRecipe Shareable(const ro::render::PaletteRecipe& recipe,
                                                  const ro::sim::Actor& player) {
  ro::render::PaletteRecipe out = recipe;
  const ro::render::ActorLook worn = LookOf(player);
  if (out.hair_palette <= 0) {
    out.hair_palette = worn.hair_palette;
  }
  if (out.hair_style <= 0) {
    out.hair_style = static_cast<std::int16_t>(worn.hair);
  }
  return out;
}

// Ce que le personnage PORTE sur ce corps, ou rien.
[[nodiscard]] const ro::sim::StyleRecipe* WornRecipe(const ro::sim::World& world,
                                                     std::uint32_t body_key) {
  const ro::sim::StyleVariants* mine = world.styles().Find(world.player_id());
  return mine != nullptr && body_key != 0 ? mine->For(body_key) : nullptr;
}

}  // namespace

StyleView::StyleView(ro::render::ActorWardrobe* wardrobe, std::filesystem::path client_root)
    : wardrobe_(wardrobe), client_root_(std::move(client_root)) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
    thumbnails_.emplace(*wardrobe_);
  }
  LoadStylePresets(client_root_, presets_);
  LoadStyleDrafts(client_root_, drafts_);
}

void StyleView::ReloadRamps(const ro::render::ActorLook& look) {
  if (wardrobe_ == nullptr) {
    return;
  }
  body_ = wardrobe_->BodyRampsFor(look);
  body_key_ = wardrobe_->BodyKeyFor(look);
  measured_palette_ = look.body_palette;
  measured_valid_ = true;
  // ⚠ L'APERÇU EST PÉRIMÉ AVEC LES RAMPES : sa palette a été peinte sur
  // l'ancienne base, et la garder ferait porter au pantin des couleurs
  // calculées sur un découpage qui n'existe plus.
  shown_valid_ = false;
}

void StyleView::RefreshDoll(const ro::render::ActorLook& look, ro::ui::PaletteEditorState& state,
                            std::uint32_t now_ms) {
  state.doll.image = ro::ui::SkinImage{};
  if (!doll_.has_value() || body_.ramps.empty()) {
    return;
  }
  const auto width = static_cast<std::int32_t>(state.doll.box_width);
  const auto height = static_cast<std::int32_t>(state.doll.box_height);
  if (width <= 0 || height <= 0) {
    return;
  }

  const int highlight = state.hovered_ramp;
  const int step = highlight >= 0 ? PulseStep(now_ms) : 0;
  // ⚠ ON NE REFAIT LA PALETTE QUE QUAND ELLE CHANGE : la recette, la pièce
  // survolée et le palier en décident, et rien d'autre.
  if (!shown_valid_ || !SameRecipe(shown_recipe_, state.recipe) || highlight != shown_highlight_ ||
      step != shown_step_) {
    ro::formats::Palette painted = ro::render::ApplyRecipe(body_.base, body_.ramps, state.recipe);
    if (highlight >= 0 && highlight < static_cast<int>(body_.ramps.size())) {
      const ro::render::PaletteRamp& ramp = body_.ramps[static_cast<std::size_t>(highlight)];
      const int force = kPulseFloor + step * kPulseStride;
      const std::size_t end = std::min<std::size_t>(
          static_cast<std::size_t>(ramp.start) + ramp.length, ro::formats::kPaletteEntryCount);
      for (std::size_t index = ramp.start; index < end; ++index) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
          const int lit = painted.entries[index][channel] + force;
          painted.entries[index][channel] = static_cast<std::uint8_t>(std::min(lit, kFullValue));
        }
      }
    }
    // 🔴 UN IDENTIFIANT NEUF, ET L'ANCIEN RENDU : la garde-robe refuse de
    // réécrire une palette en place, parce qu'une tenue déjà composée en
    // retient l'ADRESSE — la repeindre ne se verrait qu'à la prochaine
    // éviction.
    if (preview_style_ != 0) {
      wardrobe_->ForgetStyle(preview_style_);
    }
    preview_style_ = wardrobe_->NewStyleId();
    if (!wardrobe_->SetStylePalette(preview_style_, std::move(painted))) {
      preview_style_ = 0;
      return;
    }
    shown_recipe_ = state.recipe;
    shown_highlight_ = highlight;
    shown_step_ = step;
    shown_valid_ = true;
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = look;
  pose.look.style = preview_style_;
  // Debout, figé : un aperçu de couleur n'a pas à s'agiter.
  pose.animation = ro::render::kAnimationIdle;
  pose.direction = state.doll.direction;
  pose.animated = false;
  pose.box.width = width;
  pose.box.height = height;
  pose.box.min_body_height = state.doll.box_height * ro::ui::kCharacterAvatarBodyFloor;

  const ro::render::IconTexture& drawn = doll_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  state.doll.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                       .width = state.doll.box_width,
                                       .height = state.doll.box_height};
}

void StyleView::RefreshPipette(const ro::render::ActorLook& look,
                               const ro::ui::PaletteEditorState& state) {
  view_.pipette_ramp = -1;
  if (wardrobe_ == nullptr || !doll_.has_value() || !state.doll.hovered || body_.ramps.empty()) {
    return;
  }
  // ⚠ LE CADRAGE VIENT DE LA VIGNETTE ELLE-MÊME : le refaire ici le ferait
  // diverger du dessin un jour, et la pipette désignerait alors la pièce d'à
  // côté sans que rien ne le signale.
  const ro::render::VignetteFraming& framing = doll_->framing();
  if (!framing.usable()) {
    return;
  }
  // ⚠ LA MÊME APPARENCE QUE CELLE DESSINÉE, style d'aperçu compris : c'est la
  // composition affichée qu'on interroge, pas une autre.
  ro::render::ActorLook shown = look;
  shown.style = preview_style_;
  ro::render::ActorOutfit* outfit = wardrobe_->OutfitFor(shown);
  if (outfit == nullptr) {
    return;
  }
  const ro::render::ComposedActor& composed = outfit->Composed(
      ro::render::kAnimationIdle, state.doll.direction, 0.0F, /*loop=*/true, kHeldFrame);
  // L'INVERSE DU CADRAGE : une soustraction et une division, l'origine et
  // l'échelle étant en pixels de la boîte.
  const glm::vec2 at((state.doll.hover_x - framing.origin.x) / framing.scale,
                     (state.doll.hover_y - framing.origin.y) / framing.scale);
  const std::optional<std::uint8_t> index = ro::render::PaletteIndexAt(composed, at);
  if (!index.has_value()) {
    return;
  }
  // ⚠ UN INDEX HORS DE TOUTE RAMPE EST UNE VRAIE RÉPONSE : c'est la part du
  // corps qu'aucun réglage n'atteint, celle qu'annonce « Réglable : N% ».
  if (const std::optional<std::size_t> rank = ro::render::RampAt(body_.ramps, *index)) {
    view_.pipette_ramp = static_cast<int>(*rank);
  }
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
  if (wardrobe_ == nullptr || player == nullptr) {
    return;
  }
  view_.bounds = ro::ui::PaletteChoiceBounds{.first = ro::sim::kStyleFirstChoice,
                                             .hair_style = ro::sim::kHairStyleMax,
                                             .hair_palette = ro::sim::kHairPaletteMax,
                                             .body_palette = ro::sim::kBodyPaletteMax};
  view_.worn_hair_style = LookOf(*player).hair;

  // 🔴 L'AMORÇAGE PRÉCÈDE TOUT LE RESTE, et surtout la mesure des rampes. La
  // recette qu'on retrouve porte sa propre teinte de base, donc une autre
  // fusion et un autre découpage : mesurer d'abord donnerait une fenêtre où
  // les pastilles annoncent la bonne palette pendant que le pantin porte
  // encore les couleurs nues du sprite.
  //
  // ⚠ LA CLÉ SE PREND SUR L'APPARENCE NUE : c'est le corps qui décide de la
  // variante, et la variante ne doit pas décider du corps. Le pantin ne porte
  // que son corps : on en règle la palette.
  ro::render::ActorLook look = BodyOnlyLookOf(*player);
  const std::uint32_t worn_key = wardrobe_->BodyKeyFor(look);
  if (!seeded_ && !touched_) {
    if (const ro::sim::StyleRecipe* worn = WornRecipe(world, worn_key)) {
      state.recipe = FromWire(*worn);
      seeded_ = true;
    }
  }
  Impose(state.recipe, look);
  // ⚠ ON REMESURE QUAND LE CORPS CHANGE, et le corps change sans que la classe
  // bouge : un costume qu'on équipe ou qu'on retire suffit. Les réglages sont
  // CONSERVÉS et se ré-appliquent par index — c'est ce que font les autres
  // clients quand ce joueur change de corps, et la règle doit être la même
  // partout, sinon il se verrait autrement qu'on le voit.
  //
  // ⚠ ET QUAND LA TEINTE DE BASE CHANGE : elle désigne le fichier sur lequel
  // la base est fusionnée, donc le découpage des pièces.
  if (!measured_valid_ || worn_key != body_key_ || look.body_palette != measured_palette_) {
    ReloadRamps(look);
  }

  colors_.clear();
  ceilings_.clear();
  if (!body_.ramps.empty()) {
    const ro::formats::Palette painted =
        ro::render::ApplyRecipe(body_.base, body_.ramps, state.recipe);
    for (const ro::render::PaletteRamp& ramp : body_.ramps) {
      // La pastille se lit sur la palette PEINTE : ce que la fenêtre montre
      // est ce que le joueur verra sur lui.
      colors_.push_back(ToColor(ro::render::RampColor(painted, ramp)));
      // Le plafond, lui, se lit sur la BASE : c'est le sprite qui le fixe, et
      // aucun réglage ne le déplace.
      ceilings_.push_back(ro::render::RampValueCeiling(body_.base, ramp));
    }
  }

  view_.ramps = body_.ramps;
  view_.colors = colors_;
  view_.ceilings = ceilings_;
  if (body_.pixels_total > 0) {
    view_.coverage_percent = static_cast<int>(
        (static_cast<long long>(body_.pixels_covered) * kPercent + body_.pixels_total / 2) /
        body_.pixels_total);
  }

  const ro::sim::StyleVariants* mine = world.styles().Find(world.player_id());
  view_.body_style_count = mine != nullptr ? static_cast<int>(mine->by_body.size()) : 0;
  view_.body_has_own_style = mine != nullptr && body_key_ != 0 && mine->by_body.contains(body_key_);

  const ro::sim::StyleRecipe* worn = WornRecipe(world, body_key_);
  applied_ = worn != nullptr ? FromWire(*worn) : ro::render::PaletteRecipe{};
  view_.has_edits = !SameRecipe(state.recipe, applied_);
  view_.just_sent = sent_ms_ != 0 && now_ms - sent_ms_ < kSentNoticeMs;

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
  const ro::sim::StyleRecipe* kept_draft = drafts_.Find(body_key_);
  view_.has_draft = kept_draft != nullptr && !SameRecipe(FromWire(*kept_draft), state.recipe);
  // 🔴 LE CODE EST CELUI QUI VOYAGE : `app` comble ce que la recette laisse
  // ouvert, parce qu'un code quitte son porteur — voir `ShareableRecipe`.
  share_code_ = ro::sim::EncodeStyleCode(ToWire(Shareable(state.recipe, *player)));
  view_.share_code = share_code_.c_str();
  view_.code_refused = refused_ms_ != 0 && now_ms - refused_ms_ < kRefusedNoticeMs;

  RefreshDoll(look, state, now_ms);
  // ⚠ LA PIPETTE APRÈS LE PANTIN : c'est lui qui vient de poser le cadrage
  // dont elle est l'inverse.
  RefreshPipette(look, state);
  // ⚠ LES VIGNETTES EN DERNIER : elles se composent sur l'apparence du pantin,
  // teinte et coupe choisies comprises, et la grille des couleurs de cheveux
  // doit montrer la COUPE qu'on vient d'essayer.
  RefreshThumbnails(look, state);
}

void StyleView::Apply(const ro::ui::PaletteEditorAction& asked, ro::net::Connection& link,
                      ro::ui::PaletteEditorState& state, const ro::sim::World& world,
                      std::uint32_t now_ms) {
  // LA COULEUR DÉSIGNÉE, TRADUITE EN RÉGLAGE. 🔴 DEPUIS LA BASE, jamais depuis
  // la palette peinte : partir du résultat courant ferait s'additionner les
  // réglages à chaque clic, et la couleur dériverait sous la main du joueur.
  if (asked.picked_ramp >= 0 && asked.picked_ramp < static_cast<int>(body_.ramps.size())) {
    const auto rank = static_cast<std::size_t>(asked.picked_ramp);
    state.recipe.ramps[rank] =
        ro::render::AdjustToReach(body_.base, body_.ramps[rank], asked.picked_color);
    touched_ = true;
  }

  switch (asked.verb) {
    case ro::ui::PaletteEditorVerb::kNothing:
      break;
    case ro::ui::PaletteEditorVerb::kApply: {
      // ⚠ LA VARIANTE DE REPLI DÈS LA PREMIÈRE : un joueur qui n'a teint qu'un
      // corps veut garder ses couleurs en selle, et sans ce drapeau il
      // redeviendrait gris le temps d'un trajet.
      const ro::sim::StyleVariants* mine = world.styles().Find(world.player_id());
      const bool first = mine == nullptr || mine->by_body.empty();
      static_cast<void>(ro::net::SendPlayerStyle(link, ToWire(state.recipe), body_key_, first));
      sent_ms_ = now_ms;
      break;
    }
    case ro::ui::PaletteEditorVerb::kBackToWorn: {
      const ro::sim::StyleRecipe* worn = WornRecipe(world, body_key_);
      state.recipe = worn != nullptr ? FromWire(*worn) : ro::render::PaletteRecipe{};
      break;
    }
    case ro::ui::PaletteEditorVerb::kBlankSlate:
      state.recipe = ro::render::PaletteRecipe{};
      // 🔴 LE VERROU D'AMORÇAGE. Ces trois-là rendent la recette NEUTRE, et
      // c'est exactement ce que le rattrapage guette : sans ce drapeau, il
      // reposerait les couleurs du serveur à l'image suivante et la page
      // blanche se refermerait sous les doigts du joueur.
      touched_ = true;
      break;
    case ro::ui::PaletteEditorVerb::kForgetBody:
      static_cast<void>(ro::net::SendPlayerStyleClear(link, body_key_));
      state.recipe = ro::render::PaletteRecipe{};
      touched_ = true;
      sent_ms_ = now_ms;
      break;
    case ro::ui::PaletteEditorVerb::kDeleteAll:
      static_cast<void>(ro::net::SendPlayerStyleClearAll(link));
      state.recipe = ro::render::PaletteRecipe{};
      touched_ = true;
      sent_ms_ = now_ms;
      break;

    // ── LES PRÉRÉGLAGES ET LES CODES ────────────────────────────────────
    //
    // ⚠ AUCUN NE TOUCHE AU PERSONNAGE, et c'est la règle de toute la fenêtre :
    // charger un préréglage, coller un code, reprendre un brouillon ne font
    // que poser un APERÇU. Il faudra valider — un code vient d'un autre
    // joueur, et il ne doit pas pouvoir changer une apparence d'un clic.
    case ro::ui::PaletteEditorVerb::kLoadDraft:
      if (const ro::sim::StyleRecipe* kept = drafts_.Find(body_key_)) {
        state.recipe = FromWire(*kept);
        touched_ = true;
      }
      break;
    case ro::ui::PaletteEditorVerb::kSavePreset:
      // 🔴 LA RECETTE PARTAGEABLE : un préréglage se recharge des semaines
      // plus tard, éventuellement sur un autre personnage. Y laisser
      // « couleur d'origine » lui ferait prendre celle du moment, et le
      // joueur ne retrouverait pas l'allure qu'il croyait avoir rangée.
      if (const ro::sim::Actor* me = world.player();
          me != nullptr && presets_.Save(asked.preset, ToWire(Shareable(state.recipe, *me)))) {
        static_cast<void>(SaveStylePresets(client_root_, presets_));
      }
      break;
    case ro::ui::PaletteEditorVerb::kLoadPreset:
      if (const ro::sim::StyleRecipe* kept = presets_.Find(asked.preset)) {
        state.recipe = FromWire(*kept);
        // Geste explicite : plus de rattrapage d'amorçage par-dessus, même si
        // le préréglage se trouve être neutre.
        touched_ = true;
      }
      break;
    case ro::ui::PaletteEditorVerb::kDeletePreset:
      presets_.Erase(asked.preset);
      static_cast<void>(SaveStylePresets(client_root_, presets_));
      break;
    case ro::ui::PaletteEditorVerb::kSharePreset:
      // 🔴 LE CODE DU PRÉRÉGLAGE, pas celui du style en cours : le joueur
      // désigne une ligne, et c'est elle qu'il partage — même s'il a réglé
      // autre chose depuis.
      //
      // ⚠ ET IL VOYAGE SOUS LE NOM DU PRÉRÉGLAGE, pas sous notre pseudo : le
      // joueur partage « Rouge soirée », ce qui dit bien plus à son lecteur
      // que le nom de celui qui l'envoie — lequel est déjà en tête de ligne.
      if (const ro::sim::StyleRecipe* kept = presets_.Find(asked.preset)) {
        to_share_ = StyleToShare{.code = ro::sim::EncodeStyleCode(*kept), .label = asked.preset};
      }
      break;
    case ro::ui::PaletteEditorVerb::kCopyCode:
      // La fenêtre a déjà posé le code dans le presse-papiers : elle seule
      // parle à ImGui. Rien à faire ici.
      break;
    case ro::ui::PaletteEditorVerb::kShareCode:
      // Étiquette vide : c'est notre style, et `Game` y mettra notre pseudo.
      to_share_ = StyleToShare{.code = share_code_};
      break;
    case ro::ui::PaletteEditorVerb::kPasteCode:
      static_cast<void>(TryCode(asked.pasted, state, now_ms));
      break;
  }
}

bool StyleView::TryCode(std::string_view code, ro::ui::PaletteEditorState& state,
                        std::uint32_t now_ms) {
  const std::optional<ro::sim::StyleCode> read = ro::sim::DecodeStyleCode(code);
  if (!read.has_value()) {
    // ⚠ ET ON LE DIT : un collage raté qui ne dit rien laisse croire à un
    // défaut de l'éditeur. ⚠ JAMAIS ZÉRO, qui est la sentinelle.
    refused_ms_ = now_ms != 0 ? now_ms : 1;
    return false;
  }
  state.recipe = FromWire(read->recipe);
  // 🔴 LE VERROU D'AMORÇAGE : charger un code est un geste EXPLICITE. Sans ce
  // drapeau, le rattrapage reposerait les couleurs du serveur à l'image
  // suivante et le style chargé disparaîtrait sous les doigts du joueur.
  touched_ = true;
  refused_ms_ = 0;
  return true;
}

StyleView::StyleToShare StyleView::TakeStyleToShare() {
  return std::exchange(to_share_, StyleToShare{});
}

void StyleView::TickDraft(const ro::ui::PaletteEditorState& state, std::uint32_t now_ms) {
  if (body_key_ == 0) {
    return;
  }
  // 🔴 LE BROUILLON N'EXISTE QUE PAR SON ÉCART AVEC CE QUI EST POSÉ. Tant que
  // les deux coïncident, il n'y a rien à récupérer, et proposer de recharger
  // l'identique serait un bouton qui ment.
  const ro::sim::StyleRecipe told = ToWire(state.recipe);
  const std::string code = ro::sim::EncodeStyleCode(told);
  if (!draft_seen_valid_ || code != draft_seen_) {
    draft_seen_ = code;
    draft_seen_valid_ = true;
    // ⚠ L'HORLOGE REPART À CHAQUE CHANGEMENT : c'est l'IMMOBILITÉ qu'on
    // attend, pas un délai depuis le premier geste. Un glissement continu ne
    // doit rien écrire tant qu'il dure.
    //
    // ⚠ ET JAMAIS ZÉRO : c'est la sentinelle « rien en attente », et une
    // partie qui démarre pile à cet instant n'écrirait alors jamais.
    draft_since_ms_ = now_ms != 0 ? now_ms : 1;
    return;
  }
  if (draft_since_ms_ == 0 || now_ms - draft_since_ms_ < kDraftDelayMs) {
    return;
  }
  draft_since_ms_ = 0;

  // Ce qui coïncide avec le style POSÉ n'est pas un brouillon : on l'oublie.
  const bool worth_keeping = !SameRecipe(state.recipe, applied_);
  const bool changed = worth_keeping ? drafts_.Put(body_key_, told) : drafts_.Forget(body_key_);
  if (changed) {
    static_cast<void>(SaveStyleDrafts(client_root_, drafts_));
  }
}

void StyleView::Close() {
  if (wardrobe_ != nullptr && preview_style_ != 0) {
    wardrobe_->ForgetStyle(preview_style_);
  }
  preview_style_ = 0;
  shown_valid_ = false;
  // ⚠ LES VIGNETTES AUSSI : une grille refermée ne vaut aucune texture, et
  // celles des trois grilles réunies en tiendraient huit cent quatre-vingt-
  // quatre.
  if (thumbnails_.has_value()) {
    thumbnails_->Clear();
  }
  thumbs_.clear();
  shown_picker_ = ro::ui::PaletteEditorPicker::kNone;
  // ⛔ `seeded_` ET `touched_` NE SE REMETTENT PAS À ZÉRO ICI, et c'est le
  // point délicat. Refermer la fenêtre ne rend pas ses couleurs au joueur —
  // c'est tout l'objet du brouillon : il rouvre et retrouve ce qu'il
  // composait. Un ré-amorçage à chaque ouverture écraserait des retouches
  // qu'il n'a pas encore partagées.
}

}  // namespace moonclient
