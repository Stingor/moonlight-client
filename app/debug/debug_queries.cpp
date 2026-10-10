#include "debug/debug_queries.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <vector>

#include "ro/core/log.h"
#include "ro/sim/actor.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "debug/debug_json.h"

namespace moonclient {
namespace {

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CompanionAiReason;
using TargetRefusal = ro::sim::CompanionBrain::TargetRefusal;

// Ce qu'une liste rend au plus quand la requête ne le dit pas.
constexpr std::int64_t kDefaultActorLimit = 200;
constexpr std::int64_t kDefaultFrameLimit = 50;
constexpr std::int64_t kDefaultLogLimit = 100;
// Un identifiant d'acteur tient sur quatre octets, petit-boutiste.
constexpr std::size_t kActorIdBytes = 4;
constexpr unsigned kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kHexBase = 16;

[[nodiscard]] const char* KindName(ActorKind kind) {
  switch (kind) {
    case ActorKind::kPlayer:
      return "player";
    case ActorKind::kNonPlayer:
      return "npc";
    case ActorKind::kMonster:
      return "monster";
    case ActorKind::kItem:
      return "item";
    case ActorKind::kSkillUnit:
      return "skill_unit";
    case ActorKind::kUnknown:
      break;
  }
  return "unknown";
}

[[nodiscard]] const char* PostureName(ro::sim::Posture posture) {
  switch (posture) {
    case ro::sim::Posture::kStanding:
      return "standing";
    case ro::sim::Posture::kSitting:
      return "sitting";
    case ro::sim::Posture::kDead:
      return "dead";
  }
  return "?";
}

[[nodiscard]] const char* FamiliarName(ro::sim::Familiar familiar) {
  switch (familiar) {
    case ro::sim::Familiar::kNone:
      return "none";
    case ro::sim::Familiar::kPet:
      return "pet";
    case ro::sim::Familiar::kHomunculus:
      return "homunculus";
    case ro::sim::Familiar::kMercenary:
      return "mercenary";
    case ro::sim::Familiar::kElemental:
      return "elemental";
  }
  return "?";
}

[[nodiscard]] const char* ReasonName(CompanionAiReason reason) {
  switch (reason) {
    case CompanionAiReason::kNone:
      return "none";
    case CompanionAiReason::kDefendOwner:
      return "defend_owner";
    case CompanionAiReason::kDefendSelf:
      return "defend_self";
    case CompanionAiReason::kDefendPartner:
      return "defend_partner";
    case CompanionAiReason::kDefendFriend:
      return "defend_friend";
    case CompanionAiReason::kAssistOwner:
      return "assist_owner";
    case CompanionAiReason::kOwnInitiative:
      return "own_initiative";
    case CompanionAiReason::kFollowOwner:
      return "follow_owner";
    case CompanionAiReason::kStandby:
      return "standby";
    case CompanionAiReason::kDirectMove:
      return "direct_move";
    case CompanionAiReason::kDirectAttack:
      return "direct_attack";
    case CompanionAiReason::kDirectSkill:
      return "direct_skill";
    case CompanionAiReason::kLeash:
      return "leash";
    case CompanionAiReason::kRetreat:
      return "retreat";
    case CompanionAiReason::kFlee:
      return "flee";
    case CompanionAiReason::kOwnerSeated:
      return "owner_seated";
    case CompanionAiReason::kAvoid:
      return "avoid";
    case CompanionAiReason::kHarass:
      return "harass";
    case CompanionAiReason::kRule:
      return "rule";
  }
  return "?";
}

[[nodiscard]] const char* RefusalName(TargetRefusal refusal) {
  switch (refusal) {
    case TargetRefusal::kNotFoe:
      return "not_foe";
    case TargetRefusal::kAbandoned:
      return "abandoned";
    case TargetRefusal::kConcealed:
      return "concealed_or_dead";
    case TargetRefusal::kOwnSummon:
      return "own_summon";
    case TargetRefusal::kBeyondSight:
      return "beyond_companion_sight";
    case TargetRefusal::kOutOfReach:
      return "out_of_reach_without_chase";
    case TargetRefusal::kBeyondEngagement:
      return "beyond_engagement";
    case TargetRefusal::kBeyondLeash:
      return "beyond_leash";
    case TargetRefusal::kIgnored:
      return "tactic_ignore";
    case TargetRefusal::kHeld:
      return "tactic_hold";
    case TargetRefusal::kInitiativeBarred:
      return "initiative_barred";
    case TargetRefusal::kSpared:
      return "other_players_monster";
    case TargetRefusal::kNoMotive:
      return "no_motive";
  }
  return "?";
}

[[nodiscard]] const char* OutcomeName(ro::net::FrameOutcome outcome) {
  switch (outcome) {
    case ro::net::FrameOutcome::kApplied:
      return "applied";
    case ro::net::FrameOutcome::kIgnored:
      return "ignored";
    case ro::net::FrameOutcome::kUnhandled:
      return "unhandled";
    case ro::net::FrameOutcome::kUndecodable:
      return "undecodable";
  }
  return "?";
}

// LE NIVEAU D'UNE LIGNE, nommé par la console. ⚠ PAS `ro::core::ToString`, qui
// parle français au journal à l'écran : le protocole a ses propres noms.
[[nodiscard]] const char* LevelName(ro::core::LogLevel level) {
  switch (level) {
    case ro::core::LogLevel::kDebug:
      return "debug";
    case ro::core::LogLevel::kInfo:
      return "info";
    case ro::core::LogLevel::kWarning:
      return "warning";
    case ro::core::LogLevel::kError:
      return "error";
  }
  return "info";
}

// Un entier en décimal ou en hexadécimal préfixé (« 0x0080 »).
[[nodiscard]] std::optional<std::int64_t> NumberOf(std::string_view text) {
  int base = 10;
  if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
    base = kHexBase;
  }
  std::int64_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
  if (text.empty() || error != std::errc() || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::string HexBytes(const std::vector<std::uint8_t>& bytes) {
  constexpr std::string_view kDigits = "0123456789abcdef";
  constexpr unsigned kLowNibble = 0xF;
  constexpr unsigned kNibbleBits = 4;
  std::string text;
  text.reserve(bytes.size() * 3);
  for (const std::uint8_t byte : bytes) {
    if (!text.empty()) {
      text += ' ';
    }
    text += kDigits[byte >> kNibbleBits];
    text += kDigits[byte & kLowNibble];
  }
  return text;
}

// La trame porte-t-elle cet identifiant, n'importe où dans sa tête ?
[[nodiscard]] bool HeadMentions(const std::vector<std::uint8_t>& head, std::uint32_t actor_id) {
  std::array<std::uint8_t, kActorIdBytes> wanted{};
  for (std::size_t rank = 0; rank < kActorIdBytes; ++rank) {
    wanted[rank] = static_cast<std::uint8_t>((actor_id >> (rank * kBitsPerByte)) & kByteMask);
  }
  return std::search(head.begin(), head.end(), wanted.begin(), wanted.end()) != head.end();
}

// Le décalage d'une case au personnage, et sa distance dans le carré du serveur.
struct Offset {
  int dx = 0;
  int dy = 0;
  [[nodiscard]] int square() const noexcept { return std::max(std::abs(dx), std::abs(dy)); }
};

[[nodiscard]] Offset OffsetFrom(const ro::sim::World& world, const Actor& actor) {
  const Actor* player = world.player();
  if (player == nullptr) {
    return {};
  }
  const ro::sim::CellPosition from = ro::sim::StandingCell(*player);
  const ro::sim::CellPosition to = ro::sim::StandingCell(actor);
  return Offset{.dx = static_cast<int>(to.x) - from.x, .dy = static_cast<int>(to.y) - from.y};
}

void WriteActorSummary(JsonWriter& out, const ro::sim::World& world, const Actor& actor) {
  const ro::sim::CellPosition cell = ro::sim::StandingCell(actor);
  const Offset offset = OffsetFrom(world, actor);
  out.Key("id").Int(actor.id);
  out.Key("kind").String(KindName(actor.kind));
  out.Key("job").Int(actor.look.job);
  out.Key("name").String(actor.name);
  out.Key("x").Int(cell.x);
  out.Key("y").Int(cell.y);
  out.Key("dx").Int(offset.dx);
  out.Key("dy").Int(offset.dy);
  out.Key("square_distance").Int(offset.square());
  out.Key("posture").String(PostureName(actor.posture));
  out.Key("health").Int(actor.health);
  out.Key("max_health").Int(actor.max_health);
  out.Key("familiar").String(FamiliarName(actor.familiar));
}

void WriteHelp(JsonWriter& out) {
  constexpr std::array<std::array<const char*, 2>, 8> kMethods = {{
      {"help", "cette liste"},
      {"state", "carte, personnage, rayon de vue du serveur, nombre d'acteurs"},
      {"actors",
       "acteurs connus du client ; radius=N (carré), kind=monster|player|npc|item, "
       "limit=N"},
      {"actor", "un acteur en détail ; id=N"},
      {"companions", "IA des compagnons : mode, cible, raison, refus de chaque monstre proche"},
      {"frames", "trames reçues récentes ; limit=N, opcode=0x0080, aid=N (cherché dans la tête)"},
      {"frame_tally", "verdicts des trames par opcode depuis la connexion"},
      {"log", "dernières lignes du journal ; limit=N, level=debug|info|warning|error"},
  }};
  out.BeginArray();
  for (const auto& [name, what] : kMethods) {
    out.BeginObject().Key("method").String(name).Key("does").String(what).EndObject();
  }
  out.EndArray();
}

void WriteState(JsonWriter& out, const DebugSources& sources) {
  const ro::sim::World& world = *sources.world;
  out.BeginObject();
  out.Key("map").String(world.map_name());
  out.Key("now_ms").Int(sources.now_ms);
  out.Key("server_view_cells").Int(world.session().view_cells);
  out.Key("actor_count").Int(static_cast<std::int64_t>(world.actors().size()));
  out.Key("player");
  if (const Actor* player = world.player(); player != nullptr) {
    out.BeginObject();
    WriteActorSummary(out, world, *player);
    out.EndObject();
  } else {
    out.Null();
  }
  out.EndObject();
}

void WriteActors(JsonWriter& out, const DebugRequest& request, const DebugSources& sources) {
  const ro::sim::World& world = *sources.world;
  const std::int64_t radius = request.IntParam("radius", 0);
  const std::string_view kind = request.TextParam("kind");
  const auto limit = static_cast<std::size_t>(
      std::max<std::int64_t>(request.IntParam("limit", kDefaultActorLimit), 0));
  std::vector<const Actor*> kept;
  for (const auto& [actor_id, actor] : world.actors()) {
    if (!kind.empty() && kind != KindName(actor.kind)) {
      continue;
    }
    if (radius > 0 && OffsetFrom(world, actor).square() > radius) {
      continue;
    }
    kept.push_back(&actor);
  }
  std::ranges::sort(kept, [&world](const Actor* left, const Actor* right) {
    const int left_cells = OffsetFrom(world, *left).square();
    const int right_cells = OffsetFrom(world, *right).square();
    return left_cells != right_cells ? left_cells < right_cells : left->id < right->id;
  });
  out.BeginObject();
  out.Key("total").Int(static_cast<std::int64_t>(kept.size()));
  out.Key("actors").BeginArray();
  for (std::size_t rank = 0; rank < kept.size() && rank < limit; ++rank) {
    out.BeginObject();
    WriteActorSummary(out, world, *kept[rank]);
    out.EndObject();
  }
  out.EndArray();
  out.EndObject();
}

// Rend faux quand l'acteur n'existe pas : la réponse devient une erreur.
[[nodiscard]] bool WriteActor(JsonWriter& out, const DebugRequest& request,
                              const DebugSources& sources) {
  const ro::sim::World& world = *sources.world;
  const auto actor_id = static_cast<std::uint32_t>(request.IntParam("id", 0));
  const Actor* actor = world.Find(actor_id);
  if (actor == nullptr) {
    return false;
  }
  out.BeginObject();
  WriteActorSummary(out, world, *actor);
  out.Key("position_x").Int(actor->position.x);
  out.Key("position_y").Int(actor->position.y);
  out.Key("moving");
  if (actor->movement.has_value()) {
    out.BeginObject()
        .Key("from_x")
        .Int(actor->movement->from.x)
        .Key("from_y")
        .Int(actor->movement->from.y)
        .Key("to_x")
        .Int(actor->movement->to.x)
        .Key("to_y")
        .Int(actor->movement->to.y)
        .EndObject();
  } else {
    out.Null();
  }
  out.Key("option").Int(actor->option);
  out.Key("summon_master_id").Int(actor->summon_master_id.value_or(0));
  out.Key("appeared_ms_ago");
  if (actor->appeared_ms.has_value() && sources.now_ms >= *actor->appeared_ms) {
    out.Int(sources.now_ms - *actor->appeared_ms);
  } else {
    out.Null();
  }
  out.EndObject();
  return true;
}

void WriteCompanions(JsonWriter& out, const DebugSources& sources) {
  const ro::sim::World& world = *sources.world;
  out.BeginArray();
  for (const DebugCompanion& companion : sources.companions) {
    out.BeginObject();
    out.Key("companion").String(companion.name);
    out.Key("actor_id").Int(companion.actor_id);
    const Actor* self = companion.actor_id != 0 ? world.Find(companion.actor_id) : nullptr;
    out.Key("present").Bool(self != nullptr);
    if (companion.brain != nullptr) {
      const ro::sim::CompanionBrain& brain = *companion.brain;
      out.Key("mode").Int(brain.mode_number());
      out.Key("target_id").Int(brain.target_id());
      out.Key("last_reason").String(ReasonName(brain.last_reason()));
    }
    // LES MONSTRES QU'IL VOIT, et ce que sa dernière recherche de cible en a dit.
    out.Key("monsters").BeginArray();
    if (self != nullptr && companion.brain != nullptr) {
      const ro::sim::CellPosition self_cell = ro::sim::StandingCell(*self);
      for (const auto& [actor_id, actor] : world.actors()) {
        if (actor.kind != ActorKind::kMonster) {
          continue;
        }
        const ro::sim::CellPosition cell = ro::sim::StandingCell(actor);
        if (!ro::sim::WithinCells(self_cell, cell, ro::sim::kCompanionSightCells)) {
          continue;
        }
        const std::optional<TargetRefusal> refusal = companion.brain->RefusalOf(actor_id);
        out.BeginObject();
        out.Key("id").Int(actor_id);
        out.Key("job").Int(actor.look.job);
        out.Key("distance").Int(ro::sim::WholeCellDistance(self_cell, cell));
        out.Key("posture").String(PostureName(actor.posture));
        out.Key("refusal");
        if (refusal.has_value()) {
          out.String(RefusalName(*refusal));
        } else {
          out.Null();
        }
        out.EndObject();
      }
    }
    out.EndArray();
    out.EndObject();
  }
  out.EndArray();
}

void WriteFrames(JsonWriter& out, const DebugRequest& request, const DebugSources& sources) {
  const auto limit = static_cast<std::size_t>(
      std::max<std::int64_t>(request.IntParam("limit", kDefaultFrameLimit), 0));
  const std::optional<std::int64_t> opcode = NumberOf(request.TextParam("opcode"));
  const std::optional<std::int64_t> aid = NumberOf(request.TextParam("aid"));
  // Les plus récentes d'abord, filtrées, puis rendues dans l'ordre d'arrivée.
  std::vector<const ro::net::FrameTally::Recent*> kept;
  const auto& recent = sources.frames->recent();
  for (auto each = recent.rbegin(); each != recent.rend() && kept.size() < limit; ++each) {
    if (opcode.has_value() && each->opcode != *opcode) {
      continue;
    }
    if (aid.has_value() && !HeadMentions(each->head, static_cast<std::uint32_t>(*aid))) {
      continue;
    }
    kept.push_back(&*each);
  }
  std::ranges::reverse(kept);
  out.BeginArray();
  for (const ro::net::FrameTally::Recent* frame : kept) {
    out.BeginObject()
        .Key("serial")
        .Int(frame->serial)
        .Key("opcode")
        .String(ro::net::HexOpcode(frame->opcode))
        .Key("outcome")
        .String(OutcomeName(frame->outcome))
        .Key("size")
        .Int(frame->size)
        .Key("head")
        .String(HexBytes(frame->head))
        .EndObject();
  }
  out.EndArray();
}

void WriteFrameTally(JsonWriter& out, const DebugSources& sources) {
  out.BeginArray();
  for (const ro::net::FrameTally::Entry& entry : sources.frames->All()) {
    out.BeginObject()
        .Key("opcode")
        .String(ro::net::HexOpcode(entry.opcode))
        .Key("applied")
        .Int(entry.applied)
        .Key("ignored")
        .Int(entry.ignored)
        .Key("unhandled")
        .Int(entry.unhandled)
        .Key("undecodable")
        .Int(entry.undecodable)
        .EndObject();
  }
  out.EndArray();
}

void WriteLog(JsonWriter& out, const DebugRequest& request) {
  const auto limit = static_cast<std::size_t>(
      std::max<std::int64_t>(request.IntParam("limit", kDefaultLogLimit), 0));
  const std::string_view level = request.TextParam("level");
  const std::vector<ro::core::LoggedLine> lines = ro::core::RecentLogLines();
  std::vector<const ro::core::LoggedLine*> kept;
  for (auto each = lines.rbegin(); each != lines.rend() && kept.size() < limit; ++each) {
    if (!level.empty() && level != LevelName(each->level)) {
      continue;
    }
    kept.push_back(&*each);
  }
  std::ranges::reverse(kept);
  out.BeginArray();
  for (const ro::core::LoggedLine* line : kept) {
    out.BeginObject()
        .Key("ms")
        .Int(line->since_start_ms)
        .Key("level")
        .String(LevelName(line->level))
        .Key("text")
        .String(line->text)
        .EndObject();
  }
  out.EndArray();
}

[[nodiscard]] std::string Failure(std::int64_t id, std::string_view message) {
  JsonWriter out;
  out.BeginObject()
      .Key("id")
      .Int(id)
      .Key("ok")
      .Bool(false)
      .Key("error")
      .String(message)
      .EndObject();
  return out.text();
}

}  // namespace

std::string AnswerDebugRequest(const DebugRequest& request, const DebugSources& sources) {
  if (sources.world == nullptr || sources.frames == nullptr) {
    return Failure(request.id, "aucune partie en cours");
  }
  JsonWriter out;
  out.BeginObject().Key("id").Int(request.id).Key("ok").Bool(true).Key("result");
  const std::string_view method = request.method;
  if (method == "help") {
    WriteHelp(out);
  } else if (method == "state") {
    WriteState(out, sources);
  } else if (method == "actors") {
    WriteActors(out, request, sources);
  } else if (method == "actor") {
    if (!WriteActor(out, request, sources)) {
      return Failure(request.id, "acteur inconnu du client");
    }
  } else if (method == "companions") {
    WriteCompanions(out, sources);
  } else if (method == "frames") {
    WriteFrames(out, request, sources);
  } else if (method == "frame_tally") {
    WriteFrameTally(out, sources);
  } else if (method == "log") {
    WriteLog(out, request);
  } else {
    return Failure(request.id, "méthode inconnue ; « help » les liste");
  }
  out.EndObject();
  return out.text();
}

std::string AnswerUnreadableLine() {
  return Failure(0, "ligne illisible : attendu « <id> <méthode> [clé=valeur]… »");
}

}  // namespace moonclient
