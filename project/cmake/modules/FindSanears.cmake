# - Builds DSPlayer dependencies as external project
# Once done this will define
#
# SANEARS_FOUND - system has SANEARS
# SANEARS_LIBRARIES

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (DS_PLATFORM 64)
else ()
  set (DS_PLATFORM 32)
endif ()

set(BS2B_LIBRARY_DEBUG "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Debug${DS_PLATFORM}/bs2b.lib" )
set(BS2B_LIBRARY_RELEASE "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Release${DS_PLATFORM}/bs2b.lib" )

set(SANEAR_LIBRARY_DEBUG "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Debug${DS_PLATFORM}/sanear.lib" )
set(SANEAR_LIBRARY_RELEASE "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Release${DS_PLATFORM}/sanear.lib" )

set(SOUNDTOUCH_LIBRARY_DEBUG "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Debug${DS_PLATFORM}/soundtouch.lib" )
set(SOUNDTOUCH_LIBRARY_RELEASE "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Release${DS_PLATFORM}/soundtouch.lib" )

set(SOXR_LIBRARY_DEBUG "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Debug${DS_PLATFORM}/soxr.lib" )
set(SOXR_LIBRARY_RELEASE "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Release${DS_PLATFORM}/soxr.lib" )

set(ZITARESAMPLER_LIBRARY_DEBUG "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Debug${DS_PLATFORM}/zita-resampler.lib" )
set(ZITARESAMPLER_LIBRARY_RELEASE "${CORE_SOURCE_DIR}/xbmc/cores/DSPlayer/Libs/Sanear/Release${DS_PLATFORM}/zita-resampler.lib" )

set(SANEARS_FOUND 1)

include(SelectLibraryConfigurations)
select_library_configurations(BS2B)
select_library_configurations(SANEAR)
select_library_configurations(SOUNDTOUCH)
select_library_configurations(SOXR)
select_library_configurations(ZITARESAMPLER)

set(SANEARS_LIBRARIES ${BS2B_LIBRARY} ${SANEAR_LIBRARY} ${SOUNDTOUCH_LIBRARY} ${SOXR_LIBRARY} ${ZITARESAMPLER_LIBRARY} )

mark_as_advanced(SANEARS_FOUND BS2B_LIBRARY SANEAR_LIBRARY SOUNDTOUCH_LIBRARY SOXR_LIBRARY ZITARESAMPLER_LIBRARY )
