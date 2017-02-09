TESTS=sorttest

tests: $(TESTS)

clean:
	rm -rf *.o *~ core .depend *.mod.o .*.cmd *.ko *.mod.c .tmp_versions *.markers *.symvers modules.order a.out *# *init.c $(TESTS)

$(TESTS): % : %.c tests.h
	$(CC) -lrt -Wall --pedantic $< -o $@
