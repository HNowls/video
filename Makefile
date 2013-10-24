SRC := main.c audio_thread.c packet_queue.c	
TARGET := video
CC	:= gcc
CFLAGS := -g
LFLAGS := -lavutil -lavformat -lavcodec -lswscale -lz -lm -lSDL

all:
	$(CC) $(CFLAGS) $(SRC) $(LFLAGS) -o $(TARGET)
	
clean:
	rm $(TARGET)
