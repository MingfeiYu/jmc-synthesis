include_directories(catch2) # v2.13.7

file(GLOB_RECURSE FILENAMES *.cpp)

add_executable(run_tests ${FILENAMES})
target_link_libraries(run_tests PUBLIC mockturtle)
target_compile_definitions(run_tests PUBLIC JMC_DB_PATH="${PROJECT_SOURCE_DIR}/experiments/databases/xag_mc_min_6_db")
target_compile_definitions(run_tests PUBLIC JMC_EXPERIMENTS_PATH="${PROJECT_SOURCE_DIR}/experiments/")
target_compile_definitions(run_tests PUBLIC CATCH_CONFIG_CONSOLE_WIDTH=300)

add_test(NAME run_tests COMMAND run_tests)
