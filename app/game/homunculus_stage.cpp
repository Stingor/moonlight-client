#include "game/homunculus_stage.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/net/chat_target.h"
#include "ro/net/homunculus_frames.h"
#include "ro/net/skill.h"
#include "ro/render/sprite_catalog.h"

#include "chat/banned_input.h"
#include "game/game_pieces.h"
#include "game/homunculus_staff_words.h"

namespace moonclient {
namespace {

// LES COULEURS DES ACCUSÉS, celles du client, en `0xRRGGBB`.
constexpr std::uint32_t kStarvingColor = 0xFF0000;
constexpr std::uint32_t kFedColor = 0x64FFFF;
constexpr std::uint32_t kNotFedColor = 0xF51E1E;
constexpr std::uint32_t kAutoFeedColor = 0xFFFF64;
// Assez pour la fin de la ligne d'alerte de faim, dans toutes les langues.
constexpr std::size_t kHungerLineBytes = 96;
// L'émoticône que l'homoncule montre quand on n'a rien à lui donner : la
// goutte de sueur, que le client joue lui-même.
constexpr std::uint16_t kEmotionSweat = 4;

void Complain(const ro::core::Result<void>& sent, const char* what) {
  if (!sent) {
    ro::core::LogWarning("homoncule : {} non émis — {}", what, ro::core::Describe(sent.error()));
  }
}

[[nodiscard]] ro::ui::HomunculusIntimacyLabel LabelOf(ro::sim::HomunculusIntimacyGrade grade) {
  return static_cast<ro::ui::HomunculusIntimacyLabel>(grade);
}

// Le nom d'un objet, ou son numéro quand la table ne le connaît pas.
[[nodiscard]] std::string ItemName(const GamePieces& pieces, std::uint32_t item_id) {
  if (pieces.wardrobe.has_value()) {
    if (const ro::vfs::ItemEntry* entry = pieces.wardrobe->catalog().items.Find(item_id);
        entry != nullptr && !entry->name.empty()) {
      return entry->name;
    }
  }
  return std::to_string(item_id);
}

void Say(ro::sim::World& world, std::string text, std::uint32_t color, std::uint32_t now_ms) {
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), now_ms, color);
}

void WriteEcho(const ro::net::HomunculusEcho& echo, ro::sim::World& world, const GamePieces& pieces,
               std::uint32_t homunculus_id, std::uint32_t now_ms) {
  std::array<char, 256> line = {};
  switch (echo.kind) {
    case ro::net::HomunculusEchoKind::kNone:
      return;
    case ro::net::HomunculusEchoKind::kStarving:
      Say(world, ro::core::Tr("Ton homoncule meurt de faim. Nourris-le, sinon il te quittera."),
          kStarvingColor, now_ms);
      return;
    case ro::net::HomunculusEchoKind::kFed:
      // ⚠ LE SAC A DÉJÀ PERDU LA NOURRITURE : le serveur la retire avant
      // d'accuser, et le compte dit ce qui reste.
      std::snprintf(
          line.data(), line.size(), ro::core::Tr("L'homoncule a été nourri. [%s] - il en reste %d"),
          ItemName(pieces, echo.item_id).c_str(), world.inventory().AmountOf(echo.item_id));
      Say(world, line.data(), kFedColor, now_ms);
      return;
    case ro::net::HomunculusEchoKind::kNotFed:
      std::snprintf(line.data(), line.size(),
                    ro::core::Tr("La nourriture [%s] n'est pas disponible."),
                    ItemName(pieces, echo.item_id).c_str());
      Say(world, line.data(), kNotFedColor, now_ms);
      if (homunculus_id != 0) {
        world.RecordEmotion(homunculus_id,
                            ro::sim::Emotion{ro::sim::kBaseEmotionPack, kEmotionSweat}, now_ms);
      }
      return;
    case ro::net::HomunculusEchoKind::kAutoFeedOn:
      Say(world, ro::core::Tr("Alimentation automatique de l'homoncule activée"), kAutoFeedColor,
          now_ms);
      return;
    case ro::net::HomunculusEchoKind::kAutoFeedOff:
      Say(world, ro::core::Tr("Alimentation automatique de l'homoncule désactivée"), kAutoFeedColor,
          now_ms);
      return;
  }
}

