// L'étage du carnet de chasse MVP : la composition de la vue (rangs, membres,
// propriété, invitation, sprites, plan, étoile, alertes, lignes), les faits et
// l'aperçu des `<MVPL>`, les tombes de la minimap.

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/render/skin_pieces.h"

#include "mvp/mvp_tracker_stage.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// L'ART D'UN CRÉNEAU SCRIPTÉ, que le serveur ne nomme pas : la carte le
// désigne, et son libellé reste celui du MVP, en anglais. Plusieurs monstres
// pour une carte : le sprite tourne de l'un à l'autre.
//
// ⚠ LA CARTE SE COMPARE TELLE QUELLE : les noms du catalogue sont en
// minuscules, comme ceux de cette table.
struct ScriptedSlotArt {
  std::string_view map;
  std::string_view label;
  // Les monstres entre lesquels tourne le sprite, bornes comprises.
  std::uint16_t first_mob_id = 0;
  std::uint16_t last_mob_id = 0;
};

constexpr std::array<ScriptedSlotArt, 4> kScriptedSlotArt = {{
    {"lhz_dun03", "MVP BioLab 3", 1646, 1651},
    {"lhz_dun04", "MVP BioLab 4", 2235, 2241},
    {"niflheim", "Lord of Death", 1373, 1373},
    {"thana_boss", "Thanatos Phantom", 1708, 1708},
}};

[[nodiscard]] const ScriptedSlotArt* ScriptedArtOf(std::string_view map) {
  const auto found = std::ranges::find(kScriptedSlotArt, map, &ScriptedSlotArt::map);
  return found != kScriptedSlotArt.end() ? &*found : nullptr;
}

// Le sprite PEINT d'un créneau scripté : le premier monstre, ou celui de
// l'instant quand la carte en porte plusieurs.
[[nodiscard]] std::uint16_t TurningSprite(const ScriptedSlotArt& art, std::uint32_t now_ms) {
  const auto span = static_cast<std::uint32_t>(art.last_mob_id - art.first_mob_id) + 1U;
  if (span <= 1U) {
    return art.first_mob_id;
  }
  return static_cast<std::uint16_t>(art.first_mob_id + (now_ms / kMvpScriptedSpriteTurnMs) % span);
}

// Les textes de l'aperçu d'un lien : assez pour une heure et un nom de carte.
constexpr std::size_t kTipLineBytes = 160;
constexpr std::int64_t kSecondsPerMinute = 60;
// L'infobulle d'une tombe compose deux lignes : la fenêtre et l'âge.
constexpr std::size_t kTombTextsPerTomb = 2;
// L'extension du fichier par lequel la table des noms lisibles désigne une
// carte.
constexpr std::string_view kMapGridExtension = ".gat";

}  // namespace

std::optional<bool> MvpOwnership(const ro::sim::MvpGroup& group, std::string_view own_name_utf8) {
  if (group.group_id == 0) {
    return std::nullopt;
  }
  // LE SERVEUR DIT NOTRE COMPTE : la propriété s'en lit, que notre tête soit ou
  // non parmi les membres. Un compte non rattaché (zéro) ne possède rien.
  if (group.self_user_id.has_value()) {
    return *group.self_user_id != 0 && *group.self_user_id == group.owner_user_id;
  }
  if (own_name_utf8.empty()) {
    return std::nullopt;
  }
  // ⚠ UN COMPTE A PLUSIEURS TÊTES, et c'est le COMPTE qui possède : la ligne de
  // notre personnage porte l'identifiant de son compte.
  const auto mine = std::ranges::find(group.members, own_name_utf8, &ro::sim::MvpMember::name);
  if (mine == group.members.end()) {
    return std::nullopt;
  }
  return mine->user_id == group.owner_user_id;
}

const char* MvpNameHint(const ro::ui::MvpSlotView* row) {
  // L'heure de mort ou l'instant exact : de quoi composer un `<MVPL>`.
  const bool carries_respawn = row != nullptr && (row->kill_time_s > 0 || row->exact_respawn_s > 0);
  return carries_respawn ? Tr("Maj+clic : partager le respawn dans le chat")
                         : Tr("Maj+clic : lien dans le chat");
}

const ro::ui::MvpLinkMenuFacts* MvpTrackerStage::FactsFor(const MvpLink& link) {
  return &facts_kept_.emplace_back(ro::ui::MvpLinkMenuFacts{
      .import_refusal = ImportRefusal(link),
      .open_refusal = enabled_ ? nullptr : ro::ui::MvpTrackerOffText(),
      .tomb_known = link.tomb_x >= 0 && link.tomb_y >= 0 && !link.map.empty(),
      // Tombe inconnue, la carte suffit : « Y aller » y guide.
      .map_known = !link.map.empty()});
}

