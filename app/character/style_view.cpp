#include "character/style_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "ro/render/palette_ramps.h"
#include "ro/render/shown_job.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"
#include "ro/ui/character_window.h"

#include "character/character_styles.h"
#include "character/style_recipe.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// Le calque des effets de chapeau sur le fil : la règle 25 doit l'annoncer.
constexpr int kHatEffectWireLayer = static_cast<int>(ro::sim::StyleLayerKind::kHatEffect);

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

// Ce que le pantin montre.
constexpr int kShowsRecipe = 0;
constexpr int kShowsWorn = 1;
constexpr int kShowsPreset = 2;

// LES MONTURES qu'un corps peut prendre, et leur nom. Le corps d'un métier qui
// n'en a pas reste celui d'à pied, et la liste l'écarte.
struct MountOption {
  std::uint32_t option = 0;
  const char* name = nullptr;
};
constexpr std::array<MountOption, 4> kMounts = {{
    {ro::render::kOptionPeco, ro::core::TrText("Monté")},
    {ro::render::kOptionGreenDragon, ro::core::TrText("Dragon")},
    {ro::render::kOptionWolfRider, ro::core::TrText("Loup")},
    {ro::render::kOptionMadogear, ro::core::TrText("Madogear")},
}};

[[nodiscard]] ImU32 ToColor(std::array<std::uint8_t, 3> rgb) {
  return IM_COL32(rgb[0], rgb[1], rgb[2], kFullValue);
}

// Ce que la partie dit de nous, pour la session.
[[nodiscard]] StyleSession::Worn WornOf(const ro::sim::World& world, std::uint32_t body_key) {
  return StyleSession::Worn{
      .body_key = body_key,
      .mine = world.styles().Find(world.player_id()),
      .book_revision = world.styles().RevisionOf(world.player_id()),
      .body_verdict = world.styles().LastVerdict(ro::sim::StyleSubject::kBody),
      .layers_verdict = world.styles().LastVerdict(ro::sim::StyleSubject::kLayers),
      .layers_cadence_ms = world.session().style_layers_cadence_ms};
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

ro::render::PaletteRecipe Shareable(const ro::render::PaletteRecipe& recipe,
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

StyleView::StyleView(ro::render::ActorWardrobe* wardrobe, std::filesystem::path client_root,
                     std::uint32_t character_id)
    : wardrobe_(wardrobe), client_root_(std::move(client_root)), character_id_(character_id) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
    thumbnails_.emplace(*wardrobe_);
  }
  LoadStylePresets(client_root_, presets_);
  StyleDrafts drafts;
  LoadStyleDrafts(client_root_, character_id_, drafts);
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
  base_colors_.clear();
  ceilings_.clear();
  if (!body.ramps.empty()) {
    const ro::formats::Palette painted =
        ro::render::ApplyRecipe(body.base, body.ramps, session_.recipe());
    for (const ro::render::PaletteRamp& ramp : body.ramps) {
      // La pastille se lit sur la palette PEINTE : ce que la fenêtre montre
      // est ce que le joueur verra sur lui.
      colors_.push_back(ToColor(ro::render::RampColor(painted, ramp)));
      // Le nom de la pièce, lui, se lit sur la BASE : il ne change pas quand
      // on la repeint.
      base_colors_.push_back(ToColor(ro::render::RampColor(body.base, ramp)));
      // Le plafond aussi : c'est le sprite qui le fixe, et aucun réglage ne
      // le déplace.
      ceilings_.push_back(ro::render::RampValueCeiling(body.base, ramp));
    }
  }
  // 🔴 LE CODE EST CELUI QUI VOYAGE : il comble ce que la recette laisse
  // ouvert, parce qu'un code quitte son porteur — voir `Shareable`.
  share_code_ = ro::sim::EncodeStyleCode(ToWire(Shareable(session_.recipe(), worn)));
}

