mkdir -p lib
mkdir -p build
mkdir -p data
touch data/file.bin
touch data/file.binc
touch data/file.bind

RBLIB="./lib/rb.o"
ZLIB="-L./external/zlib -l:libz.a"
LIBS="$RBLIB $ZLIB"
INCLUDE="-I./external/zlib -I ./include"

DEBUG="-ggdb"
BUILD="./build/"
SRC="./src/"

echo [COMPILE] ${SRC}rb.c
gcc -c ${SRC}rb.c -o ./lib/rb.o $INCLUDE $DEBUG
echo [BUILD] ${SRC}rb_tree.c
gcc -o ${BUILD}rb_tree ${SRC}rb_tree.c $INCLUDE $LIBS $DEBUG

#echo [BUILD] ${src}compressor
#gcc -o ${BUILD}compressor ${SRC}compressor.c $INCLUDE $ZLIB $DEBUG

echo [BUILD] ${src}main.c
gcc -o ${BUILD}main ${SRC}main.c $INCLUDE $LIBS $DEBUG
echo [BUILD] ${src}main.c
echo [BUILD] ${src}main.c -> cmain
gcc -o ${BUILD}cmain ${SRC}main.c $INCLUDE $LIBS $DEBUG -DCREATE_TABLE 2> /dev/null
#echo [BUILD] ${src}main.c to 32-bit
#gcc -o ${BUILD}main32 -m32 ${SRC}main.c $INCLUDE $LIBS $DEBUG 2> /dev/null


