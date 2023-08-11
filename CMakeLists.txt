cmake_minimum_required(VERSION 3.26)
project(obts_transceiver52m)

set(CMAKE_CXX_STANDARD 20)

add_executable(obts_transceiver52m
        runTransceiver.cpp
        convert.c
        convolve.c
        DummyLoad.cpp
        radioClock.cpp
        radioInterface.cpp
        radioVector.cpp
        sigProcLib.cpp
        Transceiver.cpp
        Resampler.cpp
        radioInterfaceResamp.cpp
        Threads.cpp
        Timeval.cpp
        Sockets.cpp
        BitVector.cpp
        UHDDevice.cpp
        LinkedLists.cpp
        smplbuf.cpp
        gsmtime.cpp
)

target_link_libraries(obts_transceiver52m pthread uhd)