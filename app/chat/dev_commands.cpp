#include "chat/dev_commands.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/log.h"
#include "ro/net/frame_tally.h"
#include "ro/render/effect_command.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_command.h"
#include "effects/effect_node_view.h"
#include "world/item_dropper.h"
#include "world/map_stage.h"

namespace moonclient {
namespace {

// Combien de lignes un résumé écrit à défaut de fenêtre : au-delà, un résumé
// noie ce qu'il résume.
constexpr std::size_t kSummaryLines = 8;

// LES ARGUMENTS CHIFFRÉS D'UNE COMMANDE D'ATELIER, avec leurs défauts.
//
// ⚠ UN ARGUMENT ABSENT OU ILLISIBLE GARDE SON DÉFAUT, et la ligne passe quand
// même : ce sont des outils de réglage, et une faute de frappe n'a pas à coûter
// un aller-retour. Une commande du JEU, elle, refuserait.
std::array<std::uint32_t, 3> ReadNumbers(std::string_view rest,
                                         std::array<std::uint32_t, 3> fallback) {
  std::array<std::uint32_t, 3> read = fallback;
  std::size_t at = 0;
  for (std::size_t rank = 0; rank < read.size(); ++rank) {
    while (at < rest.size() && rest[at] == ' ') {
      ++at;
    }
    if (at >= rest.size()) {
      break;
    }
    std::uint32_t value = 0;
    const auto [after, error] = std::from_chars(rest.data() + at, rest.data() + rest.size(), value);
    if (error != std::errc{}) {
      break;
    }
    read[rank] = value;
    at = static_cast<std::size_t>(after - rest.data());
  }
  return read;
}

// À NOS PIEDS : c'est là que le jeu pose la plupart des effets, et c'est
// l'endroit dont on juge le mieux la taille et la hauteur.
glm::vec3 AtOurFeet(const ChatDispatch& to, std::uint32_t said_ms) {
  const ro::sim::Actor* self = to.world->player();
  const ro::sim::FractionalCell cell =
      self != nullptr ? ro::sim::InterpolatedCell(*self, to.planner->PathFor(*self), said_ms)
                      : ro::sim::FractionalCell{};
  const std::array<float, 3> ground = GroundPositionAt(*to.stage, cell.x, cell.y);
  return glm::vec3(ground[0], ground[1], ground[2]);
}

// `/noeud <sprite>` ou `/noeud <identifiant d'effet>`.
void SowEffectNode(const ChatDispatch& to, const std::string& sprite, std::uint32_t said_ms) {
  if (sprite.empty()) {
    ro::core::LogWarning("nommer le sprite à semer — /noeud particle1 — ou l'effet — /noeud 7");
    return;
  }
  const glm::vec3 place = AtOurFeet(to, said_ms);
  // TOUT EN CHIFFRES : un IDENTIFIANT D'EFFET, et non un nom de sprite. C'est
  // la même règle que « /effet », et c'est ce qui permet de regarder un effet
  // PROCÉDURAL sans attendre que le jeu le joue — un anneau ne se juge qu'à
  // sa place, dans le monde.
  if (std::ranges::all_of(sprite, [](char letter) { return letter >= '0' && letter <= '9'; })) {
    std::uint32_t identifier = 0;
    const auto [unused, error] =
        std::from_chars(sprite.data(), sprite.data() + sprite.size(), identifier);
    const std::span<const ro::render::EffectNodePattern> patterns =
        error == std::errc{} ? ro::render::EffectNodePatternsFor(identifier)
                             : std::span<const ro::render::EffectNodePattern>{};
    if (patterns.empty()) {
      ro::core::LogWarning("effet sans patron de nœuds : {}", sprite);
      return;
    }
    // CE QUI A ÉTÉ SEMÉ SE DIT, et ne se devine pas à l'écran : un nœud dont
    // la ressource ne se lit pas, ou dont la figure se voit par la tranche,
    // laisse le même vide qu'un effet absent.
    // PAR LE MEME CHEMIN QUE LE JEU, horloge comprise : un effet dont les
    // noeuds naissent au fil des tics ne se juge pas sur son premier tic, et
    // le semer d'un coup ici montrerait autre chose que ce qu'on regarde.
    const bool sown_any = to.effect_nodes->PlayProcedural(identifier, place);
    ro::core::LogInfo("effet {} : {} patron(s), {}", sprite, patterns.size(),
                      sown_any ? "semé(s)" : "AUCUN semé");
    return;
  }
  if (!to.effect_nodes->Play(sprite, place)) {
    ro::core::LogWarning("sprite d'effet introuvable : {}", sprite);
  }
}

void TellFrameTally(const ChatDispatch& to) {
  // LA FENÊTRE, quand il y en a une. Le relevé fait couramment plus de lignes
  // qu'un résumé n'en tient ; à défaut de fenêtre, le résumé part dans le
  // journal, qui n'a pas cette borne.
  if (to.packet_window != nullptr) {
    *to.packet_window = !*to.packet_window;
    ro::core::LogInfo("trames : inspecteur {}", *to.packet_window ? "ouvert" : "fermé");
  } else {
    for (const std::string& said : ro::net::DescribedFrameTally(*to.frames, kSummaryLines)) {
      ro::core::LogInfo("{}", said);
    }
  }
  // Le détail par opcode dans les deux cas — c'est là qu'on lit si un opcode
  // arrive et se perd, ou s'il n'arrive simplement jamais.
  for (const ro::net::FrameTally::Entry& entry : to.frames->All()) {
    ro::core::LogInfo(
        "trames {} : {} appliquée(s), {} ignorée(s), {} non traitée(s), {} refusée(s)",
        ro::net::HexOpcode(entry.opcode), entry.applied, entry.ignored, entry.unhandled,
        entry.undecodable);
  }
}

void TellEffectTally(const ChatDispatch& to) {
  const std::vector<EffectsInPlay::EffectAsked> asked_effects = to.effects->AskedEffects();
  std::size_t total = 0;
  for (const EffectsInPlay::EffectAsked& asked_effect : asked_effects) {
    total += asked_effect.asked;
  }
  ro::core::LogInfo(
      "effets : {} identifiant(s) distinct(s) demandé(s) par ce serveur, {} demande(s)",
      asked_effects.size(), total);
  // ⚠ LE DÉTAIL PART DANS LE JOURNAL ET NON DANS LE FIL : une partie en
  // demande des centaines, et le fil n'en tient que quelques lignes. C'est le
  // fichier qu'on relit après coup.
  for (const EffectsInPlay::EffectAsked& asked_effect : asked_effects) {
    ro::core::LogInfo("effets {} : {} demande(s)", asked_effect.effect_id, asked_effect.asked);
  }
}

}  // namespace

bool RunDevelopmentCommand(const ChatDispatch& to, std::string_view line, std::uint32_t said_ms) {
  // Le tirage vient de l'horloge : c'est ce qui fait qu'un effet à plusieurs
  // fichiers n'en montre pas toujours le même.
  if (const std::optional<std::string> effect = ro::render::EffectCommand(line, said_ms);
      effect.has_value()) {
    if (effect->empty()) {
      ro::core::LogWarning("cet effet n'a pas de fichier : {}", line);
    } else if (!to.effects->Play(*effect, AtOurFeet(to, said_ms), said_ms)) {
      ro::core::LogWarning("effet introuvable : {}", *effect);
    }
    return true;
  }
  if (const std::optional<std::string> sprite = EffectNodeCommand(line); sprite.has_value()) {
    SowEffectNode(to, *sprite, said_ms);
    return true;
  }
  if (line == kFrameTallyCommand && to.frames != nullptr) {
    TellFrameTally(to);
    return true;
  }
  if (line == kEffectTallyCommand && to.effects != nullptr) {
    TellEffectTally(to);
    return true;
  }
  if (line.starts_with(kFakeLootCommand) && to.world != nullptr) {
    // Trois arguments, tous facultatifs : combien, quel objet, quel pilier.
    const std::array<std::uint32_t, 3> asked =
        ReadNumbers(line.substr(kFakeLootCommand.size()),
                    {kFakeLootCount, kFakeLootItemId, kFakeLootCyclingMode});
    const std::size_t sown = SowFakeLoot(*to.world, asked[0], static_cast<std::uint16_t>(asked[1]),
                                         static_cast<std::uint16_t>(asked[2]), said_ms);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         sown == 0 ? "butin : la carte est nettoyée"
                                   : std::format("butin : {} leurre(s) semé(s)", sown),
                         said_ms);
    return true;
  }
  if (line == kSkillWindowCommand && to.skill_window != nullptr) {
    *to.skill_window = !*to.skill_window;
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         *to.skill_window ? "grimoire : ouvert" : "grimoire : fermé", said_ms);
    return true;
  }
  if (line == kInventoryWindowCommand && to.inventory_window != nullptr) {
    *to.inventory_window = !*to.inventory_window;
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         *to.inventory_window ? "sac : ouvert" : "sac : ferme", said_ms);
    return true;
  }
  return false;
}

}  // namespace moonclient
