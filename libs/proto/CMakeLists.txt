# ro_proto — le protocole, GÉNÉRÉ depuis les sources du serveur.
#
# Cette couche ne connaît que ro_core. Elle ne parle ni de socket ni de monde :
# elle décrit ce qui circule, pas comment on l'échange ni ce qu'on en fait.
#
# ⚠ Le contenu de generated/ est PRODUIT par tools/packetgen/packetgen.py, à
# partir des sources du serveur et de son compilateur. Ne pas le modifier à la
# main : la prochaine génération l'écraserait, et l'écart entre le client et le
# serveur reviendrait sans prévenir.
#
# La sortie est versionnée parce que le générateur exige les sources du serveur
# ET un compilateur C++ : un runner d'intégration continue n'a ni l'un ni
# l'autre. C'est donc la sortie que la CI compile, pas le générateur qu'elle
# relance.

add_library(ro_proto STATIC
    generated/ro/proto/common_packets.cpp
    generated/ro/proto/map_packets.cpp
    generated/ro/proto/packet_lengths.cpp
    generated/ro/proto/parsed_packets.cpp
    src/barter_packets.cpp
    src/battleground_packets.cpp
    src/buying_store_packets.cpp
    src/cart_packets.cpp
    src/cash_shop_packets.cpp
    src/chat_author_packets.cpp
    src/chat_room_packets.cpp
    src/config_packets.cpp
    src/craft_packets.cpp
    src/deal_packets.cpp
    src/guild_packets.cpp
    src/homunculus_packets.cpp
    src/companion_packets.cpp
    src/mercenary_packets.cpp
    src/pet_packets.cpp
    src/quest_marker_packets.cpp
    src/hat_effect_packets.cpp
    src/ledger_packets.cpp
    src/quest_packets.cpp
    src/manual_packets.cpp
    src/navigation_packets.cpp
    src/npc_shop_packets.cpp
    src/autospell_packets.cpp
    src/progress_packets.cpp
    src/repair_packets.cpp
    src/pincode.cpp
    src/snap_packets.cpp
    src/social_packets.cpp
    src/player_packets.cpp
    src/staff_packets.cpp
    src/store_search_packets.cpp
    src/title_packets.cpp
    src/vending_packets.cpp
    src/frame_reader.cpp
    src/framing.cpp)
add_library(ro::proto ALIAS ro_proto)

target_include_directories(ro_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/generated
                                          ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_proto
    PUBLIC ro_core
    PRIVATE ro_warnings)
