#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/damage_mark.h"
#include "ro/render/floating_number.h"
#include "ro/render/number_look.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/damage_texts.h"

#include "world/damage_text_view.h"
#include "world/hit_popup_look.h"
#include "world/hit_popup_rules.h"
#include "world/map_stage.h"
#include "world/struck_place.h"

namespace moonclient {

// Ce qu'un coup fait paraître au-dessus de sa cible : un NOMBRE quand il
// entame, un MOT quand il n'entame pas.
//
// Les compétences SANS DÉGÂTS y passent aussi : deux d'entre elles portent un
// nombre, le soin et la récupération de magie.
//
// C'est la première pièce de ce que le client fait de PLUS que jouer une
// animation. Le monde ne garde que le coup — qui, qui, quand, combien — et
// c'est ici qu'un coup nouveau devient ce qui se voit, à l'instant où il
// touche et à la couleur de sa cible.
//
// ⚠ CE QUI SE MONTRE ET COMMENT EST UN RÉGLAGE, relu à chaque appel : voir
// `HitPopupLook`. Les règles qui l'appliquent — qui frappe qui, quelle couleur,
// quel partage d'un coup multiple, quel écart au hasard, quelle dérive — vivent
// dans `hit_popup_rules.h`, et ce module ne fait que poser les nombres et les
// faire vivre.
//
// La marque est facultative : sans son sprite, un coup à zéro ne laisse rien,
// et le reste fonctionne.
class HitPopups {
 public:
  explicit HitPopups(ro::render::DigitSprite digits, const ro::formats::SprSprite* mark_sprite,
                     const ro::formats::ActAnimation* mark_animation)
      : digits_(digits),
        digit_box_(ro::render::DigitGlyphBox(digits)),
        mark_sprite_(mark_sprite),
        mark_animation_(mark_animation) {}

  // Repère les coups que le monde porte depuis le dernier passage, et donne
  // à chacun son nombre. La garde-robe dit quand le coup touche ; sans elle,
  // le nombre naît avec le coup.
  //
  // ⚠ UN COUP QUE LE RÉGLAGE TAIT EST NOTÉ QUAND MÊME : le rallumer ne rejoue
  // pas ce qui s'est passé pendant qu'il était éteint.
  void Notice(const ro::sim::World& world, ro::render::ActorWardrobe* wardrobe,
              const HitPopupLook& look);

  // L'APERÇU EN BOUCLE : tant que le réglage l'allume, joue sur le personnage
  // un coup factice à chaque intervalle, une famille après l'autre — voir
  // `RehearsalAt`. Éteint, ne fait rien, et la boucle reprend du début au
  // prochain allumage.
  void Rehearse(std::uint32_t now_ms, const ro::sim::World& world, const HitPopupLook& look);

  // Ce qui se voit à cet instant, prêt à dessiner APRÈS les acteurs. Les
  // nombres éteints s'en vont ; ceux dont la cible a disparu restent où elle
  // était.
  //
  // 🔴 DANS L'ORDRE DE NAISSANCE, ET À DESSINER TEL QUEL. Le client pose tous
  // ses nombres et ses mots à UNE MÊME profondeur, en avant de la scène : ni
  // le décor ni la distance ne les départagent, et le dernier né se dessine
  // par-dessus les autres. Les trier par distance ferait passer un nombre
  // ancien, que sa dérive a rapproché de la caméra, devant celui qui vient de
  // sortir.
  //
  // ⚠ LES NOMBRES QUE LE RÉGLAGE ÉCRIT EN POLICE N'Y SONT PAS — seule leur
  // étoile, quand ils en portent une : ils sont projetés sur `screen` et
  // déposés dans `texts_on_screen`, que l'étage d'interface peint.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      std::uint32_t now_ms, const MapStage& stage, const ro::sim::World& world,
      ro::sim::WalkPlanner& planner, const HitPopupLook& look, const NumberScreen& screen);

  // Les nombres écrits en police au dernier `Drawn`, dans le même ordre.
  [[nodiscard]] std::span<const ro::ui::DamageTextView> texts_on_screen() const noexcept {
    return texts_;
  }

  // ── LE CADRE D'ESSAI DES TRAJECTOIRES ─────────────────────────────────────
  //
  // Une seconde instance, sans monde : elle fait naître sur une cible factice
  // le coup qui montre un type, et rend ses nombres posés AUTOUR DES PIEDS de
  // cette cible. Mêmes règles, mêmes compositions que le jeu — seule la place
  // change, puisqu'il n'y a pas de carte.

