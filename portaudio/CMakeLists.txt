
add_definitions(
#    -DPA_USE_ASIO
    -DPA_USE_DS
    -DPA_USE_WMME
    -DPA_USE_WASAPI
    -DPA_USE_WDMKS
)

include_directories(src/common src/os/win)

add_library(
    portaudio SHARED

    include/pa_asio.h
    include/pa_jack.h
    include/pa_linux_alsa.h
    include/pa_mac_core.h
    include/pa_win_ds.h
    include/pa_win_wasapi.h
    include/pa_win_waveformat.h
    include/pa_win_wdmks.h
    include/pa_win_wmme.h
    include/portaudio.h

    src/common/pa_allocation.c
    src/common/pa_allocation.h
    src/common/pa_converters.c
    src/common/pa_converters.h
    src/common/pa_cpuload.c
    src/common/pa_cpuload.h
    src/common/pa_debugprint.c
    src/common/pa_debugprint.h
    src/common/pa_dither.c
    src/common/pa_dither.h
    src/common/pa_endianness.h
    src/common/pa_front.c
    src/common/pa_gitrevision.h
    src/common/pa_hostapi.h
    src/common/pa_memorybarrier.h
    src/common/pa_process.c
    src/common/pa_process.h
    src/common/pa_ringbuffer.c
    src/common/pa_ringbuffer.h
    src/common/pa_stream.c
    src/common/pa_stream.h
    src/common/pa_trace.c
    src/common/pa_trace.h
    src/common/pa_types.h
    src/common/pa_util.h
#    src/hostapi/alsa/pa_linux_alsa.c
#    src/hostapi/asihpi/pa_linux_asihpi.c
#    src/hostapi/asio/iasiothiscallresolver.cpp
#    src/hostapi/asio/iasiothiscallresolver.h
#    src/hostapi/asio/pa_asio.cpp
#    src/hostapi/coreaudio/pa_mac_core.c
#    src/hostapi/coreaudio/pa_mac_core_blocking.c
#    src/hostapi/coreaudio/pa_mac_core_blocking.h
#    src/hostapi/coreaudio/pa_mac_core_internal.h
#    src/hostapi/coreaudio/pa_mac_core_old.c
#    src/hostapi/coreaudio/pa_mac_core_utilities.c
#    src/hostapi/coreaudio/pa_mac_core_utilities.h
    src/hostapi/dsound/pa_win_ds.c
    src/hostapi/dsound/pa_win_ds_dynlink.c
    src/hostapi/dsound/pa_win_ds_dynlink.h
#    src/hostapi/jack/pa_jack.c
#    src/hostapi/oss/pa_unix_oss.c
#    src/hostapi/oss/recplay.c
    src/hostapi/skeleton/pa_hostapi_skeleton.c
    src/hostapi/wasapi/pa_win_wasapi.c
    src/hostapi/wdmks/pa_win_wdmks.c
    src/hostapi/wmme/pa_win_wmme.c
#    src/os/unix/pa_unix_hostapis.c
#    src/os/unix/pa_unix_util.c
#    src/os/unix/pa_unix_util.h
    src/os/win/pa_win_coinitialize.c
    src/os/win/pa_win_coinitialize.h
    src/os/win/pa_win_hostapis.c
    src/os/win/pa_win_util.c
    src/os/win/pa_win_waveformat.c
    src/os/win/pa_win_wdmks_utils.c
    src/os/win/pa_win_wdmks_utils.h
    src/os/win/pa_x86_plain_converters.c
    src/os/win/pa_x86_plain_converters.h

    portaudio.def
)

set_target_properties(portaudio PROPERTIES PREFIX "")

if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set_target_properties(portaudio PROPERTIES OUTPUT_NAME "portaudio_x64")
else()
    set_target_properties(portaudio PROPERTIES OUTPUT_NAME "portaudio_x86")
endif()

target_link_libraries(portaudio ole32 advapi32 setupapi winmm)

install(TARGETS portaudio)
