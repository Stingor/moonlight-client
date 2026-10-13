#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/net/connection.h"
#include "ro/render/font_atlas.h"
#include "ro/render/screen_overlay.h"
#include "ro/render/skill_panel.h"
#include "ro/sim/actor.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "world/armed_cursor.h"

// LE GRIMOIRE : la liste des compétences apprises, et de quoi les lancer.
namespace moonclient {

// UNE POSE AU SOL QUI ATTEND SON TEXTE : la compétence, son rang, et la case.
struct GroundCast {
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  ro::sim::CellPosition cell;
};

// La liste des compétences, et l'armement.
//
// LANCER SE FAIT EN DEUX TEMPS, comme dans le jeu : on choisit la compétence,
// puis on désigne ce qu'elle vise. Entre les deux elle est ARMÉE, et le clic
// suivant ne fait ni marcher ni frapper — c'est ce qui distingue « je lance
// Cold Bolt sur ce monstre » de « je vais le frapper ».
//
// Une compétence qui se lance SUR SOI ne s'arme pas : elle part au clic, il n'y
// a rien à désigner. Une PASSIVE ne part jamais.
//
// TOUT SE DIT dans la conversation — ce qui s'arme, ce qui part, ce qui est
// refusé. Un clic muet ne laisse aucun moyen de distinguer « la demande n'est
// pas partie » de « elle est partie et le serveur n'en a rien fait », et c'est
// la première question qu'on se pose quand rien n'apparaît.
// La visée qu'une compétence armée attend : une case, un piège posé, ou
// quelqu'un.
[[nodiscard]] SkillAim SkillAimOf(const ro::sim::KnownSkill& skill);

class SkillBookPanel {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }

  // Ouvre ou ferme. Fermer DÉSARME : une compétence qui attendrait sa cible
  // sous un panneau invisible détournerait le clic suivant sans rien montrer.
  void Toggle();

  [[nodiscard]] SkillAim aim() const;

  // La compétence armée, ou rien.
  [[nodiscard]] const ro::sim::KnownSkill* armed() const;

  void Disarm() { armed_.reset(); }

  // Les tables du client, d'où vient le NOM AFFICHÉ qu'écrivent les lignes du
  // grimoire. Nul tant qu'elles manquent : le nom interne les remplace.
  void UseSkillTree(const ro::sim::SkillTree* tree) noexcept { tree_ = tree; }

  // La molette. Vrai si le grimoire l'a PRISE : la caméra ne doit alors pas
  // reculer derrière lui.
  //
  // Sur le panneau ouvert, elle fait défiler la liste. Sans cela, les
  // compétences sous le bord du panneau sont INATTEIGNABLES — un prêtre en sait
  // plus que la fenêtre n'a de lignes, et rien à l'écran ne dit que la liste
  // continue.
  //
  // 🔴 AILLEURS, ELLE RÈGLE LE RANG DE CE QUI EST ARMÉ, comme dans le jeu : on
  // choisit la compétence, on dose, puis on désigne. Elle est PRISE dès qu'une
  // compétence attend sa cible, même quand le rang ne bouge pas — le client
  // coupe le zoom pour toute la durée du ciblage, et une vue qui recule au
  // milieu d'une visée serait une surprise. Voir `ArmedLevelAfterWheel`.
  //
  // `tree` peut être nul — une installation sans les tables du client garde un
  // jeu entier, moins ce réglage : rien ne dit alors quelle compétence se dose.
  bool Scroll(const ro::sim::World& world, const ro::sim::SkillTree* tree, glm::vec2 mouse_px,
              float wheel_steps, float viewport_width, float viewport_height, std::uint32_t now_ms);

  // CHOISIT UNE COMPÉTENCE : elle s'arme, ou part tout de suite quand il n'y a
  // rien à désigner, ou se refuse quand elle est passive.
  //
  // ⚠ PUBLIQUE PARCE QUE LA BARRE DE RACCOURCIS S'EN SERT AUSSI. Lancer se fait
  // en deux temps — choisir, puis désigner — et cette mécanique n'a aucune raison
  // d'exister deux fois : une case de la barre CHOISIT, exactement comme un clic
  // sur une ligne du panneau.
  void ChooseSkill(ro::net::Connection& link, ro::sim::World& world,
                   const ro::sim::KnownSkill& chosen, std::uint32_t now_ms);

