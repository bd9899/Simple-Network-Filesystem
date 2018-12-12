serverSNFS: serverSNFS.o
	gcc serverSNFS.o -o serverSNFS -pthread

serverSNFS.o: serverSNFS.c
	gcc -c serverSNFS.c

clean:
	rm *.o serverSNFS
