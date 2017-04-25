#!/bin/bash

set -x
set -e

if [ -z "$1" ]; then
   echo "say static or dynamic" >&2
   exit 1
fi

if [ "$1" != static -a "$1" != dynamic ]; then
   echo "say static or dynamic" >&2
   exit 1
fi

WITH_C_DRIVER=
WITH_CXX_DRIVER=
if [ "$2" ]; then
    if [ "$2" != "with-drivers" -a "$2" != "with-cxx-driver" ]; then
        echo "second arg must be 'with-drivers' or 'with-cxx-driver'" >&2
        exit 1
    fi
    WITH_CXX_DRIVER=1
    if [ "$2" == "with-drivers" ]; then
        WITH_C_DRIVER=1
    fi
fi

STATIC=
DYNAMIC=
if [ "$1" == static ]; then
    STATIC=1
    BUILD_SHARED_LIBS=OFF
else
    DYNAMIC=1
    BUILD_SHARED_LIBS=ON
fi

if [ "$OS" == "Windows_NT" ]; then
    UNIX=
else
    UNIX=1
fi

if [ "$UNIX" ]; then
    GENERATOR="Unix Makefiles"
    ADDL_OPTS=
    EXEC_PREFIX=.
else
    GENERATOR="Visual Studio 14 Win64"
    ADDL_OPTS="-DBOOST_ROOT=c:/local/boost_1_60_0"
    PATH=/cygdrive/c/cmake/bin:$PATH
    PATH=/cygdrive/c/Program\ Files\ \(x86\)/MSBuild/14.0/Bin:$PATH
    EXEC_PREFIX=Debug
fi

PREFIX=x
if [ "$UNIX" ]; then
    PREFIX=/opt/static_test
else
    PREFIX=c:/install
fi

MONGOCXX_VER=3.1.1-pre

rm -rf bson mongo a.out.dSYM static_bson.o dynamic_bson.o static_mongo.o dynamic_mongo.o
rm -rf */build

pushd ../..

_make() {
    TARGET="$1"

    if [ "$UNIX" ]; then
        make -j32 $TARGET VERBOSE=1 # V=1 for autotools
    else 
        if [ "$TARGET" == "" ]; then
            TARGET=ALL_BUILD.vcxproj
        elif [ "$TARGET" == "install" ]; then
            TARGET=INSTALL.vcxproj
        elif [ "$TARGET" == "test" ]; then
            TARGET=RUN_TESTS.vcxproj
        elif [ "$TARGET" == "examples" ]; then
            TARGET=examples/examples.vcxproj
        elif [ "$TARGET" == "run-examples" ]; then
            TARGET=examples/run-examples.vcxproj
        else
            # TODO
            false
        fi
        MSBuild.exe /m $TARGET
    fi
}

if [ "$WITH_C_DRIVER" ]; then
    rm -rf $PREFIX/libbson $PREFIX/libmongoc $PREFIX/libmongocxx

    cd mongo-c-driver/src/libbson
    git clean -xfd
    cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE:STRING=Debug -DENABLE_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX=$PREFIX/libbson .
    #./autogen.sh --prefix=$PREFIX/libbson --enable-debug --disable-tests --enable-static
    _make
    _make install

    cd ../..
    git clean -xfd
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH=$PREFIX/libbson -DCMAKE_BUILD_TYPE:STRING=Debug -DENABLE_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX=$PREFIX/libmongoc .
    _make
    _make install
    cd ..
fi

cd mongo-cxx-driver

if [ "$WITH_CXX_DRIVER" ]; then
    rm -rf $PREFIX/libmongocxx
    rm -rf build
    git checkout build
    cd build
    cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX=$PREFIX/libmongocxx -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -DCMAKE_PREFIX_PATH="$PREFIX/libbson/lib/cmake/libbson-1.0;$PREFIX/libbson/lib/cmake/libbson-static-1.0;$PREFIX/libmongoc/lib/cmake/libmongoc-1.0;$PREFIX/libmongoc/lib/cmake/libmongoc-static-1.0" ${ADDL_OPTS} ..
    _make
    _make install
    cd ..
fi