  // Un clic dans la fenêtre. Vrai si le panneau l'a PRIS : le monde derrière
  // lui ne doit alors rien en faire.
  //
  // Sur une ligne, il arme la compétence — ou la lance aussitôt quand elle se
  // lance sur soi. Sur une ligne déjà armée, il désarme : c'est ainsi qu'on
  // renonce sans rien lancer.
  bool ClickInPanel(ro::net::Connection& link, ro::sim::World& world, glm::vec2 mouse_px,
                    float viewport_width, float viewport_height, std::uint32_t now_ms);

  // Un clic sur un ACTEUR, quand une compétence attend sa cible. Vrai s'il a
  // servi à viser, ou s'il a servi à renoncer.
  //
  // 🔴 IL N'ÉMET RIEN, et c'est le client natif qui le veut ainsi : un clic
  // sur une entité INSCRIT une action en attente, que le tic fait avancer
  // jusqu'à ce que la cible soit à portée. Émettre ici ferait refuser la
  // demande sans un mot dès qu'on lance de trop loin. Voir `AdvancePending`
  // et `ro/sim/pending_action.h`.
  bool ReleaseOnActor(const ro::sim::Actor& target);

  // Un clic sur une UNITÉ AU SOL, quand une compétence attend un piège : Remove
  // Trap, Spring Trap. Vrai s'il a servi à viser.
  //
  // Même poursuite qu'une visée d'acteur, l'unité pour cible : la demande part
  // à portée, par le paquet qui vise quelqu'un.
  bool ReleaseOnGroundUnit(const ro::sim::SkillUnit& unit);

  // Inscrit `skill` sur `target` sans passer par l'armement : c'est la visée
  // qui ne vient pas de la souris. Même poursuite que `ReleaseOnActor`.
  void CastOn(const ro::sim::KnownSkill& skill, const ro::sim::Actor& target);

  // Inscrit `skill` sur la case `where` sans passer par l'armement : la relance
  // de Quick cast. Même poursuite que `ReleaseOnGround` : le tic approche à
  // portée, puis la demande part.
  void CastOnGround(const ro::sim::KnownSkill& skill, ro::sim::CellPosition where);

  // Combien de fois une compétence s'est ARMÉE depuis l'ouverture. C'est ce
  // que Quick cast compare pour savoir qu'un sort attend sa cible, d'où qu'il
  // vienne — une touche, une case, une ligne du panneau.
  [[nodiscard]] std::uint32_t arms() const noexcept { return arms_; }

  // Le panneau ouvert couvre-t-il ce point ? Un curseur dessus ne désigne pas
  // le monde qu'il cache.
  [[nodiscard]] bool Covers(const ro::sim::World& world, glm::vec2 mouse_px, float viewport_width,
                            float viewport_height, std::uint32_t now_ms);

  // L'entité sur qui un sort vient de PARTIR, puis zéro. C'est le paquet qui
  // la nomme, quoi qu'en dise la désignation — un cadavre qu'on relève compris.
  [[nodiscard]] std::uint32_t TakeLaunchedTarget() noexcept {
    return std::exchange(launched_target_, 0);
  }

  // Combien de demandes de compétence SUR UNE ENTITÉ — soi compris — sont
  // parties depuis l'ouverture. Celles au sol n'y sont pas. C'est ce que la
  // barre de raccourcis compare pour savoir si la dernière touche a lancé :
  // voir `ShortcutBarView::Cast`.
  [[nodiscard]] std::uint32_t entity_casts() const noexcept { return entity_casts_; }

  // VRAI UNE FOIS après que `MC_CHANGECART` est parti : la liste des
  // charrettes s'ouvre avec lui. Voir `ro/sim/cart_choice.h`.
  [[nodiscard]] bool TakeCartChoiceAsked() noexcept {
    return std::exchange(cart_choice_asked_, false);
  }

  // UNE FOIS, la pose de Talkie Box que la fenêtre de saisie doit compléter :
  // la compétence ne part qu'avec son texte. Voir `CastOnGround`.
  [[nodiscard]] std::optional<GroundCast> TakeTalkboxAsked() noexcept {
    return std::exchange(talkbox_asked_, std::nullopt);
  }

  // L'HORLOGE DES MARCHES D'APPROCHE DU PERSONNAGE, qu'il tient pour l'attaque
  // aussi : les deux poursuites partagent son plancher. Voir
  // `ro::sim::ApproachWalkClock`.
  [[nodiscard]] ro::sim::ApproachWalkClock& approach_clock() noexcept { return approach_clock_; }

  // Un clic sur une CASE, quand une compétence attend sa cible. Vrai s'il a
  // servi à lancer, ou s'il a servi à renoncer — une compétence qui visait
  // quelqu'un se désarme sur un clic au sol, comme dans le jeu.
  bool ReleaseOnGround(ro::sim::World& world, ro::sim::CellPosition where, std::uint32_t now_ms);

