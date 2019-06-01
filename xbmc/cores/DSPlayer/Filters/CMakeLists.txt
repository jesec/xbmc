set(SOURCES AllocatorCommon.cpp
            asyncio.cpp
            asyncrdr.cpp
            BaseSharedRender.cpp
            FocusThread.cpp
            LavSettings.cpp
            madVRAllocatorPresenter.cpp
            MadvrSharedRender.cpp
            RendererSettings.cpp
            XBMCFileReader.cpp
            XBMCFileSource.cpp)

set(HEADERS AllocatorCommon.h
            AsyncCallback.h
            asyncio.h
            asyncrdr.h
            BaseSharedRender.h
            FocusThread.h
            IDirectVobSub.h
            IMpaDecFilter.h
            IMPCVideoDecFilter.h
            LAVAudioSettings.h
            LavSettings.h
            LAVSplitterSettings.h
            LAVVideoSettings.h
            madVRAllocatorPresenter.h
            MadvrSharedRender.h
            mvrInterfaces.h
            RendererSettings.h
            XBMCFileReader.h
            XBMCFileSource.h)

core_add_library(Filters)