ro::ui::MvpLinkTip MvpTrackerStage::TipOf(const MvpLink& link, std::int64_t local_now_s) const {
  ro::ui::MvpLinkTip tip;
  tip.name = link.name.empty() ? std::string(Tr("MVP")) : link.name;
  tip.map = link.map;
  std::array<char, kTipLineBytes> text{};
  // UN INSTANT EXACT NE S'INVENTE PAS : quelqu'un a payé un Convex Mirror, et le
  // dire évite qu'on le prenne pour une supposition. À défaut, la fenêtre.
  const std::string exact = link.exact_respawn_s > 0 ? MvpClockText(link.exact_respawn_s) : "";
  if (!exact.empty()) {
    std::snprintf(text.data(), text.size(), Tr("Retour à %s (Convex Mirror)"), exact.c_str());
    tip.respawn_line = text.data();
  } else if (link.kill_time_s > 0 && link.delay1_min > 0) {
    const std::int64_t from_s = link.kill_time_s + link.delay1_min * kSecondsPerMinute;
    const std::int64_t to_s = from_s + link.delay2_min * kSecondsPerMinute;
    const std::string from = MvpClockText(from_s);
    const std::string to = MvpClockText(to_s);
    if (!from.empty() && !to.empty()) {
      std::snprintf(text.data(), text.size(), Tr("Retour entre %s et %s"), from.c_str(),
                    to.c_str());
      tip.respawn_line = text.data();
    }
  }
  if (link.kill_time_s > 0) {
    if (const std::string death = MvpClockText(link.kill_time_s); !death.empty()) {
      std::snprintf(text.data(), text.size(), Tr("Mort à %s"), death.c_str());
      tip.death_line = text.data();
    }
  }
  // ⚠ -1 ET NON 0 : la cellule (0, 0) existe, et une tombe peut s'y trouver.
  if (link.tomb_x >= 0 && link.tomb_y >= 0) {
    std::snprintf(text.data(), text.size(), Tr("Tombe en (%d, %d)"), static_cast<int>(link.tomb_x),
                  static_cast<int>(link.tomb_y));
    tip.tomb_line = text.data();
  }
  // L'ÉCHÉANCE SE JUGE À L'HEURE DU SERVEUR, celle du lecteur : un lien relu le
  // lendemain ne promet plus rien. Un instant exact passé est un retour SÛR ;
  // une fenêtre passée, seulement probable.
  if (MvpLinkPassed(link, ServerNow(local_now_s), &book_)) {
    tip.passed_line = link.exact_respawn_s > 0 ? Tr("Passé : le MVP est revenu.")
                                               : Tr("Passé : le MVP est sans doute déjà revenu.");
  }
  return tip;
}

std::span<const ro::ui::MinimapTomb> MvpTrackerStage::TombsOn(std::string_view map, bool shown,
                                                              std::int64_t local_now_s) {
  tombs_.clear();
  tomb_texts_.clear();
  if (!shown) {
    return tombs_;
  }
  const std::int64_t server_now_s = ServerNow(local_now_s);
  const std::vector<ro::sim::MvpTomb> found = ro::sim::MvpTombsOn(book_, map, server_now_s);
  // ⚠ RÉSERVÉ D'AVANCE : les tombes pointent dans ces textes, qu'un vecteur qui
  // grandit déplacerait.
  tomb_texts_.reserve(kTombTextsPerTomb * found.size());
  std::array<char, kTipLineBytes> text{};
  for (const ro::sim::MvpTomb& tomb : found) {
    ro::ui::MinimapTomb shown_tomb{
        .cell = {.x = tomb.cell_x, .y = tomb.cell_y}, .exact = tomb.exact, .slot_id = tomb.slot_id};
    if (const ro::sim::MvpSlot* slot = book_.FindSlot(tomb.slot_id); slot != nullptr) {
      shown_tomb.label = SlotLabel(*slot);
    }
    // LA FENÊTRE, dans les mêmes mots que l'aperçu d'un lien.
    if (const std::optional<ro::sim::MvpWindow> window = book_.WindowOf(tomb.slot_id);
        window.has_value()) {
      const std::string from = MvpClockText(window->from_s);
      const std::string to = MvpClockText(window->to_s);
      text[0] = '\0';
      if (window->exact && !from.empty()) {
        std::snprintf(text.data(), text.size(), Tr("Retour à %s (Convex Mirror)"), from.c_str());
      } else if (!from.empty() && !to.empty()) {
        std::snprintf(text.data(), text.size(), Tr("Retour entre %s et %s"), from.c_str(),
                      to.c_str());
      }
      shown_tomb.window_line = tomb_texts_.emplace_back(text.data());
    }
    // L'ÂGE DU SIGNALEMENT : une tombe vue il y a une heure n'est plus une
    // nouvelle.
    if (const ro::sim::MvpObservation* observation = book_.FindObservation(tomb.slot_id);
        observation != nullptr && observation->reported_at_s > 0) {
      const std::string age = ro::ui::FormatMvpDuration(server_now_s - observation->reported_at_s);
      std::snprintf(text.data(), text.size(), Tr("Signalée il y a %s"), age.c_str());
      shown_tomb.age_line = tomb_texts_.emplace_back(text.data());
    }
    tombs_.push_back(shown_tomb);
  }
  return tombs_;
}

