all : fixed_queue_test.out

fixed_queue_test.out : fixed_queue_test.cpp fixed_queue.h Makefile
	${CXX} -o $@ $< -g -O3 -Wall -lpthread

all : fixed_queue_test_asan.out

fixed_queue_test_asan.out : fixed_queue_test.cpp fixed_queue.h Makefile
	${CXX} -o $@ $< -g -O3 -Wall -fsanitize=address -fno-omit-frame-pointer -lpthread

clean:
	rm -f *.out
