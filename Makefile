all: qryp

qryp: y.tab.o qryp.o
	$(CC) -o $@ $(CFLAGS) $^

y.tab.o: query.y
	$(YACC) -d $<
	$(CC) -c -o $@ $(CFLAGS) y.tab.c

qryp.o: qryp.c qryp.h
	$(CC) -c -o $@ $(CFLAGS) $<

clean:
	rm -f *.o y.tab.h y.tab.c
