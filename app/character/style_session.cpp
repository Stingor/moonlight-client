#include "character/style_session.h"

#include <utility>

#include "character/style_recipe.h"

namespace moonclient {
namespace {

// L'ATTENTE DE L'ÉCHO. ⚠ LARGE : le serveur répond en un aller-retour, mais un
// réseau chargé en vaut plusieurs, et annoncer refusé ce qui revient une
// seconde plus tard ferait renvoyer pour rien.
constexpr std::uint32_t kEchoWaitMs = 5000;
// Ce que l'annonce reste à l'écran, une fois tranchée. ⚠ LE REFUS PLUS
// LONGTEMPS : il demande un geste, l'accusé n'en demande aucun.
constexpr std::uint32_t kSharedNoticeMs = 4000;
constexpr std::uint32_t kRefusedNoticeMs = 8000;
// LE DÉLAI D'IMMOBILITÉ AVANT D'ÉCRIRE LE BROUILLON. ⚠ ASSEZ LONG POUR QU'UN
// GLISSEMENT DANS LE NUANCIER NE PRODUISE QU'UNE ÉCRITURE, assez court pour que
// ce qui se perd tienne dans une seconde et demie de réglages.
constexpr std::uint32_t kDraftDelayMs = 1500;

// Une recette vide, pour qui la demande avant tout corps.
const ro::render::PaletteRecipe kNoRecipe{};

// Un instant qui sert aussi de sentinelle : jamais zéro.
[[nodiscard]] std::uint32_t Stamp(std::uint32_t now_ms) { return now_ms != 0 ? now_ms : 1; }

// L'ÉCHO PORTE-T-IL CE QU'ON A ENVOYÉ ?
//
// ⚠ SAUF LA COUPE : le serveur y met celle que le personnage porte, pas celle
// qu'il a rangée. ⚠ ET SAUF LA COULEUR DE CHEVEUX QU'ON N'IMPOSAIT PAS : un
// serveur peut ranger à sa place celle du personnage. La recette reprend les
// deux juste après l'écho, en suivant ce que le corps porte.
[[nodiscard]] bool Echoes(const ro::sim::StyleRecipe& echoed,
                          const ro::render::PaletteRecipe& sent) {
  ro::render::PaletteRecipe heard = FromWire(echoed);
  heard.hair_style = sent.hair_style;
  if (sent.hair_palette <= 0) {
    heard.hair_palette = sent.hair_palette;
  }
  return SameRecipe(heard, sent);
}

// L'écho de cette demande est-il là ?
[[nodiscard]] bool Echoed(StyleSendKind kind, std::uint32_t body_key,
                          const ro::render::PaletteRecipe& sent,
                          const ro::sim::StyleVariants* mine) {
  switch (kind) {
    case StyleSendKind::kApply: {
      if (mine == nullptr) {
        return false;
      }
      const auto own = mine->by_body.find(body_key);
      return own != mine->by_body.end() && Echoes(own->second, sent);
    }
    case StyleSendKind::kMakeMain: {
      // ⚠ LA RECETTE NE SUFFIT PAS : elle était peut-être déjà là, et c'est le
      // rang de principal qu'on demandait.
      if (mine == nullptr || mine->fallback_body != body_key) {
        return false;
      }
      const auto own = mine->by_body.find(body_key);
      return own != mine->by_body.end() && Echoes(own->second, sent);
    }
    case StyleSendKind::kForgetBody:
      return mine == nullptr || !mine->by_body.contains(body_key);
    case StyleSendKind::kDeleteAll:
      return mine == nullptr || mine->empty();
  }
  return false;
}

}  // namespace

StyleSession::StyleSession(StyleDrafts drafts) : drafts_(std::move(drafts)) {
  slots_.try_emplace(body_key_);
}

StyleSession::Slot& StyleSession::Current() { return slots_[body_key_]; }

const ro::render::PaletteRecipe& StyleSession::recipe() const {
  const auto found = slots_.find(body_key_);
  return found != slots_.end() ? found->second.recipe : kNoRecipe;
}

bool StyleSession::has_edits() const { return !SameRecipe(recipe(), applied_); }

bool StyleSession::OffersDraft() const {
  const ro::sim::StyleRecipe* kept = drafts_.Find(body_key_);
  return kept != nullptr && !SameRecipe(FromWire(*kept), recipe());
}

void StyleSession::Changed(Slot& slot) {
  ++revision_;
  slot.changed_at = revision_;
}

void StyleSession::Put(const ro::render::PaletteRecipe& recipe) {
  Slot& slot = Current();
  slot.touched = true;
  if (SameRecipe(slot.recipe, recipe)) {
    return;
  }
  slot.recipe = recipe;
  Changed(slot);
}

void StyleSession::Gesture(const ro::render::PaletteRecipe& recipe, std::uint32_t stroke) {
  Slot& slot = Current();
  slot.touched = true;
  if (SameRecipe(slot.recipe, recipe)) {
    return;
  }
  // ⚠ UN TRAIT EN COURS PROLONGE SON PAS : seul son point de départ s'est
  // rangé, au premier réglage.
  if (stroke == 0 || stroke != slot.stroke) {
    Remember(slot);
  }
  slot.redo.clear();
  slot.stroke = stroke;
  slot.recipe = recipe;
  Changed(slot);
}

void StyleSession::Remember(Slot& slot) {
  ++steps_taken_;
  if (slot.undo.size() >= kStyleHistoryDepth) {
    slot.undo.erase(slot.undo.begin());
  }
  slot.undo.push_back(slot.recipe);
}

bool StyleSession::can_undo() const {
  const auto found = slots_.find(body_key_);
  return found != slots_.end() && !found->second.undo.empty();
}

bool StyleSession::can_redo() const {
  const auto found = slots_.find(body_key_);
  return found != slots_.end() && !found->second.redo.empty();
}

bool StyleSession::Undo() {
  Slot& slot = Current();
  if (slot.undo.empty()) {
    return false;
  }
  slot.redo.push_back(slot.recipe);
  slot.recipe = slot.undo.back();
  slot.undo.pop_back();
  // 🔴 DÉFAIRE EST UN GESTE : le verrou se pose, sans quoi le corps reprendrait
  // à l'image suivante ce qu'il porte, et défaire n'aurait rien défait.
  slot.touched = true;
  slot.stroke = 0;
  Changed(slot);
  return true;
}

bool StyleSession::Redo() {
  Slot& slot = Current();
  if (slot.redo.empty()) {
    return false;
  }
  slot.undo.push_back(slot.recipe);
  slot.recipe = slot.redo.back();
  slot.redo.pop_back();
  slot.touched = true;
  slot.stroke = 0;
  Changed(slot);
  return true;
}

bool StyleSession::HasSomethingToSend() const {
  if (has_edits()) {
    return true;
  }
  // La recette est ce que le corps porte : la valider ne lui donne sa propre
  // variante que s'il reprenait celle d'un autre.
  return !own_style_ && style_count_ > 0;
}

bool StyleSession::CanSend(std::uint32_t now_ms) const {
  if (pending_.has_value()) {
    return false;
  }
  return last_sent_ms_ == 0 || now_ms - last_sent_ms_ >= kStyleSendCadenceMs;
}

void StyleSession::Follow() {
  Slot& slot = Current();
  if (slot.touched || SameRecipe(slot.recipe, applied_)) {
    return;
  }
  slot.recipe = applied_;
  Changed(slot);
}

void StyleSession::Observe(const Worn& worn, std::uint32_t now_ms) {
  // ── LE CORPS ─────────────────────────────────────────────────────────────
  //
  // 🔴 LE TRAVAIL DE L'AUTRE CORPS RESTE DE CÔTÉ, il ne se ré-applique pas
  // ici : ses rangs ne désignent pas les mêmes pièces.
  if (!observed_ || worn.body_key != body_key_) {
    observed_ = true;
    body_key_ = worn.body_key;
    slots_.try_emplace(body_key_);
    ++revision_;
  }

  // ── CE QU'IL PORTE ───────────────────────────────────────────────────────
  //
  // ⚠ LA CLÉ NULLE NE DÉSIGNE AUCUNE VARIANTE : un corps inconnu ne porte rien.
  const ro::sim::StyleVariants* mine = worn.mine;
  const ro::sim::StyleRecipe* wears =
      mine != nullptr && body_key_ != 0 ? mine->For(body_key_) : nullptr;
  applied_ = wears != nullptr ? FromWire(*wears) : ro::render::PaletteRecipe{};
  own_style_ = mine != nullptr && body_key_ != 0 && mine->by_body.contains(body_key_);
  main_ = own_style_ && mine->fallback_body == body_key_;
  style_count_ = mine != nullptr ? static_cast<int>(mine->by_body.size()) : 0;
  worn_bodies_.clear();
  if (mine != nullptr) {
    for (const auto& variant : mine->by_body) {
      worn_bodies_.push_back(variant.first);
    }
  }
  book_revision_ = worn.book_revision;

  // ⚠ LE VERDICT AVANT L'ÉCHO : un refus n'aura pas d'écho, et dit pourquoi.
  if (worn.body_verdict.sequence != body_verdict_seen_) {
    body_verdict_seen_ = worn.body_verdict.sequence;
    HearBodyVerdict(worn.body_verdict, now_ms);
  }
  HearEcho(worn, now_ms);
  HearLayers(worn, now_ms);

  // ── L'ANNONCE S'EFFACE ───────────────────────────────────────────────────
  const std::uint32_t shown_ms = now_ms - announced_ms_;
  if ((send_state_ == StyleSendState::kShared && shown_ms >= kSharedNoticeMs) ||
      (send_state_ == StyleSendState::kRefused && shown_ms >= kRefusedNoticeMs)) {
    send_state_ = StyleSendState::kNone;
  }

  // ⚠ APRÈS L'ÉCHO : c'est lui qui vient de lever le verrou d'une demande
  // tenue, et le corps reprend dans la même image ce qu'il porte désormais.
  Follow();
}

void StyleSession::HearEcho(const Worn& worn, std::uint32_t now_ms) {
  if (!pending_.has_value()) {
    return;
  }
  const Pending& asked = *pending_;
  // ⚠ UN LOT QUI NE PORTE PAS CE QU'ON A DEMANDÉ N'EST PAS L'ÉCHO : un autre
  // lot peut nous nommer entre-temps — une entrée en vue, par exemple.
  if (worn.book_revision != asked.book_revision &&
      Echoed(asked.kind, asked.body_key, asked.sent, worn.mine)) {
    Settle(asked);
    pending_.reset();
    Announce(StyleSendState::kShared, now_ms);
    return;
  }
  if (now_ms - asked.sent_ms >= kEchoWaitMs) {
    pending_.reset();
    Announce(StyleSendState::kRefused, now_ms);
  }
}

void StyleSession::HearBodyVerdict(const ro::sim::StyleVerdict& verdict, std::uint32_t now_ms) {
  // ⚠ UN VERDICT VENU AVANT NOTRE DEMANDE N'EST PAS LE SIEN.
  if (!pending_.has_value() || verdict.sequence <= pending_->verdict_floor) {
    return;
  }
  switch (verdict.result) {
    case ro::sim::StyleVerdictResult::kAccepted:
      // Retenu : l'écho, qui suit, dit ce qui est porté.
      return;
    case ro::sim::StyleVerdictResult::kSame:
      // Rien n'a changé, et aucun écho ne viendra : c'est déjà partagé.
      Settle(*pending_);
      pending_.reset();
      body_refusal_.reset();
      Announce(StyleSendState::kShared, now_ms);
      return;
    default:
      pending_.reset();
      body_refusal_ = verdict.result;
      Announce(StyleSendState::kRefused, now_ms);
      return;
  }
}

void StyleSession::HearLayers(const Worn& worn, std::uint32_t now_ms) {
  layers_.cadence_ms = worn.layers_cadence_ms;
  const ro::sim::StyleVerdict& verdict = worn.layers_verdict;
  if (verdict.sequence != layers_.last_verdict) {
    layers_.last_verdict = verdict.sequence;
    if (layers_.pending && verdict.sequence > layers_.verdict_floor) {
      layers_.pending = false;
      const bool kept = verdict.result == ro::sim::StyleVerdictResult::kAccepted ||
                        verdict.result == ro::sim::StyleVerdictResult::kSame;
      layers_.refusal =
          kept ? std::nullopt : std::optional<ro::sim::StyleVerdictResult>(verdict.result);
      layers_.state = kept ? StyleSendState::kShared : StyleSendState::kRefused;
      layers_.announced_ms = Stamp(now_ms);
    }
  }
  if (layers_.pending && now_ms - layers_.sent_ms >= kEchoWaitMs) {
    layers_.pending = false;
    layers_.refusal.reset();
    layers_.state = StyleSendState::kRefused;
    layers_.announced_ms = Stamp(now_ms);
  }
  const std::uint32_t shown_ms = now_ms - layers_.announced_ms;
  if ((layers_.state == StyleSendState::kShared && shown_ms >= kSharedNoticeMs) ||
      (layers_.state == StyleSendState::kRefused && shown_ms >= kRefusedNoticeMs)) {
    layers_.state = StyleSendState::kNone;
  }
}

bool StyleSession::CanSendLayers(std::uint32_t now_ms) const {
  if (layers_.cadence_ms == 0 || layers_.pending) {
    return false;
  }
  return layers_.sent_ms == 0 || now_ms - layers_.sent_ms >= layers_.cadence_ms;
}

void StyleSession::SentLayers(bool left, std::uint32_t now_ms) {
  layers_.refusal.reset();
  layers_.announced_ms = Stamp(now_ms);
  if (!left) {
    layers_.pending = false;
    layers_.state = StyleSendState::kRefused;
    return;
  }
  layers_.pending = true;
  layers_.sent_ms = Stamp(now_ms);
  layers_.verdict_floor = layers_.last_verdict;
  layers_.state = StyleSendState::kWaiting;
}

void StyleSession::Settle(const Pending& asked) {
  // 🔴 LE VERROU NE SE LÈVE QUE SUR UN CORPS QU'ON N'A PAS RETOUCHÉ DEPUIS LA
  // DEMANDE : ce qu'on a composé entre-temps n'est pas ce que le serveur a
  // retenu, et le suivre l'effacerait.
  const auto release = [this, &asked](std::uint32_t body_key) {
    const auto found = slots_.find(body_key);
    if (found != slots_.end() && found->second.changed_at <= asked.at_revision) {
      found->second.touched = false;
    }
    // Le brouillon ne survit ni à une validation, ni à un effacement.
    drafts_changed_ = drafts_.Forget(body_key) || drafts_changed_;
  };
  if (asked.kind != StyleSendKind::kDeleteAll) {
    release(asked.body_key);
    return;
  }
  for (const auto& slot : slots_) {
    release(slot.first);
  }
  for (const std::uint32_t body_key : asked.bodies) {
    release(body_key);
  }
}

void StyleSession::Announce(StyleSendState state, std::uint32_t now_ms) {
  send_state_ = state;
  announced_ms_ = Stamp(now_ms);
}

void StyleSession::SetPiece(std::size_t rank, const ro::render::RampAdjust& adjust,
                            std::uint32_t stroke) {
  const PieceEdit edit{.rank = rank, .adjust = adjust};
  SetPieces(std::span<const PieceEdit>(&edit, 1), stroke);
}

void StyleSession::SetPieces(std::span<const PieceEdit> edits, std::uint32_t stroke) {
  ro::render::PaletteRecipe next = recipe();
  for (const PieceEdit& edit : edits) {
    if (edit.rank < ro::render::kMaxRamps) {
      next.ramps[edit.rank] = edit.adjust;
    }
  }
  Gesture(next, stroke);
}

void StyleSession::ChooseHairStyle(std::int16_t hair_style) {
  ro::render::PaletteRecipe next = recipe();
  next.hair_style = hair_style;
  Gesture(next, 0);
}

void StyleSession::ChooseHairPalette(std::int16_t hair_palette) {
  ro::render::PaletteRecipe next = recipe();
  next.hair_palette = hair_palette;
  Gesture(next, 0);
}

void StyleSession::ChooseBodyPalette(std::int16_t body_palette) {
  ro::render::PaletteRecipe next = recipe();
  next.body_palette = body_palette;
  Gesture(next, 0);
}

void StyleSession::Load(const ro::render::PaletteRecipe& recipe) { Gesture(recipe, 0); }

bool StyleSession::LoadDraft() {
  const ro::sim::StyleRecipe* kept = drafts_.Find(body_key_);
  if (kept == nullptr) {
    return false;
  }
  Gesture(FromWire(*kept), 0);
  return true;
}

void StyleSession::BlankSlate() { Gesture(ro::render::PaletteRecipe{}, 0); }

void StyleSession::BackToWorn() {
  KeepDraft();
  Slot& slot = Current();
  if (!SameRecipe(slot.recipe, applied_)) {
    Remember(slot);
    slot.redo.clear();
    slot.stroke = 0;
  }
  slot.touched = false;
  Follow();
}

std::optional<ro::render::PaletteRecipe> StyleSession::Validate(std::int16_t worn_hair_style,
                                                                std::uint32_t now_ms,
                                                                bool make_main) {
  ro::render::PaletteRecipe sent = recipe();
  if (sent.hair_style <= 0 && worn_hair_style > 0) {
    sent.hair_style = worn_hair_style;
    Put(sent);
  }
  if (own_style_ && !has_edits() && (!make_main || main_)) {
    pending_.reset();
    Current().touched = false;
    drafts_changed_ = drafts_.Forget(body_key_) || drafts_changed_;
    Announce(StyleSendState::kShared, now_ms);
    return std::nullopt;
  }
  return sent;
}

void StyleSession::Sent(StyleSendKind kind, bool left, std::uint32_t now_ms) {
  if (!left) {
    pending_.reset();
    Announce(StyleSendState::kRefused, now_ms);
    return;
  }
  pending_ = Pending{.kind = kind,
                     .body_key = body_key_,
                     .at_revision = revision_,
                     .book_revision = book_revision_,
                     .sent = recipe(),
                     .bodies = worn_bodies_,
                     .sent_ms = Stamp(now_ms),
                     .verdict_floor = body_verdict_seen_};
  body_refusal_.reset();
  last_sent_ms_ = Stamp(now_ms);
  Announce(StyleSendState::kWaiting, now_ms);
}

void StyleSession::KeepDraft() {
  if (body_key_ == 0 || !has_edits()) {
    return;
  }
  drafts_changed_ = drafts_.Put(body_key_, ToWire(recipe())) || drafts_changed_;
}

void StyleSession::TickDraft(std::uint32_t now_ms) {
  if (draft_revision_ != revision_) {
    // ⚠ L'HORLOGE REPART À CHAQUE CHANGEMENT : c'est l'IMMOBILITÉ qu'on attend,
    // pas un délai depuis le premier geste. Un glissement continu ne doit rien
    // écrire tant qu'il dure.
    draft_revision_ = revision_;
    draft_since_ms_ = Stamp(now_ms);
    return;
  }
  if (draft_since_ms_ == 0 || now_ms - draft_since_ms_ < kDraftDelayMs) {
    return;
  }
  draft_since_ms_ = 0;
  KeepDraft();
}

bool StyleSession::TakeDraftsChanged() { return std::exchange(drafts_changed_, false); }

}  // namespace moonclient