  // Une instance neuve, sur les mêmes sprites.
  [[nodiscard]] HitPopups Twin() const { return HitPopups(digits_, mark_sprite_, mark_animation_); }

  // Fait naître le coup qui montre ce type — voir `PathRehearsal`.
  void SpawnSample(DamagePath path, std::uint32_t now_ms, const HitPopupLook& look);

  // LE SOL DU CADRE, vu de l'écran : ce qu'un pas d'une unité vers l'est — x du
  // monde — puis vers le nord — z — déplace à l'écran, en pixels de sprite, y
  // DESCENDANT, et l'angle que le client tient pour la cible. De quoi poser la
  // dérive de côté comme en jeu.
  struct SampleGround {
    glm::vec2 per_world_x{1.0F, 0.0F};
    glm::vec2 per_world_z{0.0F, 0.0F};
    float body_degrees = 0.0F;
  };

  // UN NOMBRE DU CADRE. Ses quads — ou son texte — sont en pixels de sprite
  // depuis le point de naissance ; `drift_pixels` est la dérive de côté à leur
  // ajouter, y DESCENDANT.
  struct SampledNumber {
    const ro::render::ComposedActor* frame = nullptr;
    std::optional<ro::render::NumberText> text;
    glm::vec2 drift_pixels{0.0F, 0.0F};
    DamagePath path = DamagePath::kOnMonster;
    // L'instant de sa naissance : un coup multiple en fait naître plusieurs.
    std::uint32_t born_ms = 0;
  };

  // Ce qui se voit dans le cadre à cet instant, dans l'ordre de naissance. Les
  // nombres éteints s'en vont.
  [[nodiscard]] std::span<const SampledNumber> Sampled(std::uint32_t now_ms,
                                                       const HitPopupLook& look,
                                                       const SampleGround& ground);

  // La boîte d'un chiffre du sprite, qu'un nombre en police épouse — ou rien.
  [[nodiscard]] const std::optional<ro::render::GlyphBox>& digit_box() const noexcept {
    return digit_box_;
  }

  [[nodiscard]] std::size_t size() const noexcept { return numbers_.size(); }

  // Plus aucun nombre, et plus aucun coup en mémoire : le changement de carte
  // emporte tout, comme chez le client natif, et ce que le serveur redit
  // ensuite est neuf.
  void Forget() {
    numbers_.clear();
    drawn_.clear();
    texts_.clear();
    noticed_effect_.clear();
    // ⚠ ET LES TOTAUX CONTINUS : un coup reçu juste après l'arrivée
    // reprendrait sinon la somme encaissée sur la carte quittée.
    tallies_.clear();
  }

 private:
  struct Popup {
    // QUI LE PORTE, et ce n'est pas toujours la cible du coup : le mot
    // « Miss » naît sur celui qui a RATÉ. Voir `StrikePlan::mark_on_attacker`.
    std::uint32_t carrier_id = 0;
    // L'instant où il naît : celui où le coup touche.
    std::uint32_t born_ms = 0;
    // Un coup qui n'entame pas laisse un MOT et non un nombre, et les deux ne
    // vivent ni à la même cadence ni le même temps.
    std::optional<ro::render::DamageMark> mark;
    // Vrai pour un coup CRITIQUE : l'étoile d'impact se peint derrière le
    // nombre, dans le même cadre. Ce n'est pas une marque de plus — une marque
    // REMPLACE le nombre, l'étoile le PORTE.
    bool star = false;
    // La valeur, la couleur, l'allure et l'opacité — pour un mot aussi, dont la
    // couleur et l'opacité viennent d'ici.
    ro::render::FloatingNumber number;
    // L'écart au hasard autour du point d'arrivée, tiré UNE fois à la
    // naissance : un nombre qui changerait de place à chaque image tremblerait.
    std::array<float, 2> scatter{0.0F, 0.0F};
    // LA DIAGONALE DE DÉRIVE d'un nombre jaillissant, dans notre monde — x puis
    // z —, choisie à la PREMIÈRE IMAGE d'après la direction de la cible, comme
    // chez le client. Nulle pour les autres allures, et tant qu'elle n'est pas
    // choisie.
    std::array<float, 2> drift_direction{0.0F, 0.0F};
    bool drift_chosen = false;
    ro::render::ComposedActor frame;
    // Où la cible avait les pieds à la dernière image où on l'a vue.
    glm::vec3 last_ground{0.0F, 0.0F, 0.0F};
    // L'ÉTOILE D'UN COUP DE PIED sous ce nombre — l'autre étoile, celle qui ne
    // tourne pas et monte avec lui. Voir `StrikePlan::rising_star`.
    bool rising_star = false;
    // SON TYPE, qui choisit sa trajectoire — voir `PathOf`. Le total, à côté
    // des parts, a le sien.
    DamagePath path = DamagePath::kOnMonster;
    // LA CASE DE L'UNITÉ AU SOL qui le porte, relevée au coup : le coup qui
    // brise un bloc naît après que le serveur l'a retiré. Voir
    // `world/struck_place.h`.
    std::optional<ro::sim::CellPosition> kept_unit_cell;
  };

