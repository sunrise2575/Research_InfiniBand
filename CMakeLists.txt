project(ib_session)

cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_COMPILER g++)

add_compile_options(--std=c++11 -Wall -O3)

link_libraries(pthread ibverbs)

file(GLOB SOURCE_FILES
	${CMAKE_SOURCE_DIR}/*.cpp
)

add_executable( main ${SOURCE_FILES} )
