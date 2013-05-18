CFLAGS = -Wall -O2 -g -I.
PROG = leanmqd
OBJS_MQCORE = mqcore/msgqueue.o mqcore/binding.o mqcore/binding_hash.o
OBJS_STOMP = stomp/stomp_proto.o stomp/stomp_subr.o
OBJS = lmq_main.o $(OBJS_STOMP) $(OBJS_MQCORE)

$(PROG): libsf/libsf.a $(OBJS) 
	make libsf 
	$(CC) -o $(PROG) $(OBJS) -L./libsf -lsf -lbsd

libsf/libsf.a: libsf/Makefile
	(cd libsf; make)

libsf/Makefile:
	(cd libsf; ./configure)

clean:
	(cd libsf; make clean)
	rm -f *.o mqcore/*.o stomp/*.o
	rm -f $(PROG)
