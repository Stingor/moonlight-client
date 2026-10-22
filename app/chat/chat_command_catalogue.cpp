// Les commandes que le chat propose, et le menu des macros. Voir l'en-tête.
//
// ⚠ LES AIDES « / » SONT DES LITTÉRAUX MARQUÉS PAR `TrText` dans
// `slash_commands.cpp`, et c'est ICI qu'elles se traduisent, à la lecture :
// d'où l'appel de `Tr` sur une variable, dispensé nommément dans
// `scripts/check_translations.py`.

#include "chat/chat_command_catalogue.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

#include "chat/emotion_commands.h"
#include "chat/slash_commands.h"

namespace moonclient {
namespace {

constexpr char kSlash = '/';
constexpr char kAt = '@';
constexpr std::string_view kEllipsis = "…";

[[nodiscard]] ro::ui::ChatCommandHint SlashHint(const SlashCommand& command,
                                                std::string_view help) {
  return ro::ui::ChatCommandHint{.sigil = kSlash,
                                 .name = command.name,
                                 .aliases = command.aliases,
                                 .params = command.params,
                                 .help = help,
                                 .takes_argument = command.takes_argument};
}

// Ce que la colonne d'explication d'un menu montre : la première ligne de
// l'aide, coupée entre deux caractères.
[[nodiscard]] std::string ShortHelp(std::string_view help) {
  const std::string_view first = ro::ui::FirstHelpLine(help);
  if (first.size() <= kMacroHelpBytes) {
    return std::string(first);
  }
  std::string cut(ro::core::WithoutDanglingUtf8(first.substr(0, kMacroHelpBytes)));
  cut += kEllipsis;
  return cut;
}

[[nodiscard]] char FirstLetter(const ro::ui::ChatCommandHint& hint) {
  return hint.name.empty() ? ' ' : hint.name.front();
}

}  // namespace

void ChatCommandCatalogue::Refresh(std::uint8_t group_level, const ro::vfs::JobNames& job_names) {
  if (language_serial_ == ro::core::LanguageSerial() && group_level_ == group_level &&
      job_names_count_ == job_names.size()) {
    return;
  }
  language_serial_ = ro::core::LanguageSerial();
  group_level_ = group_level;
  job_names_count_ = job_names.size();
  argument_choices_.Rebuild(job_names);
  texts_.clear();
  hints_.clear();
  macro_items_.clear();
  macro_groups_.clear();
  const auto keep = [this](std::string text) -> std::string_view {
    return texts_.emplace_back(std::move(text));
  };

  // ── « / » : LES COMMANDES DU CLIENT, par nom ────────────────────────────────
  for (const SlashCommand& command : SlashCommands()) {
    if (command.staff && group_level == 0) {
      continue;
    }
    hints_.push_back(SlashHint(command, keep(ro::core::Tr(command.help))));
  }
  std::ranges::stable_sort(hints_, {}, &ro::ui::ChatCommandHint::name);
  const std::size_t client_commands = hints_.size();
  hints_.push_back(
      SlashHint(NumberedEmotionCommand(), keep(ro::core::Tr(NumberedEmotionCommand().help))));

  // ── « / » : LES ÉMOTICÔNES, la première forme en nom, les autres en alias ──
  const std::string_view emotion_help = keep(ro::core::Tr(EmotionCommandHelp()));
  std::vector<std::pair<std::uint16_t, std::vector<std::string_view>>> emotions;
  for (const EmotionCommandRow& row : EmotionCommandRows()) {
    const std::string_view name = row.name.substr(1);
    const auto known = std::ranges::find(
        emotions, row.emotion, &std::pair<std::uint16_t, std::vector<std::string_view>>::first);
    if (known == emotions.end()) {
      emotions.push_back({row.emotion, {name}});
    } else {
      known->second.push_back(name);
    }
  }
  for (const auto& [emotion, names] : emotions) {
    std::string aliases;
    for (std::size_t rank = 1; rank < names.size(); ++rank) {
      if (!aliases.empty()) {
        aliases += ' ';
      }
      aliases += names[rank];
    }
    hints_.push_back(ro::ui::ChatCommandHint{
        .sigil = kSlash, .name = names.front(), .aliases = keep(aliases), .help = emotion_help});
  }

  // ── « @ » : CE QUE LE GROUPE DU JOUEUR ATTEINT, tel que le serveur l'écrit ──
  const std::size_t first_server = hints_.size();
  for (const ServerAtCommand& command : ServerAtCommands()) {
    if (!ServerCommandOffered(command, group_level)) {
      continue;
    }
    hints_.push_back(ro::ui::ChatCommandHint{
        .sigil = kAt,
        .name = command.name,
        .aliases = command.aliases,
        .params = command.params,
        .help = command.help,
        .takes_argument = !command.params.empty(),
        .argument_choices = argument_choices_.Of(ServerCommandArgumentSource(command.name))});
  }

  // ── LE MENU DES MACROS : les commandes du client, puis celles du serveur
  // coupées par lettres. ⚠ PAS LES ÉMOTICÔNES : le menu a déjà leur grille.
  const auto item_of = [&](const ro::ui::ChatCommandHint& hint) {
    std::string text(1, hint.sigil);
    text += hint.name;
    if (hint.takes_argument) {
      text += ' ';
    }
    return ro::ui::ChatMacroCommand{.text = keep(std::move(text)),
                                    .help = keep(ShortHelp(hint.help))};
  };
  macro_items_.reserve(client_commands + (hints_.size() - first_server));
  std::vector<std::pair<std::string_view, std::size_t>> cuts;
  cuts.emplace_back(keep(ro::core::Tr(ClientCommandsMenuLabel())), 0);
  for (std::size_t rank = 0; rank < client_commands; ++rank) {
    macro_items_.push_back(item_of(hints_[rank]));
  }
  std::size_t group_start = macro_items_.size();
  for (std::size_t rank = first_server; rank < hints_.size(); ++rank) {
    const std::size_t in_group = macro_items_.size() - group_start;
    const bool new_letter =
        rank == first_server || FirstLetter(hints_[rank]) != FirstLetter(hints_[rank - 1]);
    if (rank == first_server || (new_letter && in_group >= kMacroServerGroupSize)) {
      group_start = macro_items_.size();
      cuts.emplace_back(std::string_view(), group_start);
    }
    macro_items_.push_back(item_of(hints_[rank]));
  }
  for (std::size_t rank = 0; rank < cuts.size(); ++rank) {
    const std::size_t begin = cuts[rank].second;
    const std::size_t end = rank + 1 < cuts.size() ? cuts[rank + 1].second : macro_items_.size();
    if (begin == end) {
      continue;
    }
    std::string_view label = cuts[rank].first;
    if (label.empty()) {
      // « @a – @d » : la première et la dernière lettre du sous-menu.
      label = keep(std::string(macro_items_[begin].text.substr(0, 2)) + " – " +
                   std::string(macro_items_[end - 1].text.substr(0, 2)));
    }
    macro_groups_.push_back(ro::ui::ChatMacroCommandGroup{
        .label = label,
        .items =
            std::span<const ro::ui::ChatMacroCommand>(macro_items_).subspan(begin, end - begin)});
  }
}

}  // namespace moonclient
