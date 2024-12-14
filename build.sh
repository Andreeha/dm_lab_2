mkdir -p lib
mkdir -p build
mkdir -p data

RBLIB="./lib/rb.o ./lib/compressor.o"
ZLIB="-L./external/zlib -l:libz.a"
LIBS="$RBLIB $ZLIB"
INCLUDE="-I./external/zlib -I ./include"

DEBUG="-ggdb -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-discarded-qualifiers"
BUILD="./build/"
SRC="./src/"

echo [COMPILE] ${SRC}rb.c
gcc -c ${SRC}rb.c -o ./lib/rb.o $INCLUDE $DEBUG

echo [COMPILE] ${SRC}compressor.c
gcc -c ${SRC}compressor.c -o ./lib/compressor.o $INCLUDE $DEBUG

# Table functions

TARGET=create_table
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=erase_table
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=delete_table
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

# Entry functions

TARGET=insert
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=delete
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=delete_many
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=find
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=find_many
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=edit
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=edit_many
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG

TARGET=backup
  echo [BUILD] ${src}${TARGET}.c
  gcc -o ${BUILD}${TARGET} ${SRC}${TARGET}.c $INCLUDE $LIBS $DEBUG
