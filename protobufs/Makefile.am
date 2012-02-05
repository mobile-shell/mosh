source = userinput.proto hostinput.proto transportinstruction.proto

PROTOC = protoc
AM_CXXFLAGS = `pkg-config --cflags protobuf-lite`

userinput.pb.cc: userinput.proto
	$(PROTOC) --cpp_out=$(builddir) $<
hostinput.pb.cc: hostinput.proto
	$(PROTOC) --cpp_out=$(builddir) $<
transportinstruction.pb.cc: transportinstruction.proto
	$(PROTOC) --cpp_out=$(builddir) $<

noinst_LIBRARIES = libmoshprotos.a

libmoshprotos_a_SOURCES = $(builddir)/userinput.pb.cc $(builddir)/hostinput.pb.cc $(builddir)/transportinstruction.pb.cc

CLEANFILES = $(builddir)/userinput.pb.cc $(builddir)/hostinput.pb.cc $(builddir)/transportinstruction.pb.cc $(builddir)/userinput.pb.h $(builddir)/hostinput.pb.h $(builddir)/transportinstruction.pb.h