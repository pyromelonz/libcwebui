include ../files.mk

MBEDTLS_VERSION = 3.6.0
MBEDTLS_DIR = mbedtls

FLAGS= -I . -I../../../lib/include -I../../../lib/third_party/src/miniz/ -I../../../lib/third_party/src/rb_tree/ \
	-I../../../lib/platform/include/ -I../../../lib/include/intern/  -I../../../lib/third_party/src/is_utf8/ \
	-I$(MBEDTLS_DIR)/include \
	-D_POSIX_SOURCE  -DSINGLE_MAIN -O0 -ggdb3 -Wno-unused-result \
	-Werror=return-type -Werror=missing-declarations -Werror=format \
	-DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

CFLAGS_EXTRA= -Werror=old-style-definition

MBEDTLS_LIBS = $(MBEDTLS_DIR)/library/libmbedtls.a \
               $(MBEDTLS_DIR)/library/libmbedcrypto.a \
               $(MBEDTLS_DIR)/library/libmbedx509.a

LIBS=-lc -ldl -lpthread -levent -levent_pthreads -lstdc++

# Exclude Python files from FILES
FILES_NO_PYTHON = $(filter-out %py_utils.c %py_plugin.c %py_api_functions.c, $(FILES))

.PHONY: all clean mbedtls

all: mbedtls
	@echo "C++  main.cpp"
	@$(CXX) $(FLAGS) -c main.cpp -o main.o
	@echo "CC   libcwebui + mbedTLS"
	@$(CC) $(FLAGS) $(CFLAGS_EXTRA) -std=gnu99 $(FILES_NO_PYTHON) main.o $(MBEDTLS_LIBS) -o main $(LIBS)
	@echo "LINK main"
	@echo ""
	@echo "Build complete: ./main"

mbedtls: $(MBEDTLS_DIR)/library/libmbedtls.a

$(MBEDTLS_DIR)/library/libmbedtls.a:
	@if [ ! -d "$(MBEDTLS_DIR)" ]; then \
		echo "Downloading mbedTLS $(MBEDTLS_VERSION)..."; \
		curl -L -o mbedtls.tar.bz2 https://github.com/Mbed-TLS/mbedtls/releases/download/v$(MBEDTLS_VERSION)/mbedtls-$(MBEDTLS_VERSION).tar.bz2; \
		tar xjf mbedtls.tar.bz2; \
		mv mbedtls-$(MBEDTLS_VERSION) $(MBEDTLS_DIR); \
		rm mbedtls.tar.bz2; \
	fi
	@echo "Building mbedTLS..."
	@cd $(MBEDTLS_DIR) && cmake -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF . && make -j4

clean:
	rm -f *.o main

clean-all: clean
	rm -rf $(MBEDTLS_DIR)
