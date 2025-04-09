set -e 

if ! [ -f config.h ]; then
    cp config.def.h config.h
fi

: ${GCC:=gcc}
: ${CLANG:=clang}

if [ -z $EMPATH ]; then
    : ${EX_CFLAGS:=""}
    : ${EX_LDFLAGS:=""}

    if [ -z $CC ]; then 
        if hash "$CLANG" 2>/dev/null; then
            CC="$CLANG"
            AUTO_CFLAGS_DBG="-g -glldb"
        elif hash "$GCC" 2>/dev/null; then
            CC="$GCC"
            AUTO_CFLAGS_DBG="-g -ggdb"
        elif hash tcc 2>/dev/null; then
            CC="tcc"
            : ${AR:="tcc -ar"}
            : ${SERIAL:=1}
            AUTO_CFLAGS_DBG="-g"
        else 
            echo "could not find a C compiler. please manually set the CC variable"
        fi 
    else 
        : ${CC:=clang}
    fi 

    : ${AR:="ar"}
    : ${BUILD_CC:=$CC}
else
    : ${CC:=$EMPATH/emcc}
    : ${BUILD_CC:=clang}
    : ${EX_CFLAGS:="-O3"}
    : ${EX_LDFLAGS:=""}
    : ${AR:=$EMPATH/emar}
fi

AUTO_CFLAGS="-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-comment -Wno-format -Wno-sign-compare -Wno-char-subscripts -Wno-implicit-fallthrough -Wno-missing-braces -Werror"

if [ -z "$CFLAGS" ]; then
    ANALYZER_FLAGS="$AUTO_CFLAGS"
else 
    ANALYZER_FLAGS="$CFLAGS"
fi
ANALYZER_FLAGS="$ANALYZER_FLAGS $CFLAGS -Wno-analyzer-malloc-leak -Wno-analyzer-deref-before-check"

: ${SERIAL:=0}
: ${CFLAGS:="$AUTO_CFLAGS $AUTO_CFLAGS_DBG"}
CFLAGS="$CFLAGS $EX_CFLAGS"

#CFG_HASH=$(sum <<< "$CC $BUILD_CC $AR $CLAGS")
#CLEAN=1
#if [ -f "_settings.chksum" ]; then
#       OLD=$(cat _settings.chksum)
#       if [[ "$CFG_HASH" == "$OLD" ]]; then
#               CLEAN=0
#       fi
#fi
#echo $CFG_HASH > _settings.chksum
#
#if [ -f "build" ]; then
#       if (( $CLEAN == 1 )); then
#               echo "# completely re-building"
#               rm -r build
#       fi
#fi

FILES="ir/*.c targets/*.c ir/opt/*.c ir/transform/*.c"

# shellcheck disable=SC2086

function prepare() {
  if [[ $SERIAL == 1 ]]; then 
    SERIAL_ARGS="-DSERIAL=1"
  else 
    SERIAL_ARGS="-lpthread"
  fi 
  $BUILD_CC build.c $(build_c/slowdb/build.sh) $SERIAL_ARGS $CFLAGS -DVERBOSE=1 -DCC="\"$CC\"" -DCC_ARGS="\"$CFLAGS\"" -DLD_ARGS="\"$EX_LDFLAGS $SERIAL_ARGS\"" -DAR="\"$AR\"" -o build.exe
  echo "# build.exe compiled"
  echo "# gen cdef files"
  ./build.exe gen
}

if [[ $1 == "analyze" ]]; then
  echo "# analyzing..."
  prepare
  echo "# starting analyzer"
  "$CLANG" --analyze -Xclang -analyzer-werror $ANALYZER_FLAGS $FILES
elif [[ $1 == "ganalyze" ]]; then
  echo "analyzing (gcc) ..."
  prepare
  echo "# starting analyzer (gcc)"
  # gcc assumes that malloc() might return null, and therefore emits tons of null errors. the important ones get catched by clang analyzer anyways.
  "$GCC" -fanalyzer -fsanitize-trap=undefined -fsyntax-only $ANALYZER_FLAGS $FILES "-Wno-return-type" "-Wno-analyzer-possible-null-argument" "-Wno-analyzer-possible-null-dereference"
elif [[ $1 == "info" ]]; then
  echo CC:
  "$CC" --version
  echo BUILD_CC:
  "$CC" --version
  echo cflags: $CFLAGS
elif [[ $1 == "build" ]]; then
  echo "# compile Debug"
  prepare
  echo "# deps"
  ./build.exe deps
  echo "# lib.a"
  ./build.exe lib.a
elif [[ $1 == "exe" ]]; then
  echo "# compile executable Debug"
  prepare
  echo "# deps"
  ./build.exe deps
  echo "# lib.a"
  ./build.exe lib.a
  echo "# vxcc.exe"
  ./build.exe vxcc.exe
elif [[ $1 == "lsp" ]]; then 
  prepare
elif [[ $1 == "libfiles" ]]; then
  echo "build/lib.a"
elif [[ $1 == "clean" ]]; then
  rm -r build/
  rm build.slowdb
else
  echo "invalid arguments; usage: ./build.sh [ganalyze|analyze|build|exe|lsp|libfiles|clean]"
  echo "you can set CC, CFLAGS, BUILD_CC, python, EX_CFLAGS, AR, EX_LDFLAGS"
  echo "if you set EMPATH, these flags get added automatically (you can overwrite them manually): CC=\$EMPATH/emcc BUILD_CC=clang EX_CFLAGS="-O3" AR=\$EMPATH/emar"
fi

# shellcheck enable=SC2086
