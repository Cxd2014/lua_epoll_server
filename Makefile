
main:
	gcc -g -Wall --shared -fPIC -o libtask.so hash.c libtask.c
	gcc -g -Wall --shared -fPIC -o libparam.so lua_debug.c libparam.c
	gcc -lm -ldl -Wall -g  log.c http.c epoll.c main.c ./liblua.so ./libparam.so ./libtask.so -o main


.PHONY : clean
clean:
	rm -f main ./libparam.so ./libtask.so error.log

#####################################################
#下一步计划：支持传送html文件、支持多进程、改进编译方式
#           使用Lua作为配置文件、
#
#####################################################
