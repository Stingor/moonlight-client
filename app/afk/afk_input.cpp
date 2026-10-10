#include "afk/afk_input.h"

#include "afk/afk_settings.h"

namespace moonclient {
namespace {

// Ce qu'un genre de geste dit de lui-même, une fois pour toutes.
struct GestureTraits {
  bool from_key = false;
  bool from_mouse = false;
  // Un RELÂCHEMENT ne réveille jamais : c'est la fin d'un geste commencé avant,
  // pas une intention neuve. Sans cette règle, le raccourci qui LANCE la veille
  // la terminerait de son propre relâchement, une image plus tard.
  bool release = false;
};

[[nodiscard]] GestureTraits TraitsOf(AfkInputKind kind) noexcept {
  switch (kind) {
    case AfkInputKind::kKeyPress:
    case AfkInputKind::kTextInput:
      return {.from_key = true};
    case AfkInputKind::kKeyRelease:
      return {.from_key = true, .release = true};
    case AfkInputKind::kMouseMove:
    case AfkInputKind::kMouseWheel:
    case AfkInputKind::kMousePress:
      return {.from_mouse = true};
    case AfkInputKind::kMouseRelease:
      return {.from_mouse = true, .release = true};
  }
  return {};
}

}  // namespace

std::uint32_t AfkIdle::MaskOf(std::uint8_t button) noexcept {
  if (button == 0) {
    return 0;
  }
  // ⚠ LES BOUTONS LATÉRAUX COMPTENT AUSSI : un pouce sur le bouton « précédent »
  // repousse l'endormissement comme un autre clic. Ils arrivent dans le même
  // événement ; les écarter serait un trou qu'il faudrait écrire exprès.
  return 1U << (button - 1U);
}

void AfkIdle::SetSleeping(bool sleeping) noexcept {
  sleeping_ = sleeping;
  if (!sleeping) {
    return;
  }
  // En entrant en veille on repart d'un masque propre : un bouton resté marqué
  // d'une veille précédente ferait avaler le premier relâchement de celle-ci.
  swallowed_ = 0;
}

std::uint32_t AfkIdle::IdleMs(std::uint32_t now_ms) const noexcept {
  return any_input_ ? now_ms - last_input_ms_ : 0U;
}

bool AfkIdle::WokeSince(std::uint32_t instant_ms) const noexcept {
  // ⚠ LA SOUSTRACTION EST SIGNÉE parce qu'une horloge de millisecondes repasse
  // par zéro — tous les quarante-neuf jours pour celle du système, et un client
  // laissé ouvert, ça arrive.
  return any_wake_ && static_cast<std::int32_t>(last_wake_ms_ - instant_ms) > 0;
}

bool AfkIdle::Note(const AfkInput& input, std::uint32_t now_ms) {
  if (input.kind == AfkInputKind::kMouseMove) {
    if (any_move_ && input.x == last_x_ && input.y == last_y_) {
      return false;  // pas un mouvement
    }
    last_x_ = input.x;
    last_y_ = input.y;
    any_move_ = true;
  }

  const GestureTraits traits = TraitsOf(input.kind);
  const bool was_sleeping = sleeping_;
  // 🔴 LE DROIT DE RÉVEILLER S'ACCORDE À LA SOURCE, PAS AU MESSAGE : c'est ce
  // qui garde l'avalage symétrique. Avaler un appui sans avaler son relâchement
  // laisserait le client croire la touche encore enfoncée — un personnage qui
  // marche tout seul, et personne pour faire le rapprochement.
  const bool source_may_wake =
      wake_mode_ == static_cast<int>(AfkWake::kAny) ||
      (wake_mode_ == static_cast<int>(AfkWake::kKeyboard) && traits.from_key) ||
      (wake_mode_ == static_cast<int>(AfkWake::kMouse) && traits.from_mouse);

  const std::uint32_t button = MaskOf(input.button);
  if (input.kind == AfkInputKind::kMouseRelease && button != 0) {
    // Le relâchement suit le sort de son appui, veille ou pas : c'est ce qui
    // rend l'avalage symétrique même si le réveil a eu lieu entre les deux.
    //
    // 🔴 ET UN RELÂCHEMENT AVALÉ NE COMPTE PAS COMME ACTIVITÉ. Ce n'est pas un
    // raffinement : c'est ce qui rend le bouton « Essayer maintenant »
    // utilisable. Son clic endort le client entre l'appui et le relâchement, et
    // ce relâchement-là — le second temps d'un geste déjà fini — rouvrirait les
    // yeux aussitôt fermés. La veille durerait une image, et le bouton passerait
    // pour cassé.
    if ((swallowed_ & button) == 0) {
      last_input_ms_ = now_ms;
      any_input_ = true;
      return false;
    }
    swallowed_ &= ~button;
    return true;
  }

  last_input_ms_ = now_ms;  // repousse l'endormissement : TOUT compte
  any_input_ = true;
  if (source_may_wake && !traits.release) {
    last_wake_ms_ = now_ms;  // réveille : seulement ce qui en a le droit
    any_wake_ = true;
  }

  if (!was_sleeping) {
    return false;  // hors veille, rien n'est jamais avalé
  }

  // ── EN VEILLE : CE QUI N'A PAS LE DROIT DE RÉVEILLER N'A PAS NON PLUS LE
  // DROIT D'AGIR ───────────────────────────────────────────────────────────
  // Une touche qui traverserait sans réveiller lancerait un sort sur un monde
  // que le joueur ne voit pas, sous un angle qui n'est pas le sien. Demander
  // « seule la souris me réveille », c'est demander que le clavier soit sans
  // effet — pas qu'il agisse en aveugle.
  if (!source_may_wake) {
    return true;
  }
  // Le clic qui réveille est avalé, lui aussi, et pour une raison qui lui est
  // propre : le décor a tourné, il tomberait ailleurs que là où l'on croit
  // viser. Le clavier, lui, passe — une touche porte une intention explicite.
  if (button == 0) {
    return false;
  }
  swallowed_ |= button;
  return true;
}

}  // namespace moonclient