[[nodiscard]] ro::ui::SkinImage IconOf(GamePieces& pieces, std::uint16_t skill_id,
                                       const ro::sim::SkillNode* node) {
  if (!pieces.skill_icons.has_value() || node == nullptr) {
    return {};
  }
  const ro::render::IconTexture& icon = pieces.skill_icons->Get(skill_id, node->idname);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

}  // namespace

void HomunculusStage::FollowOwner(const ro::sim::World& world) {
  // ⚠ LE NOM, ET NON `player_id` : celui-ci est le COMPTE, qui reste le même
  // d'un personnage à l'autre.
  if (world.player_name() == owner_name_) {
    return;
  }
  owner_name_ = world.player_name();
  homunculus_.Reset();
  species_job_ = 0;
  species_.clear();
}

ro::net::FrameOutcome HomunculusStage::ApplyFrame(const ro::net::Frame& frame,
                                                  ro::sim::World& world, const GamePieces& pieces,
                                                  std::uint32_t now_ms) {
  FollowOwner(world);
  ro::net::HomunculusEcho echo;
  const ro::net::FrameOutcome outcome = ro::net::ApplyHomunculusFrame(frame, homunculus_, echo);
  WriteEcho(echo, world, pieces, own_homunculus_id(), now_ms);
  return outcome;
}

void HomunculusStage::Stage(const ro::sim::World& world, GamePieces& pieces) {
  FollowOwner(world);
  // LE REPOS QUE LE JOUEUR LANCE LUI-MÊME rend l'homoncule absent : son onglet
  // ne le montre plus jusqu'au prochain appel.
  if (const ro::sim::Actor* player = world.player();
      player != nullptr && player->skill_use.has_value()) {
    static_cast<void>(
        homunculus_.NoticeOwnSkill(player->skill_use->skill_id, player->skill_use->started_ms));
  }
  const ro::sim::HomunculusSheet& sheet = homunculus_.sheet();
  // L'ESPÈCE SE LIT SUR L'ACTEUR, tant qu'il est en vue : une évolution change
  // sa classe sous nos yeux. Hors de vue, on garde la dernière lue.
  if (const ro::sim::Actor* actor = world.Find(homunculus_.actor_id());
      actor != nullptr && homunculus_.present() && actor->look.job != species_job_) {
    species_job_ = actor->look.job;
    species_ = pieces.wardrobe.has_value()
                   ? ro::render::NonPlayerResourceName(pieces.wardrobe->catalog(), species_job_)
                   : std::string();
  }
  rows_.clear();
  const ro::sim::SkillTree* tree = pieces.skill_tree.has_value() ? &*pieces.skill_tree : nullptr;
  for (const ro::sim::HomunculusSkill& skill : homunculus_.skills()) {
    const ro::sim::SkillNode* node = tree != nullptr ? tree->Find(skill.skill_id) : nullptr;
    rows_.push_back(ro::ui::HomunculusSkillRow{
        .skill_id = skill.skill_id,
        .name = node != nullptr ? std::string_view(node->display_name) : std::string_view(),
        .idname = node != nullptr ? std::string_view(node->idname) : std::string_view(),
        .icon = IconOf(pieces, skill.skill_id, node),
        .level = skill.level,
        .sp_cost = skill.sp_cost,
        .range_cells = skill.range_cells,
        .passive = skill.passive(),
        .self_cast =
            ro::sim::CompanionCastOf(skill.targeting, skill.level) == ro::sim::CompanionCast::kNow,
        .upgradable = skill.upgradable,
    });
  }
  view_ = ro::ui::HomunculusTabView{
      .present = homunculus_.present(),
      .sheet_known = homunculus_.sheet_known(),
      .name = sheet.name,
      .species = species_,
      .level = sheet.level,
      .renamed = sheet.renamed,
      .resting = sheet.resting,
      .alive = sheet.alive,
      .hp = sheet.hp,
      .max_hp = sheet.max_hp,
      .sp = sheet.sp,
      .max_sp = sheet.max_sp,
      .exp = sheet.exp,
      .exp_next = sheet.exp_next,
      .hunger = sheet.hunger,
      .intimacy = sheet.intimacy,
      .intimacy_label = LabelOf(ro::sim::GradeOfIntimacy(sheet.intimacy)),
      .auto_feed = homunculus_.auto_feed(),
      .atk = sheet.atk,
      .matk = sheet.matk,
      .hit = sheet.hit,
      .crit = sheet.crit,
      .def = sheet.def,
      .mdef = sheet.mdef,
      .flee = sheet.flee,
      .aspd = ro::sim::AspdFromAmotion(sheet.amotion_ms),
      .amotion_ms = sheet.amotion_ms,
      .attack_range_cells = sheet.attack_range_cells,
      .skill_points = sheet.skill_points,
      .skills = rows_,
  };
  // LA SECTION « STAFF » : ce que l'onglet sait de la classe, et le plafond de
  // niveau que le serveur a dit pour elle. Le droit, lui, est posé par la partie.
  const ro::sim::SessionSettings& rules = world.session();
  view_.evolved = HomunculusEvolved(species_job_);
  view_.mutations = mutations_;
  view_.staff_level_most =
      IsMutatedHomunculus(species_job_) ? rules.homunculus_s_max_level : rules.homunculus_max_level;
}

