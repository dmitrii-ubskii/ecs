add_executable(ecs_tests
    catch_main.cpp
    test_ecs.cpp
)

target_compile_features(ecs_tests PRIVATE cxx_std_20)

target_include_directories(ecs_tests
    PRIVATE
        .
        ../include
        ../third_party/catch2
        ../extern/deeyou/include
)

