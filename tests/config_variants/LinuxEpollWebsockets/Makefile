include ../files.mk

PYTHON_PKG := $(shell pkg-config --modversion python3 )

PY_FLAGS = $(shell pkg-config --cflags python-$(PYTHON_PKG)-embed)
PY_LINK  = $(shell pkg-config --libs   python-$(PYTHON_PKG)-embed)

FLAGS= -I . -I../../../lib/include -I../../../lib/third_party/src/miniz/ -I../../../lib/third_party/src/rb_tree/ \
	-I../../../lib/platform/include/ -I../../../lib/include/intern/  -I../../../lib/third_party/src/is_utf8/ \
	-D_POSIX_SOURCE  -DSINGLE_MAIN -O3 -ggdb3 ${PY_FLAGS} -Wno-unused-result \
	-Werror=return-type -Werror=missing-declarations -Werror=format \
	-DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

CFLAGS_EXTRA= -Werror=old-style-definition

LIBS=-lc -lssl -lcrypto -ldl -lpthread -levent -levent_pthreads -lstdc++

.SILENT:
all:
	echo "C++  main.cpp"
	$(CXX) $(FLAGS) -c main.cpp  -o main.o
	echo "CC   ../lib/*.c"
	$(CC) $(FLAGS) $(CFLAGS_EXTRA) -std=gnu99 $(FILES) main.o  -o main $(LIBS) ${PY_LINK}
	echo "LINK main"

clean:
	rm -f *.o main	www_data.c
