############################################################################
# CMakeLists.txt file for building ROOT RooFitMultiProcess package
# @author Patrick Bos, Netherlands eScience Center
############################################################################

ROOT_LINKER_LIBRARY(RooFitMultiProcess
        src/worker.cxx
        src/Messenger.cxx
        src/ProcessManager.cxx
        src/util.cxx
        src/Queue.cxx
        src/FIFOQueue.cxx
        src/PriorityQueue.cxx
        src/JobManager.cxx
        src/Job.cxx
        src/Config.cxx
        src/ProcessTimer.cxx
        src/HeatmapAnalyzer.cxx
    LIBRARIES
        Core
    DEPENDENCIES
        RooFitZMQ
)

target_link_libraries(RooFitMultiProcess PUBLIC Hist RooFitZMQ)
set(RooFitMultiProcess_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
target_include_directories(RooFitMultiProcess
        PRIVATE ${RooFitMultiProcess_INCLUDE_DIR}
        INTERFACE $<BUILD_INTERFACE:${RooFitMultiProcess_INCLUDE_DIR}>)

target_link_libraries(RooFitMultiProcess PRIVATE nlohmann_json::nlohmann_json)

ROOT_ADD_TEST_SUBDIRECTORY(test)

ROOT_INSTALL_HEADERS()
