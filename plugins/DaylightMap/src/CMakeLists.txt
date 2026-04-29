INCLUDE_DIRECTORIES(
     .
     gui
     ${CMAKE_BINARY_DIR}
     ${CMAKE_BINARY_DIR}/plugins/DaylightMap/src
     ${CMAKE_BINARY_DIR}/plugins/DaylightMap/src/gui
)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(DAYLIGHTMAP_SRCS
     DaylightMap.hpp
     DaylightMap.cpp
     gui/EarthShadowMapWidget.hpp
     gui/EarthShadowMapWidget.cpp
     gui/SunriseSunsetMapWidget.hpp
     gui/SunriseSunsetMapWidget.cpp
     gui/DaylightMapDialog.hpp
     gui/DaylightMapDialog.cpp
)

################# compiles resources files ############
SET(DAYLIGHTMAP_RES ../resources/DaylightMap.qrc)
IF (${QT_VERSION_MAJOR} EQUAL "5")
     QT5_ADD_RESOURCES(DAYLIGHTMAP_RES_CXX ${DAYLIGHTMAP_RES})
ELSE()
     QT_ADD_RESOURCES(DAYLIGHTMAP_RES_CXX ${DAYLIGHTMAP_RES})
ENDIF()

ADD_LIBRARY(DaylightMap-static STATIC ${DAYLIGHTMAP_SRCS} ${DAYLIGHTMAP_RES_CXX})
TARGET_LINK_LIBRARIES(DaylightMap-static Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg)
SET_TARGET_PROPERTIES(DaylightMap-static PROPERTIES OUTPUT_NAME "DaylightMap")
SET_TARGET_PROPERTIES(DaylightMap-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
ADD_DEPENDENCIES(AllStaticPlugins DaylightMap-static)

SET_TARGET_PROPERTIES(DaylightMap-static PROPERTIES FOLDER "plugins/DaylightMap")
