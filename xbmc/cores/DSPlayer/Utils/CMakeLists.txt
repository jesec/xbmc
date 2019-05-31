set(SOURCES AudioEnumerator.cpp
            DSFileUtils.cpp
            DSFilterEnumerator.cpp
            DSTemplate.cpp
            IPinHook.cpp
            uri.cpp)

set(HEADERS AudioEnumerator.h
            DSFileUtils.h
            DSFilterEnumerator.h
            DSTemplate.h
            IPinHook.h
            SmartList.h
            StdString.h
            uri.h)

core_add_library(Utils)
