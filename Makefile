
main:
	gcc -g -Wall --shared -fPIC -o libtask.so hash.c libtask.c
	gcc -g -Wall --shared -fPIC -o libparam.so lua_debug.c libparam.c
	gcc -lm -ldl -Wall -g  log.c http.c epoll.c main.c ./liblua.so ./libparam.so ./libtask.so -o main


.PHONY : clean
clean:
	rm -f main ./libparam.so ./libtask.so error.log
