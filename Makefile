# compile for  xiami.com client
# Copyright 2011 SNDA
#


#PLATFORM=arm

ifeq ($(PLATFORM), arm)
CC = /opt/freescale/usr/local/gcc-4.1.2-glibc-2.5-nptl-3/arm-none-linux-gnueabi/bin/arm-none-linux-gnueabi-g++
LD = /opt/freescale/usr/local/gcc-4.1.2-glibc-2.5-nptl-3/arm-none-linux-gnueabi/bin/arm-none-linux-gnueabi-ld
CXX = $(CC)
else
CC = gcc
LD = ld
CXX = g++
endif

PROG = curl-cli
LIB_JSON_DIR= lib_json_0.5

SRCS = ${LIB_JSON_DIR}/json_value.cpp ${LIB_JSON_DIR}/json_reader.cpp ${LIB_JSON_DIR}/json_writer.cpp

SRCS+= data_manager.cpp xiami_cli.cpp

OBJS = ${LIB_JSON_DIR}/json_value.o ${LIB_JSON_DIR}/json_reader.o ${LIB_JSON_DIR}/json_writer.o
OBJS += data_manager.o xiami_cli.o

ifneq ($(PLATFORM),arm)
LOCALLDFLAGS += -lstdc++  -lrt -L./curl-7.21.7/host -lcurl -lpthread 
else
LOCALLDFLAGS += -L./curl-7.21.7/arm -lcurl -lz -lpthread
endif

CFLAGS += -Wall -g -I./lib_json_0.5 -I./curl-7.21.7/include/curl
CXXFLAGS += -Wall -g #-I./lib_json_0.5 -I./curl-7.21.7/include/curl


OAUTH_CFLAGS = -Wall -g -I -Wall -g  -I./curl-7.21.7/include/curl -I./openssl-0.9.8g -I./liboauth-0.9.5
ifneq ($(PLATFORM), arm)
OAUTH_LD_FLAG = -L./curl-7.21.7/host -lcurl -L./openssl-0.9.8g/host -lcrypto -L./liboauth-0.9.5/host -loauth
else
OAUTH_LD_FLAG = -L./curl-7.21.7/arm -lcurl  -L./openssl-0.9.8g/arm -lcrypto -L./liboauth-0.9.5/arm -loauth
endif

#CPPFLAGS += $(INCLUDE)

OTEST = otest

all: $(PROG)

$(PROG): $(OBJS)
	$(CXX)  $(CFLAGS)  -o $@ $^ $(LOCALLDFLAGS)


otest: 
	gcc $(OAUTH_CFLAGS) -o otest  oauth_login.c $(OAUTH_LD_FLAG)

install:
	install  $(PROG) /bin/$(PROG) 
clean:
	rm -f $(PROG) $(OBJS) 

.PHONY: all install clean