  // Fait naître ce qu'un coup montre sur sa cible, à l'instant où il touche :
  // le mot, ou un nombre par coup — et le cumul à côté, quand le partage le
  // veut. C'est le chemin des coups du monde comme celui de l'aperçu.
  void Spawn(std::uint32_t target_id, std::uint32_t lands_ms, const ro::sim::Strike& strike,
             const StrikeSides& sides, const HitPopupLook& look);

  // Repère les compétences SANS DÉGÂTS reçues depuis le dernier passage, et
  // donne un nombre à celles qui en ont un — le soin et la récupération de
  // magie, les deux seules. Voir SupportNumber.
  void NoticeSupport(const ro::sim::World& world, const HitPopupLook& look);

  // Repère les EFFETS À VALEUR joués depuis le dernier passage, et donne un
  // nombre à ceux des dix numéros qui en font paraître un. Voir EffectNumber :
  // l'effet lui-même se joue ailleurs, et pour tous.
  void NoticeEffectNumbers(const ro::sim::World& world, const HitPopupLook& look);

  // Repère les effets d'ACTEUR qui COMMANDENT un chiffre, et le fait paraître.
  //
  // Ceux-là ne portent pas de valeur dans le paquet : c'est le CLIENT qui la
  // décide, et sa table la dit — un, en rouge ou en magenta selon
  // l'effet.
  void NoticeCommandedNumbers(const ro::sim::World& world, const HitPopupLook& look);

  // Vrai tant que ce qui est né à cet instant se voit encore, la vie de son
  // type comprise.
  [[nodiscard]] bool Alive(const Popup& popup, std::uint32_t now_ms,
                           const HitPopupLook& look) const;

  // Fait naître le nombre d'une compétence sans dégâts sur `carrier_id`.
  void SpawnSupport(std::uint32_t carrier_id, std::uint32_t born_ms, std::uint16_t skill_id,
                    std::int32_t amount, const HitPopupLook& look);

  // Compose le cadre du nombre à cet instant, sur la trajectoire de son type —
  // et rend son texte quand le réglage l'écrit en police. La dérive de côté
  // n'y est pas : elle se pose dans le monde.
  [[nodiscard]] std::optional<ro::render::NumberText> ComposeOne(Popup& number, float elapsed_ms,
                                                                 const HitPopupLook& look);

  ro::render::DigitSprite digits_;
  // La boîte d'un chiffre du sprite, qu'un nombre en police épouse. Sans elle,
  // rien ne s'écrit en police : il n'y a pas de taille à reprendre.
  std::optional<ro::render::GlyphBox> digit_box_;
  const ro::formats::SprSprite* mark_sprite_ = nullptr;
  const ro::formats::ActAnimation* mark_animation_ = nullptr;
  std::vector<Popup> numbers_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::vector<ro::ui::DamageTextView> texts_;
  std::vector<SampledNumber> sampled_;
  // Les coups déjà remarqués. Voir `ro::sim::StrikeLog`.
  ro::sim::StrikeCursor noticed_strikes_;
  // Les compétences sans dégâts déjà remarquées, TOUTES : une potion qui rend
  // PV et SP envoie les deux sur la même cible. Voir `ro::sim::SupportLog`.
  ro::sim::SupportCursor noticed_supports_;
  // Et pour les effets à valeur, pour la même raison — mais la clé y porte
  // l'EFFET autant que l'acteur : un acteur en porte plusieurs à la fois, et
  // un témoin par acteur ferait passer le second pour une redite du premier.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> noticed_effect_;
  // CE QUE CHAQUE CIBLE A DÉJÀ ENCAISSÉ, quand le total continu est allumé :
  // la somme montrée au dernier coup, et l'instant de ce coup. Une entrée
  // oubliée s'efface — voir `CarriedTotal`.
  std::unordered_map<std::uint32_t, TargetTally> tallies_;
  // L'aperçu : le rang de la prochaine entrée, et l'instant où elle est due.
  // Zéro : la boucle n'a pas commencé, ou s'est éteinte.
  std::size_t rehearsal_index_ = 0;
  std::uint32_t rehearsal_due_ms_ = 0;
};

}  // namespace moonclient