void MvpTrackerStage::ComposeView(MvpTrackerWindows& windows, const MvpTrackerContext& context,
                                  const MvpTrackerSettings& hunt) {
  ComposeSlots(context.now_ms);

  member_views_.clear();
  const ro::sim::MvpGroup& group = book_.group();
  // NOS LIGNES sont celles de notre compte, toutes ses sessions comprises —
  // seulement quand le serveur le dit : un nom ne désignerait qu'une tête.
  const std::uint32_t self_user_id = group.self_user_id.value_or(0);
  for (const ro::sim::MvpMember& member : group.members) {
    member_views_.push_back(
        ro::ui::MvpMemberView{.user_id = member.user_id,
                              .level = member.level,
                              .online = member.online,
                              .name = member.name,
                              .our_account = self_user_id != 0 && member.user_id == self_user_id});
  }

  // LES LIGNES DÉTACHÉES RÉSOLUES : une ligne dont le créneau n'est pas au
  // catalogue ne se dessine pas, et n'aimante donc pas ses voisines.
  line_views_.clear();
  for (std::size_t line = 0; line < lines_.size(); ++line) {
    if (line >= line_slots_.size() || line_slots_[line] == ro::sim::kMvpNoSlot) {
      continue;
    }
    line_views_.push_back(ro::ui::MvpPinnedLineView{.line = static_cast<int>(line),
                                                    .slot_id = line_slots_[line],
                                                    .mob_id = lines_[line].mob_id,
                                                    .map = lines_[line].map,
                                                    .x = lines_[line].x,
                                                    .y = lines_[line].y});
  }

  ComposeSprites(windows.window.wanted_sprites, context, hunt);

  // LE PLAN DE LA CARTE SURVOLÉE à l'image précédente, et rien sinon.
  ro::ui::MvpMapPreview preview;
  if (!windows.window.wanted_map.empty()) {
    preview_map_ = windows.window.wanted_map;
    preview.map = preview_map_;
    preview.image = ImageAt("map\\" + preview_map_ + ".bmp");
  } else {
    preview_map_.clear();
  }

  // LES ALERTES, le temps qu'elles se montrent ; éteint, aucune.
  if (hunt.enabled) {
    ComposeAlerts(context.now_ms);
  } else {
    alert_views_.clear();
  }

  const std::optional<bool> ownership = MvpOwnership(group, context.own_name);

  // L'INVITATION CONGÉDIÉE ne revient ni avec l'instantané ni avec la poignée
  // de main ; une autre, si, et la même aussi quand son groupe la repousse —
  // `ApplyFrame` lève alors le congé.
  std::uint32_t invite_shown = book_.pending_invite();
  if (invite_shown != 0 && invite_shown == invite_dismissed_) {
    invite_shown = 0;
  } else if (invite_shown != 0) {
    invite_dismissed_ = 0;
  }

  view_ = ro::ui::MvpTrackerView{
      .server_now_s = ServerNow(context.local_now_s),
      .now_ms = context.now_ms,
      .enabled = hunt.enabled,
      .link_ready = verified_,
      .catalog_known = book_.catalog_known(),
      .slots = slot_views_,
      .group_id = group.group_id,
      .owner_user_id = group.owner_user_id,
      .group_name = group.name,
      .members = member_views_,
      .owner_known = ownership.has_value(),
      .i_own = ownership.value_or(false),
      .result_code = book_.last_result(),
      // Le refus d'un « Accepter » ou d'un « Refuser » s'écrit dans la modale,
      // pas deux fois.
      .result_shown = book_.last_result() != 0 && !result_in_modal_ &&
                      context.now_ms - book_.last_result_ms() < ro::ui::kMvpResultShownMs,
      .pending_invite = invite_shown,
      .pending_invite_name = invite_shown != 0 ? book_.pending_invite_name() : std::string_view(),
      // ⚠ LA PATIENCE EST BORNÉE : une réponse perdue ne doit pas griser les
      // boutons pour toujours.
      .invite_waiting =
          invite_answer_sent_ && context.now_ms - invite_answer_ms_ < kMvpAnswerPatienceMs,
      .invite_refusal = invite_refusal_,
      .invite_gone = invite_gone_,
      .sprites = sprite_art_,
      .map_preview = preview,
      .favorite_off = ImageAt("menu_icon\\bt_mvpfav.bmp"),
      .favorite_on = ImageAt("menu_icon\\bt_mvpfav_on.bmp"),
      .lines = line_views_,
      .alerts = alert_views_};
}

