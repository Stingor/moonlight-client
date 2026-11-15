#include "game/ui_answers.h"

// TOUT CE QUE `UiAnswers` TIENT À PART, ICI COMPLET : voir `Held`.
#include "ro/ui/bank_window.h"
#include "ro/ui/book_window.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/card_album_window.h"
#include "ro/ui/card_insert_window.h"
#include "ro/ui/cart_choice_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/description_window.h"
#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/ui/make_item_window.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/ui/npc_illustration_window.h"
#include "ro/ui/options_window.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/pet_hatch_window.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/player_report_window.h"
#include "ro/ui/repair_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/spell_list_window.h"
#include "ro/ui/storage_window.h"
#include "ro/ui/style_preview.h"
#include "ro/ui/talkbox_window.h"
#include "ro/ui/warp_list_window.h"
#include "ro/ui/weapon_refine_window.h"
#include "ro/ui/zone_capture.h"

#include "game/ui_chat_ask.h"

namespace moonclient {

UiAnswers::UiAnswers() = default;
UiAnswers::~UiAnswers() = default;
UiAnswers::UiAnswers(UiAnswers&&) noexcept = default;

}  // namespace moonclient
