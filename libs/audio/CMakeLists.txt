# ro_audio — le son : la musique et les bruits.
#
# Cette couche ne lit AUCUN fichier. On lui remet des octets déjà lus, parce que
# les deux sortes de son ne viennent pas du même endroit — les bruits sont dans
# l'archive, la musique dans un dossier qui n'y est pas — et qu'une couche de
# son n'a pas à connaître deux façons d'ouvrir un fichier. Voir docs/adr/0013.
#
# Elle ne connaît pas davantage le monde : une position est trois nombres.

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_mixer CONFIG REQUIRED)

add_library(ro_audio STATIC
    src/ambient_sound.cpp
    src/audio_engine.cpp
    src/peak_limiter.cpp
    src/sound_channel.cpp
    src/sound_onset.cpp
    src/sound_placement.cpp)
add_library(ro::audio ALIAS ro_audio)

target_include_directories(ro_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ro_audio
    PUBLIC ro_core
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer ro_warnings)