void MvpTrackerStage::ComposeAlerts(std::uint32_t now_ms) {
  // Une alerte montrée assez longtemps s'éteint pour de bon.
  std::erase_if(lit_alerts_, [now_ms](const LitAlert& lit) {
    return now_ms - lit.lit_ms > ro::ui::kMvpAlertShownMs;
  });
  alert_views_.clear();
  alert_clocks_.clear();
  // ⚠ RÉSERVÉ D'AVANCE : les vues pointent dans ces heures.
  alert_clocks_.reserve(lit_alerts_.size());
  for (const LitAlert& lit : lit_alerts_) {
    const auto found = std::ranges::find(slot_views_, lit.slot_id, &ro::ui::MvpSlotView::slot_id);
    if (found == slot_views_.end()) {
      continue;
    }
    ro::ui::MvpAlertView alert{.slot_id = lit.slot_id,
                               .label = found->label,
                               .map = found->map_label.empty() ? found->map : found->map_label,
                               .exact = found->window_known && found->exact};
    // UN INSTANT EXACT SE DIT À L'HEURE : « de retour à HH:MM ».
    if (alert.exact) {
      alert.at_clock = alert_clocks_.emplace_back(MvpClockText(found->window_from_s));
    }
    alert_views_.push_back(alert);
  }
}

void MvpTrackerStage::ComposeSlots(std::uint32_t now_ms) {
  slot_views_.clear();
  for (const ro::sim::MvpSlot& slot : book_.slots()) {
    ro::ui::MvpSlotView row{.slot_id = slot.slot_id,
                            .mob_id = slot.mob_id,
                            .delay1_ms = slot.delay1_ms,
                            .delay2_ms = slot.delay2_ms,
                            .map_cells_x = slot.map_cells_x,
                            .map_cells_y = slot.map_cells_y,
                            .map = slot.map,
                            .map_label = MapLabel(slot.map)};
    const ScriptedSlotArt* art = slot.mob_id == 0 ? ScriptedArtOf(slot.map) : nullptr;
    const ro::sim::MvpObservation* observation = book_.FindObservation(slot.slot_id);
    // LE SPRITE PEINT tourne sur un scripté ; LE MONSTRE DÉSIGNÉ par le nom est
    // celui qui est réellement tombé, quand on le sait.
    if (slot.mob_id != 0) {
      row.sprite_mob_id = slot.mob_id;
      row.link_mob_id = slot.mob_id;
    } else {
      row.sprite_mob_id = art != nullptr ? TurningSprite(*art, now_ms) : 0;
      if (observation != nullptr && observation->mob_id != 0) {
        row.link_mob_id = observation->mob_id;
      } else if (art != nullptr) {
        row.link_mob_id = art->first_mob_id;
      }
    }
    row.label = SlotLabel(slot);
    row.favorite = book_.IsFavorite(slot.slot_id);
    if (const std::optional<ro::sim::MvpWindow> window = book_.WindowOf(slot.slot_id);
        window.has_value()) {
      row.window_known = true;
      row.window_from_s = window->from_s;
      row.window_to_s = window->to_s;
      row.exact = window->exact;
    }
    if (observation != nullptr) {
      row.observed = true;
      row.source_raw = observation->source_raw;
      row.kill_time_s = observation->kill_time_s;
      row.exact_respawn_s = observation->exact_respawn_s;
      row.tomb_x = observation->tomb_x;
      row.tomb_y = observation->tomb_y;
      row.reported_at_s = observation->reported_at_s;
      row.by_name = observation->by_name;
    }
    slot_views_.push_back(row);
  }
}

std::string_view MvpTrackerStage::SlotLabel(const ro::sim::MvpSlot& slot) const {
  if (!slot.name.empty()) {
    return slot.name;
  }
  if (const ScriptedSlotArt* art = slot.mob_id == 0 ? ScriptedArtOf(slot.map) : nullptr;
      art != nullptr) {
    return art->label;
  }
  return Tr("(créneau scripté)");
}

