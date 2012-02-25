source = userinput.proto hostinput.proto transportinstruction.proto

PROTOC = protoc
AM_CPPFLAGS = $(protobuf_CFLAGS)
AM_CXXFLAGS = -Wno-long-long -Werror -Wall -fno-default-inline -pipe

SUFFIXES = .proto .pb.cc

.proto.pb.cc:
	$(PROTOC) --cpp_out=$(builddir) -I$(srcdir) $<

noinst_LIBRARIES = libmoshprotos.a

libmoshprotos_a_SOURCES = $(source)
nodist_libmoshprotos_a_SOURCES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h)

BUILT_SOURCES = $(source:.proto=.pb.cc)
CLEANFILES = $(source:.proto=.pb.cc) $(source:.proto=.pb.h)
