#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/sim/packed_position.h"
#include "ro/sim/skill_book.h"
#include "ro/ui/quick_cast_look.h"

namespace moonclient {

// ── QUICK CAST : LANCER UN SORT EN UNE ACTION ───────────────────────────────
//
// Sans lui, un sort de zone ou ciblé demande deux gestes : la touche l'ARME,
// puis le CLIC désigne. Quick cast supprime le second : dès qu'un sort s'arme,
// la visée se résout d'elle-même et le sort part. Et il le RELANCE tant que la
// touche reste tenue, à la cadence réglée. Il répète de même une case d'item.
//
// Tout ce qui compte reste le chemin ordinaire : le sort part par le grimoire,
// comme sur un clic — approche de la cible comprise —, et le serveur reste seul
// juge de ce qui est légal (SP, portée, cooldown, PVP).
//
// ── LES SOURCES DE VISÉE, ET LEUR ORDRE ─────────────────────────────────────
//
// Un sort AU SOL vise une case, et il n'y en a qu'une source : la souris. Si
// elle ne désigne pas le monde — une fenêtre dessous, le ciel —, rien ne part,
// et le sort reste armé : le clic ordinaire reprend la main.
//
// Un sort CIBLÉ a trois sources. 🔴 LA TUILE DE LA GRILLE DE GROUPE D'ABORD,
// pour un sort de SOUTIEN seulement : elle est dessinée par-dessus le monde, et
// le curseur qui la vise ne vise pas ce qui passe dessous. Jamais pour un sort
// offensif : une tuile est survolée en permanence pendant qu'on soigne. 🔴 PUIS
// LA SOURIS : viser quelqu'un du curseur est un geste explicite, il ne doit
// jamais se faire voler par une cible mémorisée. La cible du mode Ciblage ne répond que là où la
// souris ne dit rien — curseur dans le vide, sur une fenêtre, ou visée à la souris éteinte. C'est
// aussi pourquoi le curseur ne se teste que pour les visées à la souris : refuser de lancer sur la
// cible du HUD parce que le curseur passe sur une fenêtre serait le contraire de ce que ce réglage
// promet.
//
// ── UNE TOUCHE NE LAISSE JAMAIS DE CERCLE DERRIÈRE ELLE ─────────────────────
//
// Quick cast peut ne pas lancer — cadence pas écoulée, cooldown, rien sous le
// curseur. Le sort reste alors armé, et c'est un PIÈGE : le clic suivant, qui
// visait autre chose, lancerait le sort oublié. Deux réponses :
//   · la RÉPÉTITION s'arme même quand le premier lancement échoue : une cadence
//     ou un cooldown qui se lève finit par lancer sans avoir à relâcher ;
//   · au RELÂCHEMENT, ce qui reste armé par cette touche est retiré.
//
// 🔴 SEULEMENT POUR UNE VISÉE ARMÉE PAR UNE TOUCHE, et dans un mode que Quick
// cast prend en charge. Armer un sort en CLIQUANT sa case ne pose aucune
// touche : le déroulé « ça arme, je clique ma cible » reste entier.
//
// ⚠ ET « ARMÉE PAR UNE TOUCHE » VEUT DIRE « TANT QU'ELLE EST TENUE », pas « vue
// à l'instant » : la barre réarme le sort sur la répétition du clavier, et ce
// second armement n'arrive avec aucune frappe fraîche. S'il n'était gardé par
// personne, c'est lui qui resterait armé au relâchement.

// Ce que vise un sort : une case, un adversaire, un allié — ou rien que Quick
// cast prenne en charge.
//
// 🔴 C'EST LA VALEUR EXACTE DE LA VISÉE QUI TRANCHE, comme dans le jeu : il
// choisit son mode de ciblage sur elle, pas sur ses bits. Une compétence qui
// VISE UN PIÈGE POSÉ (Remove Trap, Spring Trap) s'annonce par sa seule valeur
// de piège, et le jeu l'arme dans un mode à part que Quick cast laisse tel quel :
// elle se désigne au clic sur le piège, comme avant.
enum class QuickCastAim : std::uint8_t { kGround, kOffensive, kSupport, kNative };
[[nodiscard]] QuickCastAim QuickCastAimOf(const ro::sim::KnownSkill& skill);

// Une frappe n'annonce que l'action qu'elle déclenche dans la foulée. Au-delà,
// la touche est TENUE pour autre chose, et ne doit pas s'attribuer un armement
// fait à la souris entre-temps.
inline constexpr std::uint32_t kQuickCastPendingKeyLifetimeMs = 250;

// CE QUE QUICK CAST DEMANDE AU JEU : lire l'état, résoudre la visée, lancer.
//
// ⚠ UNE INTERFACE, ET NON LE JEU LUI-MÊME : la machine ci-dessous ne connaît ni
// la caméra, ni la connexion, ni les fenêtres. C'est ce qui la rend essayable
// sans rien de tout cela.
class QuickCastHands {
 public:
  QuickCastHands() = default;
  QuickCastHands(const QuickCastHands&) = delete;
  QuickCastHands& operator=(const QuickCastHands&) = delete;
  QuickCastHands(QuickCastHands&&) = delete;
  QuickCastHands& operator=(QuickCastHands&&) = delete;
  virtual ~QuickCastHands() = default;

