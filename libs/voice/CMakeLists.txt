# ro_voice — la voix en jeu : le fil du serveur voix, Opus, la gigue et le
# mixage (ADR 0135).
#
# Elle ne connaît que ro_core et ro_net, dont elle prend les sockets. Les
# appareils de son lui sont remis en fonctions : elle ne connaît pas SDL, et
# s'essaie donc partout, sous sanitizers compris.
#
# yaml-cpp lit les messages JSON du serveur, que le YAML contient ; Opus code
# la voix. Liés en PRIVATE : aucun en-tête public ne les nomme.

find_package(Opus CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

add_library(ro_voice STATIC
    src/opus_codec.cpp
    src/speaker_queue.cpp
    src/voice_engine.cpp
    src/voice_link.cpp
    src/voice_messages.cpp
    src/voice_mixer.cpp
    src/voice_wire.cpp)
add_library(ro::voice ALIAS ro_voice)

target_include_directories(ro_voice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_voice
    PUBLIC ro_core ro_net
    PRIVATE Opus::opus yaml-cpp::yaml-cpp ro_warnings)