cd build
(
    export DYLD_LIBRARY_PATH=$PREFIX/libmongoc/lib:$PREFIX/libbson/lib
    export PATH=$(pwd)/src/bsoncxx/Debug:$(pwd)/src/mongocxx/Debug:/cygdrive/c/install/libmongoc/bin:/cygdrive/c/install/libbson/bin:$PATH
    _make test
)
_make examples
(
    export DYLD_LIBRARY_PATH=$PREFIX/libmongoc/lib:$PREFIX/libbson/lib
    export PATH=$(pwd)/src/bsoncxx/Debug:$(pwd)/src/mongocxx/Debug:/cygdrive/c/install/libmongoc/bin:/cygdrive/c/install/libbson/bin:$PATH
    _make run-examples
)
popd

if [ "$STATIC" ]; then
    mkdir -p find_package_bsoncxx_static/build
    cd find_package_bsoncxx_static/build
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-static-${MONGOCXX_VER};$PREFIX/libbson/lib/cmake/libbson-static-1.0" ..
    _make
    $EXEC_PREFIX/hello_bsoncxx
    cd ../..
else
    mkdir -p find_package_bsoncxx/build
    cd find_package_bsoncxx/build
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-${MONGOCXX_VER};$PREFIX/libbson/lib/cmake/libbson-1.0" ..
    _make
    (
        export DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libbson/lib
        export PATH=/cygdrive/c/install/libmongocxx/bin:/cygdrive/c/install/libmongoc/bin:/cygdrive/c/install/libbson/bin:$PATH
        $EXEC_PREFIX/hello_bsoncxx
    )
    cd ../..
fi

if [ "$STATIC" ]; then
    mkdir -p find_package_mongocxx_static/build
    cd find_package_mongocxx_static/build
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-static-${MONGOCXX_VER};$PREFIX/libbson/lib/cmake/libbson-static-1.0;$PREFIX/libmongoc/lib/cmake/libmongoc-static-1.0;$PREFIX/libmongocxx/lib/cmake/libmongocxx-static-${MONGOCXX_VER}" ..
    _make
    $EXEC_PREFIX/hello_mongocxx
    cd ../..
else 
    mkdir -p find_package_mongocxx/build
    cd find_package_mongocxx/build
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-${MONGOCXX_VER};$PREFIX/libmongocxx/lib/cmake/libmongocxx-${MONGOCXX_VER}" ..
    _make
    (
        export DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libmongoc/lib:$PREFIX/libbson/lib
        export PATH=/cygdrive/c/install/libmongocxx/bin:/cygdrive/c/install/libmongoc/bin:/cygdrive/c/install/libbson/bin:$PATH
        $EXEC_PREFIX/hello_mongocxx
    )
    cd ../..
fi

if [ "$UNIX" ]; then
    if [ "$STATIC" ]; then
        g++ -std=c++11 -g -Wall -c static_bson.cpp $(PKG_CONFIG_PATH=$PREFIX/libbson/lib/pkgconfig:$PREFIX/libmongocxx/lib/pkgconfig pkg-config --cflags libbsoncxx-static)
        g++ -o bson -std=c++11 -g -Wall static_bson.o $(PKG_CONFIG_PATH=$PREFIX/libbson/lib/pkgconfig:$PREFIX/libmongocxx/lib/pkgconfig pkg-config --libs libbsoncxx-static)
        ./bson
    else 
        g++ -std=c++11 -g -Wall -c dynamic_bson.cpp $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig pkg-config --cflags libbsoncxx)
        g++ -o bson -std=c++11 -g -Wall dynamic_bson.o $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig pkg-config --libs libbsoncxx)
        DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libbson/lib:$PREFIX/libmongoc/lib ./bson
    fi

    if [ "$STATIC" ]; then
        g++ -std=c++11 -g -Wall -c static_mongo.cpp $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig:$PREFIX/libbson/lib/pkgconfig:$PREFIX/libmongoc/lib/pkgconfig pkg-config --cflags libmongocxx-static)
        g++ -o mongo -std=c++11 -g -Wall static_mongo.o $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig:$PREFIX/libbson/lib/pkgconfig:$PREFIX/libmongoc/lib/pkgconfig pkg-config --libs libmongocxx-static)
        ./mongo
    else
        g++ -std=c++11 -g -Wall -c dynamic_mongo.cpp $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig pkg-config --cflags libmongocxx)
        g++ -o mongo -std=c++11 -g -Wall dynamic_mongo.o $(PKG_CONFIG_PATH=$PREFIX/libmongocxx/lib/pkgconfig pkg-config --libs libmongocxx)
        DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libbson/lib:$PREFIX/libmongoc/lib ./mongo
    fi
fi
