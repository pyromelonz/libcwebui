
FLAGS=-I../../../lib -I../../../lib/include -Wswitch-enum


#FLAGS += -fsanitize=address
#FLAGS += -fsanitize=thread
#FLAGS += -fsanitize=undefined

PYTHON_PKG := $(shell pkg-config --modversion python3 )

PY_FLAGS = $(shell pkg-config --cflags python-$(PYTHON_PKG)-embed)
PY_LINK  = $(shell pkg-config --libs   python-$(PYTHON_PKG)-embed)

#
# bei neueren python-config versionen muss man wohl noch  --embed anhängen
# bei älteren geht das aber nicht
#

all: ../../testSite/src/test_plugin.so ssl_certs
	
	echo $(PY_LINK)
	
	cp ../../../lib/lib/libWebserver.so .
	
	$(CXX) $(FLAGS) $(PY_FLAGS) -c main.cpp  -O0 -ggdb3 
	$(CXX) $(FLAGS) main.o -O0 -ggdb3 -o main -lstdc++ -lWebserver -lssl -levent -levent_pthreads $(PY_LINK)  -L../../../lib/lib/ -Wl,-rpath=.
	

../../../lib/lib/libWebserver.so:
	${MAKE} -C ../../../lib

../../testSite/src/test_plugin.so: ../../../lib/lib/libWebserver.so
	${MAKE} -C ../../testSite/src/
	
ssl_certs: server.pem dh2048.pem root.pem

server.pem:
	@echo "Generating self-signed server certificate..."
	openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes -subj "/CN=localhost"
	cat server.key server.crt > server.pem
	rm -f server.key server.crt

dh2048.pem:
	@echo "Generating DH parameters (this may take a moment)..."
	openssl dhparam -out dh2048.pem 2048

root.pem: server.pem
	@echo "Copying server cert as root CA..."
	cp server.pem root.pem

clean:
	rm -f main.o main
	${MAKE} -C ../../../lib clean

clean_certs:
	rm -f server.pem dh2048.pem root.pem

