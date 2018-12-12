clientSNFS: clientSNFS.o 
	gcc clientSNFS.o -o clientSNFS `pkg-config fuse --cflags --libs`

clientSNFS.o: clientSNFS.c
	gcc -c clientSNFS.c -D_FILE_OFFSET_BITS=64

clean:
	rm *.o clientSNFS