std::string_view MvpTrackerStage::MapLabel(const std::string& map) {
  if (pieces_.map_names == nullptr || map.empty()) {
    return {};
  }
  // 🔴 GARDÉ PAR CARTE : la table se consulte une fois, et la vue rendue pointe
  // dans une valeur qui ne bouge plus. Inconnu, le nom se garde vide.
  auto found = map_label_cache_.find(map);
  if (found == map_label_cache_.end()) {
    const std::optional<std::string_view> label =
        pieces_.map_names->DisplayNameOf(map + std::string(kMapGridExtension));
    found = map_label_cache_.emplace(map, std::string(label.value_or(std::string_view()))).first;
  }
  return found->second;
}

void MvpTrackerStage::ComposeSprites(const std::vector<std::uint16_t>& wanted,
                                     const MvpTrackerContext& context,
                                     const MvpTrackerSettings& hunt) {
  sprite_art_.clear();
  if (pieces_.wardrobe == nullptr) {
    return;
  }
  // 🔴 DEUX SPRITES NEUFS PAR IMAGE, table et lignes confondues : quatre-vingts
  // .spr lus d'un coup figeraient l'ouverture. Une instance par classe, gardée :
  // la vignette ne se recompose que quand son image change.
  int loads = 0;
  const ro::render::VignetteFit fit = ro::render::VignetteFit::kCenteredShrinkOnly;
  const auto composed = [this](std::uint16_t mob_id) {
    return std::ranges::find(sprite_art_, mob_id, &ro::ui::MvpSpriteArt::mob_id) !=
           sprite_art_.end();
  };
  const auto compose = [&](std::uint16_t mob_id, MonsterSprite& sprite, bool animate) {
    const MonsterSprite::Shown shown =
        sprite.Compose(MonsterSprite::Framing{.sprite_class = mob_id,
                                              .direction = 0,
                                              .animate = animate,
                                              .box_width = context.sprite_box_px,
                                              .box_height = context.sprite_box_px,
                                              .fit = fit},
                       context.now_ms);
    // Une case de liste ne parle pas : les sons des images se jettent.
    static_cast<void>(sprite.TakeSounds());
    sprite_art_.push_back(ro::ui::MvpSpriteArt{.mob_id = mob_id, .image = shown.image});
  };
  for (const std::uint16_t mob_id : wanted) {
    if (mob_id == 0 || composed(mob_id)) {
      continue;
    }
    auto found = sprites_.find(mob_id);
    if (found == sprites_.end()) {
      if (loads >= kMvpSpriteLoadsPerFrame) {
        continue;
      }
      found = sprites_.try_emplace(mob_id, *pieces_.wardrobe).first;
      ++loads;
    }
    compose(mob_id, found->second, hunt.look.animate_sprites);
  }
  // 🔴 LA DEMANDE EST CELLE DE L'IMAGE PRÉCÉDENTE, les rangs ceux de celle-ci : un
  // scripté qui tourne, ou un rang qui entre au défilement, désigne un monstre
  // qu'on n'a pas encore demandé. Déjà chargé, il se peint dans l'image même —
  // sans budget, puisque rien ne se lit ; seul un chargement neuf attend son
  // tour. Les lignes détachées désignent les mêmes créneaux que ces rangs.
  //
  // ⚠ FIGÉ SUR SA PREMIÈRE IMAGE : seuls les rangs dessinés à l'image précédente
  // s'animent. Un rang hors de vue garde ainsi une vignette qui ne se recompose
  // plus, au lieu de quatre-vingts silhouettes recomposées à chaque pas de
  // l'attente ; dessiné, il est demandé, et s'anime dès l'image suivante.
  for (const ro::ui::MvpSlotView& row : slot_views_) {
    if (row.sprite_mob_id == 0 || composed(row.sprite_mob_id)) {
      continue;
    }
    if (const auto loaded = sprites_.find(row.sprite_mob_id); loaded != sprites_.end()) {
      compose(row.sprite_mob_id, loaded->second, false);
    }
  }
}

ro::ui::SkinImage MvpTrackerStage::ImageAt(std::string_view path) {
  if (pieces_.images == nullptr) {
    return {};
  }
  std::string full(ro::render::kSkinRoot);
  full += path;
  // ⚠ `GetOurs` : le chemin est écrit dans nos sources, en UTF-8. L'absence
  // est retenue par le cache, et ne coûte qu'une fois.
  const ro::render::IconTexture& icon = pieces_.images->GetOurs(full);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

}  // namespace moonclient
