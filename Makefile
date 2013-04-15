CC =gcc
CFLAGS = -Wall -g3 -O0
BIN=bin
rediscp=rediscp

OBJ= rediscp.o lib/hiredis.o lib/net.c lib/sds.o
QUIET_LINK = @printf ' %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR);

all: $(rediscp)
	 @echo "compile done."

$(rediscp): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^
clean:
	$(RM) $(BIN)/* *.o
