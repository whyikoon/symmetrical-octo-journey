CC       = gcc
CFLAGS	 = -Wall -O -g
OBJS     = sample.o
INCLUDE  = -I ./include -I ./include/exports/ -I ./
TARGET	 = quickstart
LIBVAR	+= -liot_sdk \
           -liot_hal \
           -liot_tls \
           -lpthread \
           -lrt
LIBPATH = -L ./lib

vpath %.c ./

PK = a1f5HigxNBo
DN = sample
DS = RAF9Tn9jjhct5xCv0acuhe9pvai52ldt
DOMAIN = iot-as-mqtt.cn-shanghai.aliyuncs.com
DID = -DDEVICE_NAME=\"${DN}\" \
	  -DPRODUCT_KEY=\"${PK}\" \
	  -DDEVICE_SECRET=\"${DS}\" \
	  -DMQTT_DOMAIN=\"${DOMAIN}\" \
          -DENDPOINT=\"${ENDPOINT}\"

sample.o:sample.c 
	$(CC) $(CFLAGS) $(INCLUDE) ${DID} -c $^ 

OBJS= sample.o

.PHONY:all
all:$(OBJS) $(LIB)
	$(CC) $(CFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS) $(LIBVAR) $(LIBPATH)

.PHONY:clean
clean:
	rm -f *.o
	rm -f $(TARGET)
