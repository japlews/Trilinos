
TRIBITS_ADD_EXECUTABLE_AND_TEST(
  ImportExport_test
  SOURCES cxx_main.cpp
  ARGS -v
  COMM mpi serial
  NUM_MPI_PROCS 1-4
  FAIL_REGULAR_EXPRESSION "tests FAILED"
  )