void StyleView::RefreshBodies(const ro::render::ActorLook& worn_body, std::uint32_t worn_key) {
  if (!bodies_.empty() && worn_key == bodies_for_) {
    // ⚠ LES APPARENCES SE REFONT À CHAQUE IMAGE, les clés non : la coupe ou la
    // couleur portées changent sans changer de corps.
    for (BodyChoice& body : bodies_) {
      const ro::render::ActorLook tweak = body.look;
      body.look = worn_body;
      body.look.job = tweak.job;
      body.look.cash_mount = tweak.cash_mount;
      body.look.body_style = tweak.body_style;
    }
    return;
  }
  // 🔴 UN CHANGEMENT DE CORPS PORTÉ RAMÈNE AU CORPS PORTÉ : le rang d'avant ne
  // désigne plus la même variante.
  bodies_for_ = worn_key;
  edited_body_ = 0;
  bodies_.clear();
  body_names_.clear();
  const auto offer = [this](const char* name, const ro::render::ActorLook& look) {
    const std::uint32_t key = wardrobe_->BodyKeyFor(look);
    if (key == 0) {
      return;
    }
    for (const BodyChoice& known : bodies_) {
      if (known.key == key) {
        return;
      }
    }
    bodies_.push_back(BodyChoice{.name = name, .look = look, .key = key});
    body_names_.push_back(name);
  };
  bodies_.push_back(
      BodyChoice{.name = ro::core::TrText("Corps porté"), .look = worn_body, .key = worn_key});
  body_names_.push_back(bodies_.front().name);

  const std::uint32_t foot = ro::render::FootJobOf(worn_body.job);
  ro::render::ActorLook variant = worn_body;
  variant.job = foot;
  offer(ro::core::TrText("À pied"), variant);
  for (const MountOption& mount : kMounts) {
    variant = worn_body;
    variant.job = ro::render::ShownJobFor(foot, mount.option);
    if (variant.job != foot) {
      offer(mount.name, variant);
    }
  }
  variant = worn_body;
  variant.cash_mount = !worn_body.cash_mount;
  offer(worn_body.cash_mount ? ro::core::TrText("Sans monture de boutique")
                             : ro::core::TrText("Monture de boutique"),
        variant);
  variant = worn_body;
  variant.body_style = worn_body.body_style == 0 ? 1 : 0;
  offer(worn_body.body_style == 0 ? ro::core::TrText("Tenue alternative")
                                  : ro::core::TrText("Tenue ordinaire"),
        variant);
}

const ro::render::PaletteRecipe& StyleView::Shown(const ro::ui::PaletteEditorState& state) {
  ShownKey key{.source = kShowsRecipe,
               .revision = session_.revision(),
               .layers_revision = layers_.revision()};
  const ro::render::PaletteRecipe* shown = &session_.recipe();
  if (state.doll.before) {
    key.source = kShowsWorn;
    shown = &session_.applied();
  } else if (state.hovered_preset >= 0 &&
             static_cast<std::size_t>(state.hovered_preset) < presets_.all().size()) {
    key.source = kShowsPreset;
    key.preset = state.hovered_preset;
    preview_ = FromWire(presets_.all()[static_cast<std::size_t>(state.hovered_preset)].recipe);
    shown = &preview_;
  }
  if (!(key == shown_key_)) {
    shown_key_ = key;
    ++shown_serial_;
  }
  return *shown;
}

