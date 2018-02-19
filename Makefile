CXX = g++
CXXFLAGS = -W -Wall --std=c++11 -libverbs -lpthread -O3

# Target
TARGET = main

# Thread Safe Container
OBJECT += thread_safe_queue.o
OBJECT += thread_safe_list.o
OBJECT += thread_safe_map.o

# Play-and-Stop Thread
OBJECT += play_n_stop_thread.o

# InfiniBand Session
OBJECT += ib_session_constructor.o
OBJECT += ib_session_destructor.o
OBJECT += ib_session_error.o
OBJECT += ib_session_private.o
OBJECT += ib_session_public.o
OBJECT += ib_session_private_threaded.o

all : $(TARGET)

$(TARGET) : $(OBJECT)
	$(CXX) -o $@ $< $(CXXFLAGS)

clean :
	rm -rf *.o
	rm -rf main
