chanfs: chanfs.c fs.c fs_utils.c chan_parse.c
	gcc -Wall -g -o chanfs chanfs.c fs.c fs_utils.c chan_parse.c -D_FILE_OFFSET_BITS=64 -I/opt/homebrew/include -I/usr/local/include -L/opt/homebrew/lib -pthread -lfuse -lcurl -lcjson 
