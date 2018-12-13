
all: bleconfd

CPPFLAGS=-Wall -Wextra -I$(HOSTAPD_HOME)/src/utils -I$(HOSTAPD_HOME)/src/common -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
CPPFLAGS+=-I$(CJSON_HOME)
CPPFLAGS+=-I$(BLUEZ_HOME)
CPPFLAGS+=$(shell pkg-config --cflags glib-2.0) -g
LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=-pthread -L$(CJSON_HOME) -lcjson

WITH_BLUEZ=1

SRCS=\
  main.cc \
  jsonRpc.cc \
  rpclogger.cc \
  util.cc \
  rpcserver.cc \
  appsettings.cc \
  wifiservice.cc \
  netservice.cc

ifneq ($(WITH_BLUEZ),)
	CPPFLAGS+=-DWITH_BLUEZ
  BLUEZ_LIBS+=-L$(BLUEZ_HOME)/src/.libs/ -lshared-mainloop -L$(BLUEZ_HOME)/lib/.libs -lbluetooth-internal
  SRCS+=gattServer.cc
  SRCS+=beacon.cc
  SRCS+=bleclass.cc
endif

OBJS=$(patsubst %.cc, %.o, $(notdir $(SRCS)))
OBJS+=wpa_ctrl.o os_unix.o

clean:
	$(RM) -f $(OBJS) bleconfd

bleconfd: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o bleconfd $(BLUEZ_LIBS)

wpa_ctrl.o: $(HOSTAPD_HOME)/src/common/wpa_ctrl.c
	$(CC) $(CPPFLAGS) -c $< -o $@

os_unix.o: $(HOSTAPD_HOME)/src/utils/os_unix.c
	$(CC) $(CPPFLAGS) -c $< -o $@

gattServer.o: bluez/gattServer.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

bleclass.o: bluez/bleclass.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

beacon.o: bluez/beacon.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

appsettings.o: services/appsettings.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

wifiservice.o: services/wifiservice.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@

netservice.o: services/netservice.cc
	$(CXX) $(CPPFLAGS) -c $< -o $@
