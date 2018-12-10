clientSNFS: 
	gcc -o clientSNFS clientSNFS.c -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags --libs`

clean:
	rm clientSNFS
