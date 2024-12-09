LIBS="./lib/rb.o -L./external/zlib -l:libz.a"
INCLUDE="-I./external/zlib -I ./include"

DEBUG="-ggdb"
BUILD="./build/"
SRC="./src/"

gcc -c ${SRC}rb.c -o ./lib/rb.o $INCLUDE $DEBUG

gcc -o ${BUILD}rb_tree ${SRC}rb_tree.c $INCLUDE $LIBS $DEBUG

gcc -o ${BUILD}main ${SRC}main.c $INCLUDE $LIBS $DEBUG
gcc -o ${BUILD}main32 -m32 ${SRC}main.c $INCLUDE $LIBS $DEBUG 2> /dev/null


