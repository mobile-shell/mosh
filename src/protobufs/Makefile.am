source = userinput.proto hostinput.proto transportinstruction.proto

PROTOC = protoc
AM_CPPFLAGS = $(protobuf_lite_CFLAGS)
AM_CXXFLAGS = --std=c++0x -pedantic -Wall -Wextra -fno-default-inline -pipe

SUFFIXES = .proto .pb.cc

.proto.pb.cc:
	$(PROTOC) --cpp_out=$(builddir) -I$(srcdir) $<

noinst_LIBRARIES = libmoshprotos.a

libmoshprotos_a_SOURCES = $(source)
nodist_libmoshprotos_a_SOURCES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h)

BUILT_SOURCES = $(source:.proto=.pb.cc)
CLEANFILES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h)
