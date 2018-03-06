# project setting
CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )
PROJECT ( ib_session )

# set compiler
SET( CMAKE_CXX_COMPILER g++ )

# compile options
ADD_COMPILE_OPTIONS( --std=c++11 -Wall -O3 -lpthread )

# link libs
LINK_LIBRARIES( pthread ibverbs )

# cpp files
SET( SOURCE_FILES main.cpp ib_session.cpp play_wait_thread.cpp )

# make executable
ADD_EXECUTABLE( main ${SOURCE_FILES} )
