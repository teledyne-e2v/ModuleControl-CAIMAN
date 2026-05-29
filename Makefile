all:
	g++ `pkg-config --cflags glib-2.0` main.cpp i2c_caiman.cpp ModuleControl.cpp `pkg-config --libs glib-2.0` -g -O0 -o ModuleControl
	g++ `pkg-config --cflags glib-2.0` gst_main.cpp `pkg-config --libs glib-2.0` -o gstCommands
