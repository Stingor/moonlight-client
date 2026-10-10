#include "world/quick_cast.h"

namespace moonclient {

QuickCastAim QuickCastAimOf(const ro::sim::KnownSkill& skill) {
  switch (skill.targeting) {
    case ro::sim::kSkillOnGround:
      return QuickCastAim::kGround;
    case ro::sim::kSkillOnEnemy:
      return QuickCastAim::kOffensive;
    case ro::sim::kSkillOnFriend:
      return QuickCastAim::kSupport;
    default:
      return QuickCastAim::kNative;
  }
}

void QuickCast::NotePress(std::uint32_t key, std::uint32_t now_ms) {
  pending_key_ = key;
  pending_key_ms_ = now_ms;
}

std::uint32_t QuickCast::TakePendingKey(const QuickCastHands& hands, std::uint32_t now_ms) {
  const std::uint32_t key = pending_key_;
  pending_key_ = 0;
  if (key == 0 || now_ms - pending_key_ms_ > kQuickCastPendingKeyLifetimeMs ||
      !hands.KeyHeld(key)) {
    return 0;
  }
  return key;
}

bool QuickCast::CanCastNow(const QuickCastHands& hands) const {
  // Quatre interrupteurs mènent ici, dont deux qui ne sont pas à nous : le mode
  // Ciblage ou la grille de groupe peuvent fournir la visée à eux seuls, Quick
  // cast éteint par ailleurs.
  if (!look_->ground && !look_->target && !hands.HudCastArmed() && !hands.TileCastArmed()) {
    return false;
  }
  // Jeu au second plan : la touche est peut-être toujours enfoncée, mais elle
  // ne s'adresse plus à lui.
  return hands.InPlay() && hands.HasFocus();
}

bool QuickCast::ClaimsAim(QuickCastAim aim, const QuickCastHands& hands) const {
  switch (aim) {
    case QuickCastAim::kGround:
      return look_->ground;
    case QuickCastAim::kOffensive:
      return look_->target || hands.HudCastArmed();
    // La grille ne vise que pour le soutien : elle ne prend donc pas la main sur
    // un sort offensif, dont le cercle doit rester au clic.
    case QuickCastAim::kSupport:
      return look_->target || hands.HudCastArmed() || hands.TileCastArmed();
    case QuickCastAim::kNative:
      return false;
  }
  return false;
}

bool QuickCast::TooSoon(std::uint32_t now_ms) const {
  const std::uint32_t period_ms =
      look_->repeat_ms > 0 ? static_cast<std::uint32_t>(look_->repeat_ms) : 0U;
  return last_cast_ms_.has_value() && now_ms - *last_cast_ms_ < period_ms;
}

bool QuickCast::EmitCast(QuickCastHands& hands, const ro::sim::KnownSkill& skill) const {
  // Le serveur refuserait : inutile d'émettre. Cela ne couvre que les sorts qui
  // ont un cooldown — les autres sont bornés par le délai d'après-incantation,
  // que le serveur ne transmet pas, d'où la cadence en complément.
  if (hands.CoolingDown(skill)) {
    return false;
  }
  const QuickCastAim aim = QuickCastAimOf(skill);
  // Ce qui vise un piège posé, et ce que le jeu arme à part : le ciblage ordinaire reste entier.
  if (aim == QuickCastAim::kNative) {
    return false;
  }
  const bool mouse_on_world = hands.MouseOnWorld();
  if (aim == QuickCastAim::kGround) {
    if (!look_->ground || !mouse_on_world) {
      return false;
    }
    const std::optional<ro::sim::CellPosition> cell = hands.GroundUnderMouse();
    return cell.has_value() && hands.CastOnGround(skill, *cell);
  }
  // La tuile pour le soutien, la souris ensuite, la cible du mode Ciblage en
  // dernier : voir l'en-tête.
  std::uint32_t id = 0;
  if (aim == QuickCastAim::kSupport && hands.TileCastArmed()) {
    id = hands.TileTarget();
  }
  if (id == 0 && look_->target && mouse_on_world) {
    id = hands.ActorUnderMouse(aim);
  }
  if (id == 0) {
    id = hands.HudTarget(aim);
  }
  return id != 0 && hands.CastOnActor(skill, id);
}

void QuickCast::StartRepeat(std::uint32_t key, const ro::sim::KnownSkill& skill) {
  repeat_key_ = key;
  repeat_skill_ = skill;
}

void QuickCast::OnEnterTargeting(QuickCastHands& hands, std::uint32_t now_ms) {
  // Un armement consomme la frappe en attente, qu'il aboutisse ou non.
  const std::uint32_t key = TakePendingKey(hands, now_ms);
  // Un nouvel armement annule la relance précédente.
  repeat_key_ = 0;
  if (!CanCastNow(hands)) {
    return;
  }
  const std::optional<ro::sim::KnownSkill> armed = hands.Armed();
  if (!armed.has_value()) {
    return;
  }
  // 🔴 LU AVANT LA CADENCE : c'est lui qui dit si le cercle qui vient
  // d'apparaître nous appartient, et un refus par la cadence doit quand même le
  // faire surveiller.
  const QuickCastAim aim = QuickCastAimOf(*armed);
  const bool claimed = ClaimsAim(aim, hands);
  // « Encore tenue », et non « déjà vue » : une touche relâchée dont le retrait
  // n'a pas encore eu lieu ne doit pas s'attribuer un armement fait à la souris.
  const bool still_held = arm_key_ != 0 && hands.KeyHeld(arm_key_);
  if (claimed && (key != 0 || still_held)) {
    if (key != 0) {
      arm_key_ = key;
    }
    arm_aim_ = aim;
  } else if (!claimed) {
    arm_key_ = 0;
  }
  // Reste un cas SANS surveillance, et c'est voulu : un genre de visée à nous,
  // aucune touche — un armement au clic. Son cercle attend le clic, comme dans
  // le jeu.

  const std::uint32_t held = key != 0 ? key : (still_held ? arm_key_ : 0);
  if (TooSoon(now_ms) || !EmitCast(hands, *armed)) {
    // Rien n'est parti. La relance s'arme QUAND MÊME : c'est ce qui laisse un
    // lancement refusé sur l'instant aboutir sans relâcher.
    if (held != 0 && claimed) {
      StartRepeat(held, *armed);
    }
    return;
  }
  last_cast_ms_ = now_ms;
  hands.Disarm();
  // ⚠ `arm_key_` RESTE : la touche est toujours tenue, elle réarmera un sort
  // dans un instant, et il faut encore quelqu'un pour le retirer.
  if (held != 0) {
    StartRepeat(held, *armed);
  }
}

void QuickCast::UpdateDisarm(QuickCastHands& hands) {
  if (arm_key_ == 0 || hands.KeyHeld(arm_key_)) {
    return;
  }
  const QuickCastAim armed_aim = arm_aim_;
  arm_key_ = 0;
  if (!hands.InPlay()) {
    return;
  }
  // Plus rien d'armé : le sort a fini par partir. Ou autre chose est armé
  // depuis, et l'effacer volerait au joueur une visée qu'il vient de demander.
  const std::optional<ro::sim::KnownSkill> armed = hands.Armed();
  if (!armed.has_value() || QuickCastAimOf(*armed) != armed_aim) {
    return;
  }
  hands.Disarm();
}

void QuickCast::Update(QuickCastHands& hands, std::uint32_t now_ms) {
  if (repeat_key_ == 0) {
    return;
  }
  // Relâchée : fin de la relance, avant toute autre garde, pour que l'état
  // s'oublie même quand une autre bloque le lancement.
  if (!hands.KeyHeld(repeat_key_)) {
    repeat_key_ = 0;
    return;
  }
  if (TooSoon(now_ms) || !CanCastNow(hands)) {
    return;
  }
  // Une incantation en cours : la relance attendrait, autant ne rien émettre.
  // Seulement ici — un appui volontaire n'est jamais bloqué par cette lecture.
  if (hands.Casting()) {
    return;
  }
  // La visée est RÉÉVALUÉE : au sol la case suit le curseur, et un monstre
  // suivi du curseur enchaîne les lancements. Plus rien de visé : on reste prêt.
  if (EmitCast(hands, repeat_skill_)) {
    last_cast_ms_ = now_ms;
  }
}

void QuickCast::OnUseItemSlot(QuickCastHands& hands, std::size_t slot, std::uint32_t item_id,
                              std::uint32_t now_ms) {
  // La frappe est consommée même relance éteinte : elle a servi à cet usage-là.
  const std::uint32_t key = TakePendingKey(hands, now_ms);
  // Sans touche, c'est un clic sur la case : un seul usage, rien à répéter.
  if (!look_->item || key == 0) {
    return;
  }
  item_key_ = key;
  item_slot_ = slot;
  item_id_ = item_id;
  last_item_ms_ = now_ms;
}

void QuickCast::UpdateItemRepeat(QuickCastHands& hands, std::uint32_t now_ms) {
  if (item_key_ == 0) {
    return;
  }
  // Relâchée, réglage éteint, jeu au second plan, ou frappe partie dans un
  // champ de texte : on arrête net, plutôt que de vider l'Inventory derrière.
  if (!hands.KeyHeld(item_key_) || !look_->item || !hands.HasFocus() || hands.Typing()) {
    item_key_ = 0;
    return;
  }
  // Une carte qui se charge : on patiente, la touche est toujours tenue.
  if (!hands.InPlay()) {
    return;
  }
  const auto period_ms = static_cast<std::uint32_t>(
      ro::ui::QuickCastItemPeriodMs(look_->item_repeat_ms, hands.Staff()));
  if (now_ms - last_item_ms_ < period_ms) {
    return;
  }
  if (!hands.RepeatItem(item_slot_, item_id_)) {
    item_key_ = 0;
    return;
  }
  last_item_ms_ = now_ms;
}

void QuickCast::Advance(QuickCastHands& hands, const QuickCastSignals& seen, std::uint32_t now_ms) {
  if (seen.arms != arms_seen_) {
    arms_seen_ = seen.arms;
    OnEnterTargeting(hands, now_ms);
  }
  if (seen.item_uses != item_uses_seen_) {
    item_uses_seen_ = seen.item_uses;
    OnUseItemSlot(hands, seen.item_slot, seen.item_id, now_ms);
  }
  UpdateDisarm(hands);
  Update(hands, now_ms);
  UpdateItemRepeat(hands, now_ms);
}

void QuickCast::Forget() {
  repeat_key_ = 0;
  pending_key_ = 0;
  item_key_ = 0;
  arm_key_ = 0;
}

}  // namespace moonclient
