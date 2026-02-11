include ../files.mk

WOLFSSL_VERSION = 5.7.4
WOLFSSL_DIR = wolfssl

FLAGS= -I . -I../../../lib/include -I../../../lib/third_party/src/miniz/ -I../../../lib/third_party/src/rb_tree/ \
	-I../../../lib/platform/include/ -I../../../lib/include/intern/  -I../../../lib/third_party/src/is_utf8/ \
	-I$(WOLFSSL_DIR)/include -I$(WOLFSSL_DIR) \
	-D_POSIX_SOURCE  -DSINGLE_MAIN -O0 -ggdb3 -Wno-unused-result \
	-Werror=return-type -Werror=missing-declarations -Werror=format \
	-DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

CFLAGS_EXTRA= -Werror=old-style-definition

WOLFSSL_LIBS = $(WOLFSSL_DIR)/src/.libs/libwolfssl.a

LIBS=-lc -ldl -lpthread -levent -levent_pthreads -lstdc++ -lm

# Exclude Python files from FILES
FILES_NO_PYTHON = $(filter-out %py_utils.c %py_plugin.c %py_api_functions.c, $(FILES))

.PHONY: all clean wolfssl

all: wolfssl
	@echo "C++  main.cpp"
	@$(CXX) $(FLAGS) -c main.cpp -o main.o
	@echo "CC   libcwebui + wolfSSL"
	@$(CC) $(FLAGS) $(CFLAGS_EXTRA) -std=gnu99 $(FILES_NO_PYTHON) main.o $(WOLFSSL_LIBS) -o main $(LIBS)
	@echo "LINK main"
	@echo ""
	@echo "Build complete: ./main"

wolfssl: $(WOLFSSL_DIR)/src/.libs/libwolfssl.a

$(WOLFSSL_DIR)/src/.libs/libwolfssl.a:
	@if [ ! -d "$(WOLFSSL_DIR)" ]; then \
		echo "Downloading wolfSSL $(WOLFSSL_VERSION)..."; \
		curl -L -o wolfssl.tar.gz https://github.com/wolfSSL/wolfssl/archive/refs/tags/v$(WOLFSSL_VERSION)-stable.tar.gz; \
		tar xzf wolfssl.tar.gz; \
		mv wolfssl-$(WOLFSSL_VERSION)-stable $(WOLFSSL_DIR); \
		rm wolfssl.tar.gz; \
	fi
	@echo "Building wolfSSL..."
	@cd $(WOLFSSL_DIR) && autoreconf -i && ./configure --enable-static --disable-shared --enable-singlethreaded=no && make -j4

clean:
	rm -f *.o main

clean-all: clean
	rm -rf $(WOLFSSL_DIR)