  // Fait avancer l'action en attente d'une image : approcher, lancer, ou
  // abandonner. Sans rien en attente, elle ne fait rien.
  //
  // `walk` peut être nul — le temps qu'une carte se charge, il n'y a pas de
  // grille de franchissement, et la case d'approche ne se calcule pas.
  void AdvancePending(ro::net::Connection& link, ro::sim::World& world,
                      ro::sim::WalkPlanner& planner, const ro::sim::WalkMap* walk,
                      std::uint32_t now_ms);

  // Renonce à ce qui était en attente. Un clic remplace l'intention d'avant :
  // on ne poursuit plus une cible qu'on a cessé de viser.
  //
  // ⚠ LES DEUX ENSEMBLE : l'action et la compétence qu'elle porte vont de
  // pair, et n'en effacer qu'une laisserait le tic sans mémoire, ou la mémoire
  // sans tic.
  void CancelPending() {
    pending_.reset();
    pending_skill_.reset();
  }

  // Vrai tant qu'une cible est poursuivie. C'est ce qui tient le personnage en
  // GARDE, comme chez le client natif.
  [[nodiscard]] bool has_pending() const noexcept { return pending_.has_value(); }
  // La cible de la compétence qui attend d'être à portée, ou zéro.
  [[nodiscard]] std::uint32_t pending_target_id() const noexcept {
    return pending_.has_value() ? pending_->target_id : 0;
  }

  // Ce qu'il faut peindre. Vide quand le panneau est fermé.
  [[nodiscard]] std::vector<ro::render::ScreenQuad> Drawn(const ro::render::FontAtlas& font,
                                                          const ro::sim::World& world,
                                                          float viewport_width,
                                                          float viewport_height,
                                                          std::uint32_t now_ms);

 private:
  // Refait les libellés à partir du grimoire du monde.
  //
  // Les vues du panneau désignent `labels_` et `details_` : les refaire à
  // chaque image coûte quelques allocations et évite d'avoir à savoir quand le
  // serveur a changé une ligne.
  void Rebuild(const ro::sim::World& world, std::uint32_t now_ms);

  // Ramène le premier rang montré dans ce que la fenêtre permet.
  //
  // Le panneau rétrécit quand la fenêtre rétrécit, et le grimoire raccourcit
  // quand on oublie une compétence : un rang valable hier ne l'est plus, et
  // c'est un seul endroit qui le rattrape.
  [[nodiscard]] std::size_t FirstRowWithin(const ro::render::SkillPanelLayout& layout) const;

  // Inscrit `skill` sur l'entité ou l'unité `target_id` : la poursuite commune.
  void CastOnId(const ro::sim::KnownSkill& skill, std::uint32_t target_id);

  // Le nom que la conversation écrit : voir `SkillShownName`.
  [[nodiscard]] std::string ShownName(const ro::sim::KnownSkill& skill) const;

  const ro::sim::SkillTree* tree_ = nullptr;
  bool open_ = false;
  // Une COPIE, et non un pointeur dans le grimoire : le serveur peut mettre à
  // jour une compétence entre le choix et le clic sur la cible, et le tableau
  // se réarrange alors sous le pointeur.
  std::optional<ro::sim::KnownSkill> armed_;
  // L'ACTION EN ATTENTE, et la compétence qu'elle lancera à l'arrivée.
  //
  // ⚠ LA COMPÉTENCE SE GARDE EN ENTIER, comme `armed_` et pour la même raison :
  // le serveur peut mettre à jour une ligne du grimoire pendant qu'on marche
  // vers la cible, et le tableau se réarrange alors sous un pointeur.
  std::optional<ro::sim::PendingAction> pending_;
  std::optional<ro::sim::KnownSkill> pending_skill_;
  std::uint32_t launched_target_ = 0;
  std::uint32_t entity_casts_ = 0;
  bool cart_choice_asked_ = false;
  std::optional<GroundCast> talkbox_asked_;
  std::uint32_t arms_ = 0;
  ro::sim::ApproachWalkClock approach_clock_;
  // Le premier rang MONTRÉ. Il survit à une fermeture : rouvrir le grimoire
  // au même endroit vaut mieux que de tout refaire défiler.
  std::size_t first_row_ = 0;
  std::vector<std::string> labels_;
  std::vector<std::string> details_;
  std::vector<ro::render::SkillPanelRow> rows_;
  std::string title_;
};

}  // namespace moonclient
