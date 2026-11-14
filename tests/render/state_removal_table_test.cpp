#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/state_effect.h"

namespace {

using ro::render::StateEffectEntry;
using ro::render::StateEffectMoment;
// En `int` : un couple écrit en littéraux s'y élargit, et rien ne s'y rétrécit.
using Couple = std::pair<int, int>;

// `EFST_WZ_SIGHTBLASTER` et sa nuée : une bascule, qui retire sans garde.
constexpr std::uint16_t kSightBlasterState = 198;
constexpr std::uint16_t kSightBlasterEffect = 601;

// Les dix états qui posent `état + 135`, et le retirent de même.
constexpr std::uint16_t kFirstOffsetState = 1016;
constexpr std::uint16_t kLastOffsetState = 1025;
constexpr std::uint16_t kOffsetStateEffect = 135;

[[nodiscard]] bool Removes(std::uint16_t state_id, std::uint16_t effect_id) {
  return std::ranges::any_of(
      ro::render::StateRemovalsFor(state_id),
      [effect_id](const StateEffectEntry& entry) { return entry.effect_id == effect_id; });
}

[[nodiscard]] std::optional<StateEffectMoment> MomentOf(std::uint16_t state_id,
                                                        std::uint16_t effect_id) {
  for (const StateEffectEntry& entry : ro::render::StateRemovalsFor(state_id)) {
    if (entry.effect_id == effect_id) {
      return entry.moment;
    }
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE("la table des retraits se cherche par etat", "[render][state_removal]") {
  const std::span<const StateEffectEntry> retraits = ro::render::StateRemovalTable();
  REQUIRE_FALSE(retraits.empty());
  // ⚠ `StateRemovalsFor` rend une suite CONTIGUË : un état dont les retraits
  // seraient éparpillés n'en rendrait que le premier paquet.
  CHECK(std::ranges::is_sorted(retraits, {}, &StateEffectEntry::state_id));
}

TEST_CASE("un retrait n'entre dans la table que s'il tombe a la levee", "[render][state_removal]") {
  // La table n'est lue qu'à la levée. Un retrait que le client ne fait qu'À LA
  // POSE, appliqué là, retirerait ce qu'il laisse en place.
  for (const StateEffectEntry& entry : ro::render::StateRemovalTable()) {
    INFO("état " << entry.state_id << ", effet " << entry.effect_id);
    CHECK(entry.moment != StateEffectMoment::kOnApply);
  }
  // 🔴 ET UN RETRAIT SANS GARDE Y ENTRE : il tombe aux deux instants. Sight
  // Blaster en est le témoin — sa bascule retire sans condition, et le filtre
  // « à la chute » seul lui faisait perdre sa coupe.
  CHECK(Removes(kSightBlasterState, kSightBlasterEffect));
  CHECK(MomentOf(kSightBlasterState, kSightBlasterEffect) == StateEffectMoment::kAlways);
}

TEST_CASE("un retrait passe aussi par le message 109", "[render][state_removal]") {
  // Aucun retrait direct dans ces deux cas : le client envoie
  // le message 109 à l'acteur, qui retire l'effet d'identifiant donné.
  CHECK(Removes(kSightBlasterState, kSightBlasterEffect));
  CHECK(Removes(328, 788));
  // L'aller-retour le plus lisible, par la queue de retrait partagée.
  CHECK(Removes(1196, 1913));
}

TEST_CASE("un retrait relit la case que son corps a ecrite", "[render][state_removal]") {
  // `HIDWORD(param_7) = 1859`, puis la chute retire `HIDWORD(param_7)`.
  CHECK(Removes(1184, 1859));
  // Le cas retire 1982 en clair, puis 1985 par la case, deux lignes plus bas.
  CHECK(Removes(1233, 1982));
  CHECK(Removes(1233, 1985));
}

TEST_CASE("un retrait fait dans les deux branches tombe a la levee", "[render][state_removal]") {
  // Retiré à la pose ET à la chute : les deux moments s'additionnent. Garder le
  // premier vu jetait l'autre, et la levée perdait son retrait.
  CHECK(Removes(1209, 2245));
  CHECK(Removes(1212, 2269));
}

TEST_CASE("un cas qui retire puis saute vers la pose garde sa creation",
          "[render][state_removal]") {
  // Ces cas retirent leur effet, puis sautent vers la queue qui le repose. Un
  // retrait propre ne dit pas que le cas ne crée rien : l'héritage se décide
  // par espèce de geste.
  const auto cree = [](std::uint16_t state_id, std::uint16_t effect_id) {
    return std::ranges::any_of(
        ro::render::StateEffectsFor(state_id),
        [effect_id](const StateEffectEntry& entry) { return entry.effect_id == effect_id; });
  };
  CHECK(cree(107, 368));
  CHECK(Removes(107, 368));
  CHECK(cree(165, 418));
  CHECK(cree(165, 544));
  CHECK(Removes(165, 418));
}

TEST_CASE("un retrait lit ce que le sauteur a prepare", "[render][state_removal]") {
  // Le sauteur écrit la case, la QUEUE la retire : lue à la queue seule,
  // l'identifiant est une inconnue.
  //
  // `param_7 = x | 0x3F500000000` : la moitié haute, écrite d'un coup.
  CHECK(Removes(675, 1013));
  CHECK(MomentOf(675, 1013) == StateEffectMoment::kOnRelease);
  // ⚠ L'ÉCRITURE EST HISSÉE AVANT LA GARDE, le saut la porte : le moment est
  // celui du saut, pas celui de l'écriture.
  CHECK(Removes(919, 1097));
  CHECK(MomentOf(919, 1097) == StateEffectMoment::kOnRelease);
  // `état + 135`, un décalage : il se développe, il ne se jette pas.
  for (std::uint16_t offset_state = kFirstOffsetState; offset_state <= kLastOffsetState;
       ++offset_state) {
    INFO("état " << offset_state);
    CHECK(Removes(offset_state, static_cast<std::uint16_t>(offset_state + kOffsetStateEffect)));
    CHECK(MomentOf(offset_state, static_cast<std::uint16_t>(offset_state + kOffsetStateEffect)) ==
          StateEffectMoment::kOnRelease);
  }
}

TEST_CASE("un retrait reprend ce qu'un etat pose, le sien ou celui d'un voisin",
          "[render][state_removal]") {
  // Un retrait dont rien ne dit d'où vient l'effet est le signe qu'une des
  // deux moitiés a été mal lue.
  std::set<Couple> poses;
  std::set<std::uint16_t> placed_effects;
  for (const std::span<const StateEffectEntry> table :
       {ro::render::StateEffectTable(), ro::render::StatePacketEffectTable(),
        ro::render::StateRebuildEffectTable()}) {
    for (const StateEffectEntry& entry : table) {
      poses.emplace(entry.state_id, entry.effect_id);
      placed_effects.insert(entry.effect_id);
    }
  }
  std::set<Couple> voisins;
  std::set<Couple> inexpliques;
  for (const StateEffectEntry& entry : ro::render::StateRemovalTable()) {
    const Couple couple{entry.state_id, entry.effect_id};
    if (poses.contains(couple)) {
      continue;
    }
    (placed_effects.contains(entry.effect_id) ? voisins : inexpliques).insert(couple);
  }
  // 🔴 UN ÉTAT PEUT ÉTEINDRE L'EFFET D'UN AUTRE : 95 retire ce que pose 433, 606
  // ce que posent 655 et 659.
  CHECK(voisins == std::set<Couple>{{95, 453}, {606, 963}});
  // ⚠ ET TROIS RETRAITS VISENT UN EFFET QU'AUCUN ÉTAT NE POSE. 395 est celui de
  // la compétence 396, posé sur le lanceur ; 819 et 2052 ont un poseur que nos
  // tables ne relèvent pas. Les retirer sans jamais les créer ne fait rien,
  // mais la liste est fermée : un quatrième désignerait une lecture fausse.
  CHECK(inexpliques == std::set<Couple>{{121, 395}, {394, 819}, {1230, 2052}});
  // Les traces de la course (442), que pose l'annonce de TK_RUN, se retirent
  // par les gestes gardés de ses deux états.
  for (const std::uint16_t running_state : {std::uint16_t{133}, std::uint16_t{134}}) {
    CHECK(std::ranges::any_of(ro::render::StateGuardedGesturesFor(running_state),
                              [](const ro::render::StateGuardedGesture& gesture) {
                                return gesture.effect_id == 442 && gesture.withdraws;
                              }));
  }
}
