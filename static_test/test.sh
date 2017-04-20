#!/bin/sh

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
if [ "$2" ]; then
    if [ "$2" != "with-c-driver" ]; then
        echo "second arg must be 'with-c-driver'" >&2
        exit 1
    fi
    WITH_C_DRIVER=1
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
else
    GENERATOR="Visual Studio 14 Win64"
    ADDL_OPTS='-DBOOST_ROOT="c:/local/boost_1_60_0"'
fi

PREFIX=x
if [ "$UNIX" ]; then
    PREFIX=/opt/static_test
else
    PREFIX=c:/install
fi

rm -rf bson mongo a.out.dSYM static_bson.o dynamic_bson.o static_mongo.o dynamic_mongo.o
rm -rf $PREFIX/libmongocxx
rm -rf */build

exit

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
            # TODO
            false
        elif [ "$TARGET" == "run-examples" ]; then
            # TODO
            false
        else
            # TODO
            false
        fi
        MSBuild.exe /m "$TARGET"
    fi
}

if [ "$WITH_C_DRIVER" ]; then
    rm -rf $PREFIX/libbson $PREFIX/libmongoc $PREFIX/libmongocxx

    cd mongo-c-driver/src/libbson
    git clean -xfd
    cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE:STRING=Debug -DENABLE_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX=$PREFIX/libbson .
    #./autogen.sh --prefix=$PREFIX/libbson --enable-debug --disable-tests --enable-static
    _make 2>&1 | tee build.log # CMAKE
    #make V=1 2>&1 | tee build.log # AUTOTOOLS
    _make install

    cd ../..
    git clean -xfd
    cmake -G "$GENERATOR" -DCMAKE_PREFIX_PATH=$PREFIX/libbson -DCMAKE_BUILD_TYPE:STRING=Debug -DENABLE_TESTS:BOOL=OFF -DCMAKE_INSTALL_PREFIX=$PREFIX/libmongoc .
    _make 2>&1 | tee build.log # CMAKE
    _make install
    cd ..
fi

cd mongo-cxx-driver
rm -rf build
git checkout build
cd build

#PKG_CONFIG_PATH=$PREFIX/libbson/lib/pkgconfig cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX=$PREFIX/libmongocxx ..
CMAKE_PREFIX_PATH="$PREFIX/libbson/lib/cmake/libbson-1.0:$PREFIX/libbson/lib/cmake/libbson-static-1.0:$PREFIX/libmongoc/lib/cmake/libmongoc-1.0:$PREFIX/libmongoc/lib/cmake/libmongoc-static-1.0" cmake -G "$GENERATOR" -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX=$PREFIX/libmongocxx -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS ..
_make | tee build.log
_make install
DYLD_LIBRARY_PATH=$PREFIX/libmongoc/lib:$PREFIX/libbson/lib _make test
_make examples
DYLD_LIBRARY_PATH=$PREFIX/libmongoc/lib:$PREFIX/libbson/lib _make run-examples
popd

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

if [ "$STATIC" ]; then
    mkdir -p find_package_bsoncxx_static/build
    cd find_package_bsoncxx_static/build
    CMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-static-3.1.1-pre:$PREFIX/libbson/lib/cmake/libbson-static-1.0" cmake -G "$GENERATOR" ..
    _make
    ./hello_bsoncxx
    cd ../..
else
    mkdir -p find_package_bsoncxx/build
    cd find_package_bsoncxx/build
    CMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-3.1.1-pre:$PREFIX/libbson/lib/cmake/libbson-1.0" cmake -G "$GENERATOR" ..
    _make
    DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libbson/lib ./hello_bsoncxx
    cd ../..
fi

if [ "$STATIC" ]; then
    mkdir -p find_package_mongocxx_static/build
    cd find_package_mongocxx_static/build
    CMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-static-3.1.1-pre:$PREFIX/libbson/lib/cmake/libbson-static-1.0:$PREFIX/libmongoc/lib/cmake/libmongoc-static-1.0:$PREFIX/libmongocxx/lib/cmake/libmongocxx-static-3.1.1-pre" cmake -G "$GENERATOR" ..
    _make
    ./hello_mongocxx
    cd ../..
else 
    mkdir -p find_package_mongocxx/build
    cd find_package_mongocxx/build
    CMAKE_PREFIX_PATH="$PREFIX/libmongocxx/lib/cmake/libbsoncxx-3.1.1-pre:$PREFIX/libmongocxx/lib/cmake/libmongocxx-3.1.1-pre" cmake -G "$GENERATOR" ..
    _make
    DYLD_LIBRARY_PATH=$PREFIX/libmongocxx/lib:$PREFIX/libmongoc/lib:$PREFIX/libbson/lib ./hello_mongocxx
    cd ../..
fi
