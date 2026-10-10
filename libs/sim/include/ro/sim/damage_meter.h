#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/meter_tally.h"

namespace ro::sim {

// UN FAIT AU PALMARÈS.
struct MeterRecord {
  MeterBlow blow;
  // L'instant, dans l'horloge de l'appelant.
  std::uint32_t at_ms = 0;
  // Le numéro de l'activité de son mode où il a eu lieu : c'est ce qui permet
  // de dire « celle-ci » parmi les dix.
  std::uint32_t activity = 0;
};

// QUI LE COMPTEUR SUIT, en plus de nous : les membres du groupe.
struct MeterActor {
  std::uint32_t id = 0;
  std::string name;
};

// LE COMPTEUR DE COMBAT : ce que nous et notre groupe infligeons, soignons,
// encaissons et recevons — Skada et Recount, pour Ragnarok.
//
// Pour NOUS, chaque mode a sa mécanique complète : la fenêtre glissante, le combat qui s'ouvre et
// se ferme tout seul, les colonnes du graphique, le palmarès. Pour le groupe, il tient des SEGMENTS
// : le combat en cours, le précédent, la session ; chacun ventilé par acteur et par compétence.
//
// ⚠ IL NE VOIT QUE CE QUE LE SERVEUR MONTRE : un coup n'est annoncé qu'à qui
// est à portée de vue. Un membre hors de l'écran ne compte pas.
//
// 🔴 SOUS UNE COMPÉTENCE DE ZONE, LA SOURCE ANNONCÉE EST L'UNITÉ AU SOL, et
// c'est son lanceur qu'on crédite. Pour NOUS, pas : le serveur Moonlight nous
// envoie en plus `ZC_MOONLIGHT_SKILL_DMG`, la somme de chaque battement, toutes
// cibles comprises, même hors de vue. Compter aussi les coups vus les
// compterait deux fois.
//
// UN COMBAT S'OUVRE SUR DES DÉGÂTS, infligés ou subis, par l'un de ceux qu'on
// suit, et se ferme après un silence de dégâts. Les soins s'y rangent ; hors
// combat, ils ne vont qu'à la session.
class DamageMeter {
 public:
  // Ce que le joueur règle. Les bornes sont celles du panneau de réglages.
  struct Settings {
    // La largeur d'une colonne du graphique, en millisecondes.
    std::uint32_t slot_ms = 200;
    // La fenêtre glissante sur laquelle le rythme se calcule, en secondes.
    std::uint32_t window_secs = 10;
    // Le silence au bout duquel le combat est réputé fini, en secondes.
    std::uint32_t combat_timeout_secs = 5;
  };

  // Le nombre de colonnes du graphique. L'histoire dure donc
  // `slot_ms × kPlotSlots` millisecondes.
  static constexpr std::size_t kPlotSlots = 150;
  // La taille d'un palmarès.
  static constexpr std::size_t kKeptRecords = 10;
  // Combien de cumuls de zone attendent, au plus, que la conversation les
  // prenne. Au-delà, les plus récents se perdent : sous un Meteor Storm, une
  // conversation qui ne passe plus ne doit pas faire grossir la file.
  static constexpr std::size_t kKeptAreaCycles = 64;

  [[nodiscard]] Settings& settings() noexcept { return settings_; }
  [[nodiscard]] const Settings& settings() const noexcept { return settings_; }

  // ── QUI ON SUIT ───────────────────────────────────────────────────────────
  // Nous, et les membres du groupe. Refait à chaque image par l'appelant : le
  // groupe change, et nous ne le connaissons pas ici.
  void Track(std::uint32_t self_id, std::string_view self_name, std::span<const MeterActor> party);
  // Nous seuls, quand le monde le sait avant l'appelant.
  void SetSelf(std::uint32_t self_id) noexcept { self_id_ = self_id; }
  [[nodiscard]] std::uint32_t self_id() const noexcept { return self_id_; }
  [[nodiscard]] bool tracks(std::uint32_t id) const noexcept;

