HEADERS := $(wildcard *.h)

all : fixed_queue_test.out

fixed_queue_test.out : fixed_queue_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -lpthread

all : fixed_queue_test.asan.out

fixed_queue_test.asan.out : fixed_queue_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -fsanitize=address -fno-omit-frame-pointer -lpthread



all : fixed_queue_test_2.out

fixed_queue_test_2.out : fixed_queue_test_2.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -lpthread

all : fixed_queue_test_2.asan.out

fixed_queue_test_2.asan.out : fixed_queue_test_2.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -fsanitize=address -fno-omit-frame-pointer -lpthread



all : free_allocate_test.out 
	
free_allocate_test.out : free_allocate_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -lpthread -latomic

all : free_allocate_test.asan.out

free_allocate_test.asan.out : free_allocate_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -fsanitize=address -fno-omit-frame-pointer -lpthread -latomic



all : free_allocate_test_2.out 

free_allocate_test_2.out : free_allocate_test_2.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -lpthread -latomic

all : free_allocate_test_2.asan.out

free_allocate_test_2.asan.out : free_allocate_test_2.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -fsanitize=address -fno-omit-frame-pointer -lpthread -latomic



all : free_allocate_test_3.out 

free_allocate_test_3.out : free_allocate_test_3.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -lpthread -latomic

all : free_allocate_test_3.asan.out

free_allocate_test_3.asan.out : free_allocate_test_3.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -mcx16 -fsanitize=address -fno-omit-frame-pointer -lpthread -latomic



all : linked_queue_test.out

linked_queue_test.out : linked_queue_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -lpthread -latomic

all : linked_queue_test.asan.out

linked_queue_test.asan.out : linked_queue_test.cpp ${HEADERS} Makefile
	${CXX} -o $@ $< -g -O3 -Wall ${CFLAGS} -fsanitize=address -fno-omit-frame-pointer -lpthread -latomic



clean:
	rm -f *.out
