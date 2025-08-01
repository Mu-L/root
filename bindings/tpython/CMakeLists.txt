# Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

###########################################################
# CMakeLists.txt file for building TPython
###########################################################

ROOT_STANDARD_LIBRARY_PACKAGE(ROOTTPython
  HEADERS
    TPyArg.h
    TPyReturn.h
    TPython.h
  SOURCES
    src/TPyArg.cxx
    src/TPyClassGenerator.cxx
    src/TPyReturn.cxx
    src/TPython.cxx
  DICTIONARY_OPTIONS
    -writeEmptyRootPCM
  DEPENDENCIES
    Core
  LIBRARIES
    CPyCppyy
    # We link libTPython against Python libraries to compensate for the fact that libcppyy
    # is built with unresolved symbols. If we didn't do this, invoking TPython from C++
    # would not work.
    Python3::Python
)

ROOT_ADD_TEST_SUBDIRECTORY(test)
