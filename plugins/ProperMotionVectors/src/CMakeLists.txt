INCLUDE_DIRECTORIES(
     .
     gui
)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(ProperMotionVectors_SRCS
     ProperMotionVectors.hpp
     ProperMotionVectors.cpp
)

SET(ProperMotionVectors_RES ../resources/ProperMotionVectors.qrc)
IF (${QT_VERSION_MAJOR} EQUAL "5")
     QT5_ADD_RESOURCES(ProperMotionVectors_RES_CXX ${ProperMotionVectors_RES})
ELSE()
     QT_ADD_RESOURCES(ProperMotionVectors_RES_CXX ${ProperMotionVectors_RES})
ENDIF()

SET(ProperMotionVectors_Qt_Libraries Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::OpenGL)

IF (STELLARIUM_GUI_MODE STREQUAL "Standard")
     LIST(APPEND ProperMotionVectors_SRCS
          gui/ProperMotionVectorsDialog.hpp
          gui/ProperMotionVectorsDialog.cpp
     )
     LIST(APPEND ProperMotionVectors_Qt_Libraries Qt${QT_VERSION_MAJOR}::Widgets)
ENDIF()

ADD_LIBRARY(ProperMotionVectors-static STATIC ${ProperMotionVectors_SRCS} ${ProperMotionVectors_RES_CXX})
TARGET_LINK_LIBRARIES(ProperMotionVectors-static ${ProperMotionVectors_Qt_Libraries})
SET_TARGET_PROPERTIES(ProperMotionVectors-static PROPERTIES OUTPUT_NAME "ProperMotionVectors")
SET_TARGET_PROPERTIES(ProperMotionVectors-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
ADD_DEPENDENCIES(AllStaticPlugins ProperMotionVectors-static)

SET_TARGET_PROPERTIES(ProperMotionVectors-static PROPERTIES FOLDER "plugins/ProperMotionVectors")
