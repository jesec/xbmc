set(SOURCES GUIDialogDSFilters.cpp
            GUIDialogDSManager.cpp
            GUIDialogDSPlayercoreFactory.cpp
            GUIDialogDSPlayerProcessInfo.cpp
            GUIDialogDSRules.cpp
            GUIDialogLAVAudio.cpp
            GUIDialogLAVSplitter.cpp
            GUIDialogLAVVideo.cpp
            GUIDialogSanear.cpp)

set(HEADERS GUIDialogDSFilters.h
            GUIDialogDSManager.h
            GUIDialogDSPlayercoreFactory.h
            GUIDialogDSPlayerProcessInfo.h
            GUIDialogDSRules.h
            GUIDialogLAVAudio.h
            GUIDialogLAVSplitter.h
            GUIDialogLAVVideo.h
            GUIDialogSanear.h)

core_add_library(Dialogs)