void HomunculusStage::Apply(const ro::ui::HomunculusTabAction& asked, ro::net::Connection& link,
                            ro::sim::World& world, std::uint32_t now_ms,
                            ro::ui::SkillDescriptionWindowState& description,
                            ro::ui::HomunculusTabState& tab) {
  // LA FAIM QUI DESCEND AU PALIER où il perd de l'intimité : le bandeau le dit.
  const ro::sim::HomunculusSheet& sheet = homunculus_.sheet();
  if (hunger_alarm_.Observe(homunculus_.present() && homunculus_.sheet_known(), sheet.hunger)) {
    std::array<char, kHungerLineBytes> suffix{};
    std::snprintf(suffix.data(), suffix.size(), ro::core::Tr("a faim : %d / %d."), sheet.hunger,
                  ro::sim::kHomunculusHungerMost);
    hunger_notice_ = ToastNotice{.name = sheet.name, .suffix = std::string(" ") + suffix.data()};
  }
  switch (asked.verb) {
    case ro::ui::HomunculusVerb::kNothing:
    // La fenêtre de l'IA et la proposition de recopie passent par
    // `CompanionAiStage`, que la partie appelle.
    case ro::ui::HomunculusVerb::kOpenAi:
    case ro::ui::HomunculusVerb::kAcceptAiProposal:
    case ro::ui::HomunculusVerb::kDeclineAiProposal:
      break;
    case ro::ui::HomunculusVerb::kRename:
      // LA BORNE D'ABORD, sur la forme envoyée, puis les balises et les mots.
      if (ro::core::Cp1252FromUtf8(asked.name).size() > ro::sim::kHomunculusNameMaxBytes) {
        tab.status = ro::core::Tr("Donne à ton homoncule un nom de 23 lettres au plus.");
        break;
      }
      if (std::optional<std::string> refused = CompanionNameRefusal(banned_, asked.name);
          refused.has_value()) {
        tab.status = std::move(*refused);
        break;
      }
      Complain(ro::net::RequestHomunculusRename(link, asked.name), "renommage");
      break;
    case ro::ui::HomunculusVerb::kFeed:
      Feed(link);
      break;
    case ro::ui::HomunculusVerb::kAutoFeed:
      Complain(ro::net::RequestHomunculusAutoFeed(link, asked.auto_feed), "auto-alimentation");
      break;
    case ro::ui::HomunculusVerb::kDelete: {
      const ro::core::Result<void> sent = ro::net::RequestHomunculusDelete(link);
      Complain(sent, "abandon");
      // LE CLIENT L'OUBLIE LUI-MÊME : le serveur n'envoie rien qui le dise.
      if (sent) {
        homunculus_.Forget();
      }
      break;
    }
    case ro::ui::HomunculusVerb::kSkillUp:
      // Le serveur renvoie la compétence et la fiche : niveau et points se
      // corrigent par eux, pas à l'aveugle.
      Complain(ro::net::RequestSkillUp(link, asked.skill_id), "montée de compétence");
      break;
    case ro::ui::HomunculusVerb::kUseSkill:
      static_cast<void>(UseSkill(link, world, asked.skill_id, asked.skill_level, now_ms));
      break;
    case ro::ui::HomunculusVerb::kStaff:
      SendStaffCommand(asked, link, world);
      break;
    case ro::ui::HomunculusVerb::kDescribeSkill:
      ro::ui::AskSkillDescription(description, asked.skill_id);
      break;
  }
}

