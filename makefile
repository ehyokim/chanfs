chanfs: chanfs.c fs.c fs_utils.c chan_parse.c
	gcc -Wall -g -pg -o chanfs chanfs.c fs.c fs_utils.c chan_parse.c -D_FILE_OFFSET_BITS=64 -pthread -lfuse -lcurl -lcjson 