void StyleView::RefreshDoll(const ro::render::ActorLook& look,
                            const ro::render::PaletteRecipe& shown,
                            ro::ui::PaletteEditorState& state, std::uint32_t now_ms) {
  state.doll.image = ro::ui::SkinImage{};
  const LayerTab* tab = OpenLayer();
  // LES POSES D'UN COMPAGNON sont celles du .act d'un monstre : repos, marche,
  // attaque. Il ne s'assoit pas : sa pose « assis » est le repos.
  constexpr std::array<std::size_t, ro::ui::kPaletteDollPoses.size()> kCompanionPoses = {0, 1, 0,
                                                                                         2};
  const auto pose_rank = static_cast<std::size_t>(
      std::clamp(state.doll.pose, 0, static_cast<int>(ro::ui::kPaletteDollPoses.size()) - 1));
  const std::size_t pose = tab != nullptr && tab->companion_look.has_value()
                               ? kCompanionPoses[pose_rank]
                               : ro::ui::kPaletteDollPoses[pose_rank];
  // LES TEINTURES DE CALQUES : ce qu'on compose, ou ce qu'on porte tant que
  // « avant / après » est tenu.
  if (shown_key_.source == kShowsWorn) {
    composed_.assign(layers_.worn().begin(), layers_.worn().end());
  } else {
    composed_ = layers_.Composed();
  }
  const StyleDoll::Shot shot{
      .look = look,
      .recipe = &shown,
      .revision = shown_serial_,
      // ⚠ LA PULSATION NE VAUT QUE SUR CE QU'ON COMPOSE : sur ce qu'on porte ou
      // sur un préréglage survolé, elle désignerait des pièces qu'on ne règle
      // pas. Et seulement sur le corps, le seul dont le pantin peint les pièces
      // lui-même.
      .highlight = shown_key_.source == kShowsRecipe && tab == nullptr ? state.hovered_ramp : -1,
      .now_ms = now_ms,
      .direction = state.doll.direction,
      .animation = pose,
      .animated = state.doll.animated,
      .box = ro::render::VignetteBox{.width = static_cast<std::int32_t>(state.doll.box_width),
                                     .height = static_cast<std::int32_t>(state.doll.box_height),
                                     .min_body_height =
                                         state.doll.box_height * ro::ui::kCharacterAvatarBodyFloor},
      .layers = &composed_,
      // 🔴 LA PEAU SE RÈGLE SUR LA SILHOUETTE ENTIÈRE, même sous la Tête : le
      // visage et les bras se montrent ensemble.
      .focus = state.skin_focus ? ro::render::StyleLayer::kSkin
               : tab != nullptr ? tab->at.layer
                                : ro::render::StyleLayer::kBody};
  hat_effect_doll_.reset();
  const ro::render::IconTexture* drawn = doll_->Draw(shot);
  if (drawn == nullptr) {
    return;
  }
  state.doll.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn->texture.name()),
                                       .width = state.doll.box_width,
                                       .height = state.doll.box_height};
  // LES EFFETS DE CHAPEAU QUE LE JOUEUR AFFICHE se posent sur le pantin de
  // l'onglet des effets, et là seulement : c'est là qu'on les règle, et ailleurs
  // ils cacheraient les pièces qu'on teint.
  if (view_tab_ != ro::ui::PaletteTabKind::kEffects) {
    return;
  }
  const std::optional<StyleDoll::EffectSeat> seat = doll_->EffectSeatOf();
  if (!seat.has_value() || worn_hat_effects_.empty()) {
    return;
  }
  hat_effect_doll_ =
      HatEffectDoll{.worn = worn_hat_effects_,
                    .seat = *seat,
                    .box_px = glm::vec2(state.doll.box_width, state.doll.box_height)};
}

