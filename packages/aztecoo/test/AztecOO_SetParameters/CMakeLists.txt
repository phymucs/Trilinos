


TRIBITS_ADD_EXECUTABLE_AND_TEST(
  AztecOO_SetParameters
  SOURCES cxx_main.cpp
  ARGS -v
  COMM serial mpi
  )
