cmake_minimum_required(VERSION 3.20)

project(MWRenderExample LANGUAGES CXX)

find_package(MWRender CONFIG REQUIRED)

add_executable(mwrender_api_example api_example.cpp)
target_compile_features(mwrender_api_example PRIVATE cxx_std_20)
target_link_libraries(mwrender_api_example PRIVATE MWRender::Core)

