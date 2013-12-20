CXXFLAGS=-std=c++11 -stdlib=libc++ -g -O0 -DENTITYX_PYTHON_TEST_DATA=. $(shell python-config --includes)
LDFLAGS=-std=c++11 -stdlib=libc++ -g $(shell python-config --libs)
OBJS=PythonSystem.o

all: libs python_test

clean:
	rm -f libentityx_python.a python_test $(OBJS)

libs: libentityx_python.a

libentityx_python.a: $(OBJS)
	ar r $@ $<

python_test: libentityx_python.a