  // En partie, carte chargée.
  [[nodiscard]] virtual bool InPlay() const = 0;
  // ⚠ LA FENÊTRE DU JEU A LE CLAVIER : une touche tenue ne s'adresse plus à
  // lui quand on est passé à une autre application.
  [[nodiscard]] virtual bool HasFocus() const = 0;
  // Un champ de texte attend des caractères.
  [[nodiscard]] virtual bool Typing() const = 0;
  // Le droit du compte : voir `ro::ui::QuickCastItemFloorMs`.
  [[nodiscard]] virtual bool Staff() const = 0;
  [[nodiscard]] virtual bool KeyHeld(std::uint32_t key) const = 0;

  // Le sort armé, par copie : le grimoire peut se réarranger sous un pointeur.
  [[nodiscard]] virtual std::optional<ro::sim::KnownSkill> Armed() const = 0;
  virtual void Disarm() = 0;
  [[nodiscard]] virtual bool CoolingDown(const ro::sim::KnownSkill& skill) const = 0;
  // Le personnage incante : une relance attendrait de toute façon la fin.
  [[nodiscard]] virtual bool Casting() const = 0;

  // La souris désigne-t-elle le MONDE, et non une fenêtre ?
  [[nodiscard]] virtual bool MouseOnWorld() const = 0;
  // La case franchissable sous la souris, s'il y en a une.
  [[nodiscard]] virtual std::optional<ro::sim::CellPosition> GroundUnderMouse() const = 0;
  // L'entité sous la souris qu'un sort de ce genre peut viser, ou zéro.
  [[nodiscard]] virtual std::uint32_t ActorUnderMouse(QuickCastAim aim) const = 0;
  // Le mode Ciblage répond-il à « sur qui ? » ? Voir `TargetingSettings`.
  [[nodiscard]] virtual bool HudCastArmed() const = 0;
  // Sa cible, pour un sort de ce genre, ou zéro.
  [[nodiscard]] virtual std::uint32_t HudTarget(QuickCastAim aim) const = 0;
  // La grille de groupe répond-elle à « sur qui ? » : affichée, et son réglage
  // de lancer sur la tuile survolée coché.
  [[nodiscard]] virtual bool TileCastArmed() const = 0;
  // Le membre dont la tuile est sous le curseur et qu'un sort peut viser, ou
  // zéro. ⚠ LA MACHINE NE LE LIT QUE POUR UN SORT DE SOUTIEN.
  [[nodiscard]] virtual std::uint32_t TileTarget() const = 0;

  virtual bool CastOnGround(const ro::sim::KnownSkill& skill, ro::sim::CellPosition where) = 0;
  virtual bool CastOnActor(const ro::sim::KnownSkill& skill, std::uint32_t id) = 0;
  // Réutilise la case `slot`. Faux quand il n'y a plus rien à répéter : la case
  // ne porte plus cet item, ou l'Inventory n'en a plus.
  virtual bool RepeatItem(std::size_t slot, std::uint32_t item_id) = 0;
};

// CE QUE LE JEU A COMPTÉ DEPUIS LE DÉBUT : les armements du grimoire, les usages
// d'une case d'item de la barre, et la dernière case servie. Un compte qui a
// bougé depuis l'image précédente dit qu'un sort s'est armé, ou qu'un item a
// servi — d'où que vienne le geste.
struct QuickCastSignals {
  std::uint32_t arms = 0;
  std::uint32_t item_uses = 0;
  std::size_t item_slot = 0;
  std::uint32_t item_id = 0;
};

class QuickCast {
 public:
  explicit QuickCast(const ro::ui::QuickCastLook& look) : look_(&look) {}