  // ── CE QUI ARRIVE ─────────────────────────────────────────────────────────
  // Un fait déjà attribué. ⚠ IGNORÉ si `who` n'est pas suivi.
  void Record(const MeterBlow& blow, std::uint32_t now_ms);
  // Un coup infligé PAR NOUS, réduit à ses dégâts.
  void Record(std::int64_t damage, std::uint32_t now_ms);
  // Un coup du monde. `author` est son auteur réel — le lanceur quand c'est
  // une unité au sol qui frappe —, `by_unit` dit que c'en est une.
  void NoteStrike(const Strike& strike, std::uint32_t author, bool by_unit, const Actor* attacker,
                  const Actor* target);
  // Un soin annoncé : une compétence sans dégâts, dont le montant est le soin.
  // `author` est le soigneur réel, zéro quand c'est un objet.
  void NoteHeal(const SupportHit& hit, std::uint32_t author, std::uint32_t target_id,
                const Actor* healer, const Actor* target);
  // Ce que le serveur nous rend sans l'annoncer comme un soin : la
  // régénération des compétences, un vol de vie, Kaahi, le gain à la mort
  // d'un monstre. Il ne le dit qu'à nous, et parfois EN PLUS du soin annoncé —
  // voir l'implémentation.
  void NoteRecovery(std::int64_t amount, std::uint32_t now_ms);

  // L'INSTANT QUI PASSE : ferme le combat après son silence, avance les
  // colonnes, et recalcule les rythmes. À appeler une fois par image.
  void Tick(std::uint32_t now_ms);

  // Oublier le combat, son graphique et son segment — un changement de réglage
  // qui les rend illisibles, ou le geste du joueur. ⚠ LE PALMARÈS ET LA
  // SESSION RESTENT.
  void Forget();
  // Vider les palmarès.
  void ForgetRecords() noexcept;
  // Tout recommencer : session, segments, palmarès.
  void ForgetSession();

  // ── NOUS, MODE PAR MODE ───────────────────────────────────────────────────
  //
  // 🔴 CHAQUE MODE A SON ACTIVITÉ, et non celle du combat : le premier fait
  // l'ouvre, un silence la ferme, mode par mode. Un prêtre qui soigne hors de
  // tout combat voit ainsi sa courbe, son pic et son total ; et le compteur de
  // dégâts infligés ne s'ouvre que sur NOS coups.
  //
  // Notre activité dans ce mode est-elle ouverte ?
  [[nodiscard]] bool active(MeterMode mode) const noexcept { return tracks_[Rank(mode)].active; }
  // Depuis combien de temps elle dure, en millisecondes. Zéro hors activité.
  [[nodiscard]] std::uint32_t activity_ms(MeterMode mode, std::uint32_t now_ms) const noexcept;
  // LA DURÉE QUI DIVISE LE TOTAL : jusqu'à maintenant pendant l'activité,
  // jusqu'au DERNIER FAIT une fois close. Zéro avant le premier.
  [[nodiscard]] std::uint32_t activity_span_ms(MeterMode mode, std::uint32_t now_ms) const noexcept;
  // Le numéro de l'activité en cours ou de la dernière, à partir de un.
  [[nodiscard]] std::uint32_t activity(MeterMode mode) const noexcept {
    return tracks_[Rank(mode)].number;
  }
  // Le rythme sur la fenêtre glissante, par seconde.
  [[nodiscard]] float rate(MeterMode mode) const noexcept { return tracks_[Rank(mode)].rate; }
  // Le plus haut rythme atteint pendant CETTE activité. ⚠ LE MÊME RYTHME QUE
  // CELUI QU'ON LIT, et non le pic d'une colonne, qui gonfle un coup unique.
  [[nodiscard]] float peak(MeterMode mode) const noexcept { return tracks_[Rank(mode)].peak; }
  // Notre total de CETTE activité.
  [[nodiscard]] std::int64_t total(MeterMode mode) const noexcept {
    return tracks_[Rank(mode)].total;
  }
  // Les colonnes, dans l'ordre du tampon circulaire, et l'endroit où la
  // prochaine s'écrira — donc la plus ANCIENNE visible.
  [[nodiscard]] std::span<const float> plot(MeterMode mode) const noexcept {
    return tracks_[Rank(mode)].plot;
  }
  [[nodiscard]] std::size_t plot_offset(MeterMode mode) const noexcept {
    return tracks_[Rank(mode)].offset;
  }
  // LE PALMARÈS d'un mode, du plus gros au plus petit. À égalité, le plus
  // ANCIEN garde sa place.
  [[nodiscard]] std::span<const MeterRecord> records(MeterMode mode) const noexcept {
    return records_[Rank(mode)];
  }

  // ── LE COMBAT ─────────────────────────────────────────────────────────────
  [[nodiscard]] bool in_combat() const noexcept { return in_combat_; }
  // Depuis combien de temps ce combat dure, en millisecondes. Zéro hors combat.
  [[nodiscard]] std::uint32_t combat_ms(std::uint32_t now_ms) const noexcept;
  // LA DURÉE QUI DIVISE LE TOTAL : jusqu'à maintenant en combat, jusqu'au
  // DERNIER COUP une fois le combat clos. Zéro avant le premier.
  [[nodiscard]] std::uint32_t fight_span_ms(std::uint32_t now_ms) const noexcept;
  // Le numéro du combat en cours ou du dernier, à partir de un.
  [[nodiscard]] std::uint32_t fight() const noexcept { return fight_; }

