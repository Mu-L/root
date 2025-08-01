if(ROOT_pyroot_FOUND)
  set(api_macro_file runPyAPITestCppyy.C) # Uses new Cppyy API
  ROOTTEST_ADD_TEST(api
                    MACRO ${api_macro_file}
                    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTREF PyAPITest.ref
                    ENVIRONMENT CLING_STANDARD_PCH=none)

  # TPython::LoadMacro and TPython::Import are broken in the new Cppyy
  # https://bitbucket.org/wlav/cppyy/issues/65
  # For now, we rely on our own implementation of TPython
  if(NOT MSVC OR CMAKE_SIZEOF_VOID_P EQUAL 8 OR llvm13_broken_tests)
    if(NOT MSVC OR "${CMAKE_GENERATOR_PLATFORM}" MATCHES "Win32" OR win_broken_tests)
      ROOTTEST_ADD_TEST(class MACRO
                        runPyClassTest.C
                        WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}
                        OUTREF PyClassTest.ref
                        ENVIRONMENT CLING_STANDARD_PCH=none)
    endif()
  endif()

  ROOTTEST_ADD_TEST(cling
                    MACRO PyROOT_clingtests.py
                    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTREF PyROOT_clingtests.ref)
endif()