  // UNE IMAGE, dans l'ordre : le sort qui vient de s'armer, l'item qui vient de
  // servir, le cercle d'une touche relâchée, puis les deux relances.
  void Advance(QuickCastHands& hands, const QuickCastSignals& seen, std::uint32_t now_ms);

  // Une touche vient d'être ENFONCÉE, pas répétée. Retenue pour l'action
  // qu'elle déclenche dans la foulée : c'est ce qui distingue le clavier de la
  // souris.
  void NotePress(std::uint32_t key, std::uint32_t now_ms);

  // Un sort vient de s'ARMER, d'où qu'il vienne.
  void OnEnterTargeting(QuickCastHands& hands, std::uint32_t now_ms);

  // Une case d'item vient de servir, d'où que vienne le geste. Arme la
  // répétition si la frappe qui l'a déclenchée est toujours tenue.
  void OnUseItemSlot(QuickCastHands& hands, std::size_t slot, std::uint32_t item_id,
                     std::uint32_t now_ms);

  // Retire le cercle qu'une touche RELÂCHÉE a laissé derrière elle.
  void UpdateDisarm(QuickCastHands& hands);
  // Relance le sort retenu tant que sa touche reste tenue.
  void Update(QuickCastHands& hands, std::uint32_t now_ms);
  // Réutilise la case d'item retenue tant que sa touche reste tenue.
  void UpdateItemRepeat(QuickCastHands& hands, std::uint32_t now_ms);

  // Le changement de carte : il n'y a plus de cercle à retirer, ni rien à
  // relancer.
  void Forget();
  // L'arrivée sur la MÊME carte : le cercle et la relance de sort tombent avec
  // le grimoire désarmé, la frappe en attente aussi.
  //
  // 🔴 L'ITEM TENU CONTINUE. Une aile de mouche qui part de la case EST cette
  // arrivée : l'oublier ici arrêterait la touche tenue au premier téléport.
  void ForgetAim();

  [[nodiscard]] std::uint32_t arm_key() const noexcept { return arm_key_; }
  [[nodiscard]] std::uint32_t repeat_key() const noexcept { return repeat_key_; }
  [[nodiscard]] std::uint32_t item_key() const noexcept { return item_key_; }

 private:
  // Gardes communes au premier lancement et à chaque relance.
  [[nodiscard]] bool CanCastNow(const QuickCastHands& hands) const;
  // Prend, et efface, la frappe en attente ; zéro si elle a expiré ou n'est
  // plus tenue. ⚠ CONSOMMÉE DANS TOUS LES CAS : une frappe ne vaut que pour
  // l'action qu'elle amène.
  [[nodiscard]] std::uint32_t TakePendingKey(const QuickCastHands& hands, std::uint32_t now_ms);
  // Ce genre de visée est-il le NÔTRE ? C'est ce qui décide si une touche a le
  // droit de laisser un cercle derrière elle.
  [[nodiscard]] bool ClaimsAim(QuickCastAim aim, const QuickCastHands& hands) const;
  // Résout la visée et lance. Vrai si le sort est parti.
  bool EmitCast(QuickCastHands& hands, const ro::sim::KnownSkill& skill) const;
  [[nodiscard]] bool TooSoon(std::uint32_t now_ms) const;
  void StartRepeat(std::uint32_t key, const ro::sim::KnownSkill& skill);

  const ro::ui::QuickCastLook* look_;

  // Les comptes du jeu à l'image précédente : voir `QuickCastSignals`.
  std::uint32_t arms_seen_ = 0;
  std::uint32_t item_uses_seen_ = 0;

  std::optional<std::uint32_t> last_cast_ms_;
  // La dernière frappe fraîche, et quand : voir `kQuickCastPendingKeyLifetimeMs`.
  std::uint32_t pending_key_ = 0;
  std::uint32_t pending_key_ms_ = 0;

  // La touche responsable du cercle armé, et ce qu'il vise. Zéro : rien à
  // surveiller.
  std::uint32_t arm_key_ = 0;
  QuickCastAim arm_aim_ = QuickCastAim::kGround;

  // Le sort relancé tant que `repeat_key_` est tenue. Zéro : pas de relance —
  // un armement à la souris, par exemple.
  std::uint32_t repeat_key_ = 0;
  ro::sim::KnownSkill repeat_skill_;

  // L'item réutilisé tant que `item_key_` est tenue. L'identifiant est retenu
  // pour que la boucle s'arrête si la case change sous elle.
  std::uint32_t item_key_ = 0;
  std::size_t item_slot_ = 0;
  std::uint32_t item_id_ = 0;
  std::uint32_t last_item_ms_ = 0;
};

}  // namespace moonclient