// 🔴 COMPOSÉE PUIS ENVOYÉE COMME SI LE JOUEUR LA TAPAIT, et rien n'est validé
// ici : le serveur revalide selon le groupe du compte, et son refus arrive dans
// la conversation.
void HomunculusStage::SendStaffCommand(const ro::ui::HomunculusTabAction& asked,
                                       ro::net::Connection& link, const ro::sim::World& world) {
  const std::string line = HomunculusStaffCommand(asked);
  if (line.empty()) {
    return;
  }
  if (auto sent = ro::net::SendServerCommand(link, world.player_name(), line); !sent) {
    // ⚠ LA COMMANDE COMPOSÉE, ET PAS SEULEMENT L'ERREUR : sans elle, on ne sait
    // ni laquelle a échoué, ni sur quoi.
    ro::core::LogWarning("commande de staff « {} » non émise : {}", line,
                         ro::core::Describe(sent.error()));
  }
}

void HomunculusStage::Feed(ro::net::Connection& link) {
  Complain(ro::net::RequestHomunculusFeed(link), "nourrissage");
}

bool HomunculusStage::UseSkill(ro::net::Connection& link, ro::sim::World& world,
                               std::uint16_t skill_id, int level, std::uint32_t now_ms) {
  const ro::sim::HomunculusSkill* skill = FindSkill(skill_id);
  if (skill == nullptr) {
    return false;
  }
  const ro::sim::CompanionCast cast = ro::sim::CompanionCastOf(skill->targeting, skill->level);
  if (cast != ro::sim::CompanionCast::kNow && cast != ro::sim::CompanionCast::kNeedsAim) {
    SayCompanionCastRefusal(world, cast, now_ms);
    return false;
  }
  if (homunculus_.sheet().resting) {
    Say(world, ro::core::Tr("L'homoncule est au repos : il ne lance rien."), 0, now_ms);
    return false;
  }
  // Le niveau demandé, borné à l'appris.
  const int cast_level = std::clamp(level, 1, skill->level);
  if (cast == ro::sim::CompanionCast::kNeedsAim) {
    aim_request_ = CompanionAim{
        .companion = ro::sim::AiCompanion::kHomunculus,
        .skill_id = skill->skill_id,
        .skill_level = static_cast<std::uint16_t>(cast_level),
        .range_cells = skill->range_cells,
        .on_ground = ro::sim::SkillNeedsGround(ro::sim::KnownSkill{.targeting = skill->targeting}),
    };
    return false;
  }
  // La cible est l'homoncule : le serveur l'y ramènerait de toute façon.
  const ro::core::Result<void> sent = ro::net::RequestSkillOnTarget(
      link, skill->skill_id, static_cast<std::uint16_t>(cast_level), homunculus_.actor_id());
  Complain(sent, "skill");
  return sent.has_value();
}

void SayCompanionCastRefusal(ro::sim::World& world, ro::sim::CompanionCast cast,
                             std::uint32_t now_ms) {
  switch (cast) {
    case ro::sim::CompanionCast::kNow:
      return;
    case ro::sim::CompanionCast::kPassive:
      Say(world, ro::core::Tr("Skill passif : rien à lancer."), 0, now_ms);
      return;
    case ro::sim::CompanionCast::kNotLearned:
      Say(world, ro::core::Tr("Ce skill n'est plus connu."), 0, now_ms);
      return;
    case ro::sim::CompanionCast::kNeedsAim:
      // Rien à refuser : elle s'arme, voir `CompanionAim`.
      return;
  }
}

}  // namespace moonclient
