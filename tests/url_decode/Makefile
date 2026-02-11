
OPTS= -std=c99 -ggdb3
#-O2

FLAGS=-I. -I../../lib/ -I../../lib/include/ -I../../lib/include/intern/ -I../../lib/third_party/src/simclist/ -I../../lib/third_party/src/rb_tree/ \
	-I../../lib/third_party/src/is_utf8/ -I../../lib/third_party/src/miniz/ -I../../lib/platform/include/ ${OPTS}


FILES=../../lib/src/convert.c

all: main.gcc #main.clang

#-fsanitize=address
main.gcc: ${FILES} main.c
	gcc ${FLAGS}  -fno-omit-frame-pointer main.c ${FILES} -o main.gcc -lc -lssl -lcrypto -ldl -lpthread -levent -levent_pthreads

main.clang: ${FILES} main.c
	clang ${FLAGS} -fsanitize=address -fno-omit-frame-pointer main.c ${FILES} -o main.clang -lc -lssl -lcrypto -ldl -lpthread -levent -levent_pthreads

clean:
	rm -f main.gcc main.clang