  // ── LES SEGMENTS ──────────────────────────────────────────────────────────
  // ⚠ LE TEMPS ACTIF DU COMBAT EN COURS ET DE LA SESSION se lit à l'instant :
  // voir `active_ms`.
  [[nodiscard]] const MeterSegment& current() const noexcept { return current_; }
  [[nodiscard]] const MeterSegment& previous() const noexcept { return previous_; }
  [[nodiscard]] const MeterSegment& session() const noexcept { return session_; }
  // Le temps actif d'un segment À CET INSTANT : un combat en cours compte
  // jusqu'à maintenant.
  [[nodiscard]] std::uint32_t active_ms(const MeterSegment& segment,
                                        std::uint32_t now_ms) const noexcept;

  // Les cumuls de zone arrivés depuis le dernier appel, dans l'ordre ; la file
  // se vide.
  [[nodiscard]] std::vector<std::int64_t> TakeAreaCycles();

 private:
  struct Blow {
    std::uint32_t at_ms = 0;
    std::int64_t amount = 0;
  };
  // Notre activité dans un mode : ses faits, ses colonnes, son bilan.
  struct RateTrack {
    std::deque<Blow> blows;
    std::array<float, kPlotSlots> plot = {};
    std::size_t offset = 0;
    float rate = 0.0F;
    float peak = 0.0F;
    std::int64_t total = 0;
    bool active = false;
    std::uint32_t number = 0;
    std::uint32_t since_ms = 0;
    std::uint32_t last_ms = 0;
    // Le début de la colonne en cours. ⚠ AUCUNE COLONNE N'AVANCE AVANT LE
    // PREMIER FAIT : la première rafale remplirait sinon tout le tampon de
    // vide — d'où l'activité, et non un instant nul, que l'horloge atteint en
    // bouclant.
    std::uint32_t slot_since_ms = 0;
  };

  void OpenFight(std::uint32_t now_ms);
  void CloseFight();
  void Note(RateTrack& track, std::int64_t amount, std::uint32_t now_ms);
  void AdvanceSlots(RateTrack& track, std::uint32_t now_ms) const;
  void Enter(const MeterBlow& blow, std::uint32_t now_ms, std::uint32_t activity);
  [[nodiscard]] std::string_view NameOf(std::uint32_t id) const;

  Settings settings_;
  std::uint32_t self_id_ = 0;
  std::string self_name_;
  std::vector<MeterActor> party_;

  std::array<RateTrack, kMeterModeCount> tracks_;

  // Le dernier fait de DÉGÂTS : c'est son silence qui ferme le combat.
  std::uint32_t last_damage_ms_ = 0;
  std::uint32_t fight_since_ms_ = 0;
  std::uint32_t fight_ = 0;
  bool in_combat_ = false;

  MeterSegment current_;
  MeterSegment previous_;
  MeterSegment session_;

  // Le dernier soin annoncé sur nous : voir `NoteRecovery`.
  std::uint32_t last_heal_on_self_ms_ = 0;
  std::int64_t last_heal_on_self_ = 0;

  std::array<std::vector<MeterRecord>, kMeterModeCount> records_;
  std::vector<std::int64_t> area_cycles_;
};

// SI CETTE SORTE DE COUP PORTE DE VRAIS DÉGÂTS. Le serveur numérote dans le
// même champ des faits qui n'en sont pas — ramasser un objet, s'asseoir, se
// lever, esquiver —, et les compter ferait battre le compteur sur un joueur
// assis.
[[nodiscard]] bool DamageActionCounts(std::uint8_t action);

// LES COMPÉTENCES DONT LE SERVEUR ANNONCE UN SOIN, montant dans le champ du
// niveau : Heal — sous lequel passent aussi Sanctuary, Potion Pitcher et les
// soins sans lanceur —, Coluceo Heal et Highness Heal, sous leur propre
// numéro. Toute autre compétence sans dégâts y porte son NIVEAU.
inline constexpr std::uint16_t kHealAnnounceSkill = 28;
inline constexpr std::array<std::uint16_t, 3> kHealAnnounceSkills = {kHealAnnounceSkill, 2043,
                                                                     2051};

}  // namespace ro::sim