void StyleView::RefreshThumbnails(const ro::render::ActorLook& look,
                                  const ro::ui::PaletteEditorState& state) {
  thumbs_.clear();
  view_.thumbnails = {};
  view_.thumbnails_first = 0;
  const ro::ui::PaletteEditorPicker open = state.picker.open;
  if (open != shown_picker_) {
    // ⚠ LES TEXTURES DE LA GRILLE PRÉCÉDENTE SE RENDENT ICI, au changement, et
    // pas à chaque image : une grille refermée n'en vaut aucune.
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
  if (!served_kept_ && world.session().style_char_select_served) {
    served_kept_ = true;
    static_cast<void>(NoteCharacterStylesServed(client_root_));
  }
  const std::uint32_t revision = world.styles().RevisionOf(world.player_id());
  if (revision == 0 || revision == kept_revision_ || character_id == 0) {
    return;
  }
  kept_revision_ = revision;
  // La tenue de ce que le serveur dit que nous montrons : nos corps, et les
  // teintures que notre bloc porte.
  const ro::sim::StyleOutfit shown = OutfitOf(world.styles().Find(world.player_id()),
                                              world.styles().FindLayers(world.player_id()));
  static_cast<void>(NoteCharacterStyles(client_root_, character_id, &shown));
}

void StyleView::SeedFromSelection(ro::sim::World& world, std::uint32_t character_id) {
  const std::uint32_t player_id = world.player_id();
  if (seeded_ || player_id == 0 || character_id == 0) {
    return;
  }
  seeded_ = true;
  // ⚠ LE SERVEUR A DÉJÀ PARLÉ : ce qu'il dit l'emporte sur ce qu'on avait gardé.
  if (world.styles().RevisionOf(player_id) != 0) {
    return;
  }
  const KeptCharacterStyles kept = LoadCharacterStyles(client_root_);
  const auto found = kept.characters.find(character_id);
  if (found == kept.characters.end()) {
    return;
  }
  // ⚠ LA V7 SEULE : les teintures de calques arrivent avec le premier bloc du
  // serveur, à l'entrée sur la carte.
  const ro::sim::StyleVariants variants = VariantsOfOutfit(found->second);
  if (variants.empty()) {
    return;
  }
  world.styles().BeginBatchFor(player_id);
  for (const auto& [body_key, recipe] : variants.by_body) {
    world.styles().Remember(player_id, body_key, recipe, body_key == variants.fallback_body);
  }
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

  // ── LE CORPS RÉGLÉ ───────────────────────────────────────────────────────
  //
  // ⚠ LA CLÉ SE PREND SUR L'APPARENCE NUE : c'est le corps qui décide de la
  // variante, et la variante ne doit pas décider du corps. Le pantin ne porte
  // que son corps : on en règle la palette.
  const ro::render::ActorLook worn_body = BodyOnlyLookOf(*player);
  RefreshBodies(worn_body, wardrobe_->BodyKeyFor(worn_body));
  edited_body_ = std::clamp(edited_body_, 0, static_cast<int>(bodies_.size()) - 1);
  const BodyChoice& edited = bodies_[static_cast<std::size_t>(edited_body_)];

  // 🔴 LA SESSION D'ABORD, LA MESURE ENSUITE. La recette qu'on retrouve porte
  // sa propre teinte de base, donc une autre fusion et un autre découpage :
  // mesurer d'abord donnerait une fenêtre où les pastilles annoncent la bonne
  // palette pendant que le pantin porte encore les couleurs nues du sprite.
  session_.Observe(WornOf(world, edited.key), now_ms);
  RefreshLayerState(world, now_ms);
  ro::render::ActorLook look = edited.look;
  const ro::render::PaletteRecipe& shown = Shown(state);
  Impose(shown, look);
  // Le pantin peint sur cette mesure-ci : celle de ce qu'il montre.
  const ro::render::ActorWardrobe::BodyRamps& measured = doll_->Measure(look, session_.body_key());
  // 🔴 UN PRÉRÉGLAGE SURVOLÉ NE CHANGE QUE LE PANTIN. Sa teinte de base
  // redécouperait les pièces ; la liste changerait de hauteur, la fenêtre de
  // taille, la ligne survolée glisserait sous la souris, et l'aperçu
  // s'éteindrait pour se rallumer à l'image suivante, sans fin. La liste et les
  // onglets restent donc sur la recette qu'on règle.
  const bool previewing = shown_key_.source == kShowsPreset;
  ro::render::ActorLook settled_look = edited.look;
  if (previewing) {
    Impose(session_.recipe(), settled_look);
    listed_body_ = wardrobe_->BodyRampsFor(settled_look);
  }
  const ro::render::ActorWardrobe::BodyRamps& body = previewing ? listed_body_ : measured;
  RefreshPieces(body, worn);

  // ── LES CALQUES PORTÉS ───────────────────────────────────────────────────
  //
  // ⚠ SUR L'APPARENCE ENTIÈRE, coupe et couleur essayées comprises : la
  // teinture des cheveux se range sous la coupe qu'on montre.
  ro::render::ActorLook full = worn;
  full.job = look.job;
  full.cash_mount = look.cash_mount;
  full.body_style = look.body_style;
  Impose(shown, full);
  // Les onglets, eux aussi, sur la recette qu'on règle : une coupe essayée par
  // un préréglage survolé changerait la clé de la tête.
  ro::render::ActorLook settled_full = full;
  if (previewing) {
    settled_full.job = settled_look.job;
    settled_full.cash_mount = settled_look.cash_mount;
    settled_full.body_style = settled_look.body_style;
    Impose(session_.recipe(), settled_full);
  }
  RefreshTabs(previewing ? settled_full : full, world);
  // 🔴 L'ONGLET DEMANDÉ, S'IL EST LÀ ET RÉGLABLE : un chapeau ôté ramène au
  // corps.
  const LayerTab* asked_tab = TabOf(state.tab);
  view_tab_ = asked_tab != nullptr && asked_tab->shown.blocked == ro::ui::PaletteTabBlock::kNone
                  ? state.tab
                  : ro::ui::PaletteTabKind::kBody;
  // Ce que le joueur affiche, rangé et sans doublon : le pantin le porte.
  worn_hat_effects_.assign(player->hat_effects.begin(), player->hat_effects.end());
  // L'ONGLET DES EFFETS DE CHAPEAU, en queue, quand le joueur en affiche : il
  // n'est pas un calque de palette et ne passe pas par les pièces.
  hat_effects_.Refresh(world, state.effects, connected, server_layer_last_ >= kHatEffectWireLayer,
                       atelier_open_, wardrobe_ != nullptr ? &wardrobe_->catalog().items : nullptr,
                       now_ms);
  view_.effects = &hat_effects_.view();
  if (!hat_effects_.view().rows.empty()) {
    ro::ui::PaletteTab effects;
    effects.kind = ro::ui::PaletteTabKind::kEffects;
    effects.name = ro::ui::PaletteTabName(ro::ui::PaletteTabKind::kEffects);
    effects.dyed = std::ranges::any_of(hat_effects_.view().rows,
                                       [](const auto& row) { return !row.worn.neutral(); });
    tab_views_.push_back(std::move(effects));
    if (state.tab == ro::ui::PaletteTabKind::kEffects) {
      view_tab_ = ro::ui::PaletteTabKind::kEffects;
    }
  }
  // L'onglet d'un objet qui pose un effet mène à celui des effets. ⚠ LA PREMIÈRE
  // VUE EST LE CORPS : celle d'un calque vient un rang plus loin.
  for (std::size_t rank = 0; rank < tabs_.size() && rank + 1 < tab_views_.size(); ++rank) {
    ro::ui::PaletteTab& view_of_tab = tab_views_[rank + 1];
    view_of_tab.poses_effect = view_of_tab.item && hat_effects_.PosesEffect(tabs_[rank].item_id);
  }

  view_.recipe = session_.recipe();
  view_.ramps = body.ramps;
  view_.colors = colors_;
  view_.base_colors = base_colors_;
  view_.ceilings = ceilings_;
  const PieceMarks& marks = Marks();
  view_.links = marks.links();
  view_.locked = marks.locked();
  if (body.pixels_total > 0) {
    view_.coverage_percent = static_cast<int>(
        (static_cast<long long>(body.pixels_covered) * kPercent + body.pixels_total / 2) /
        body.pixels_total);
  }
  // La suite et le reste se rangent sous les huit pièces v7 ; un onglet de
  // calque ouvert remplace ensuite ces rangées par les siennes, mais la pipette
  // lit encore celles du corps.
  RefreshBodyMore(body);
  view_.body_style_count = session_.style_count();
  view_.body_has_own_style = session_.own_style();
  view_.is_main = session_.is_main();
  view_.has_edits = session_.has_edits();
  view_.has_something_to_send = session_.HasSomethingToSend();
  view_.can_send = session_.CanSend(now_ms);
  view_.evicts_oldest = session_.EvictsOldest();
  view_.can_undo = !body_undo_.empty() || session_.can_undo();
  view_.can_redo = !body_redo_.empty() || session_.can_redo();
  view_.send = ToView(session_.send_state());
  view_.bodies = body_names_;
  view_.edited_body = edited_body_;
  view_.tabs = tab_views_;
  view_.tab = view_tab_;
  view_.atelier_open = atelier_open_;
  // 🔴 « VALIDER » ENVOIE LE CORPS ET LES CALQUES DU MÊME CLIC : il a quelque
  // chose à faire si l'un des deux en a, et peut partir si chacun de ceux qui
  // ont à faire le peut.
  const bool body_pending = session_.HasSomethingToSend();
  const bool layers_pending = !LayerOrders(atelier_open_).empty();
  view_.has_something_to_send = body_pending || layers_pending;
  view_.can_send = (!body_pending || session_.CanSend(now_ms)) &&
                   (!layers_pending || session_.CanSendLayers(now_ms));
  view_.items_wait_atelier = !atelier_open_ && !LayerOrders(/*atelier_open=*/true).empty() &&
                             !layers_pending && !body_pending;
  view_.layers_send = ToView(session_.layers_send_state());
  view_.layers_refusal = LayersRefusal();

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

  // ⚠ APRÈS CE QUI EST RANGÉ : un onglet de calque montre SON code.
  if (const LayerTab* tab = OpenLayer()) {
    RefreshLayerPieces(*tab);
  }
  RefreshSkinLine();
  RefreshDoll(DollLook(look, full), shown, state, now_ms);

  // ⚠ LA PIPETTE APRÈS LE PANTIN, sur le point relevé à l'image précédente :
  // c'est lui qui vient de poser le cadrage dont elle est l'inverse. Et
  // seulement sur ce qu'on compose : les pièces de ce qu'on porte peuvent être
  // découpées autrement.
  if (shown_key_.source == kShowsRecipe) {
    ReadPipette(state);
  }

  // ⚠ LES VIGNETTES EN DERNIER : elles se composent sur l'apparence du pantin,
  // teinte et coupe choisies comprises.
  RefreshThumbnails(look, state);
  // Les tenues : la section de la vue, et une tenue reprise versée en
  // brouillon, que l'image suivante montrera.
  RefreshOutfits(world, now_ms);
}

void StyleView::Tick(const ro::sim::World& world, std::uint32_t now_ms) {
  // ⚠ LE CORPS GARDÉ : fenêtre fermée, on ne le remesure pas. Un changement de
  // corps se constate à la prochaine ouverture.
  session_.Observe(WornOf(world, session_.body_key()), now_ms);
  session_.TickDraft(now_ms);
  if (session_.TakeDraftsChanged()) {
    static_cast<void>(SaveStyleDrafts(client_root_, character_id_, session_.drafts()));
  }
}

StyleView::StyleToShare StyleView::TakeStyleToShare() {
  return std::exchange(to_share_, StyleToShare{});
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
  // ⚠ L'APERÇU DES EFFETS S'ÉTEINT AVEC LA FENÊTRE : un réglage non validé ne
  // reste pas sur le personnage.
  hat_effects_.Close();
  // ⛔ LA SESSION NE SE REMET PAS À ZÉRO : refermer la fenêtre ne rend pas ses
  // couleurs au joueur. Il rouvre et retrouve ce qu'il composait.
}

}  // namespace moonclient
