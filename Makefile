
all: wpacontrol

CPPFLAGS=-Wall -Wextra -I$(HOSTAPD_HOME)/src/utils -I$(HOSTAPD_HOME)/src/common -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
CPPFLAGS+=-I$(CJSON_HOME)
#CPPFLAGS+=$(shell pkg-config --cflags glib-2.0)
#LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=-pthread -L$(CJSON_HOME) -lcjson

SRCS=wpacontrol.c \
  $(HOSTAPD_HOME)/src/common/wpa_ctrl.c \
  $(HOSTAPD_HOME)/src/utils/os_unix.c

OBJS=$(patsubst %.c, %.o, $(notdir $(SRCS)))

clean:
	$(RM) -f $(OBJS) wpacontrol

wpacontrol: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o wpacontrol

wpa_ctrl.o: $(HOSTAPD_HOME)/src/common/wpa_ctrl.c
	$(CC) $(CPPFLAGS) -c $< -o $@

os_unix.o: $(HOSTAPD_HOME)/src/utils/os_unix.c
	$(CC) $(CPPFLAGS) -c $< -o $@
