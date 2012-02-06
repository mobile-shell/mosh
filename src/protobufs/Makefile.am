source = userinput.proto hostinput.proto transportinstruction.proto

PROTOC = protoc
AM_CPPFLAGS = $(protobuf_lite_CFLAGS)
AM_CXXFLAGS = -g -O2 --std=c++0x -pedantic -Werror -Wall -Wextra -Weffc++ -fno-default-inline -pipe

userinput.pb.cc: userinput.proto
	$(PROTOC) --cpp_out=$(builddir) -I$(srcdir) $<
hostinput.pb.cc: hostinput.proto
	$(PROTOC) --cpp_out=$(builddir) -I$(srcdir) $<
transportinstruction.pb.cc: transportinstruction.proto
	$(PROTOC) --cpp_out=$(builddir) -I$(srcdir) $<

.o.cc:
	$(CXX) $(CXXFLAGS) -o $@ $<

noinst_LIBRARIES = libmoshprotos.a

libmoshprotos_a_SOURCES = userinput.proto hostinput.proto transportinstruction.proto

libmoshprotos_a_LIBADD = userinput.pb.$(OBJEXT) hostinput.pb.$(OBJEXT) transportinstruction.pb.$(OBJEXT)

CLEANFILES = userinput.pb.cc hostinput.pb.cc transportinstruction.pb.cc userinput.pb.h hostinput.pb.h transportinstruction.pb.h
