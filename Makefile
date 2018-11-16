
all: bleconfd

CPPFLAGS=-Wall -Wextra -I$(HOSTAPD_HOME)/src/utils -I$(HOSTAPD_HOME)/src/common -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
CPPFLAGS+=-I$(CJSON_HOME)
CPPFLAGS+=$(shell pkg-config --cflags glib-2.0) -g
LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=-pthread -L$(CJSON_HOME) -lcjson

SRCS=\
  main.cc \
  wpaControl.cc \
  jsonRpc.cc \
  logger.cc

OBJS=$(patsubst %.cc, %.o, $(notdir $(SRCS)))
OBJS+=wpa_ctrl.o os_unix.o

clean:
	$(RM) -f $(OBJS) bleconfd

bleconfd: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o bleconfd 

wpa_ctrl.o: $(HOSTAPD_HOME)/src/common/wpa_ctrl.c
	$(CC) $(CPPFLAGS) -c $< -o $@

os_unix.o: $(HOSTAPD_HOME)/src/utils/os_unix.c
	$(CC) $(CPPFLAGS) -c $< -o $@
