#!/bin/bash
export DISPLAY=:0.0
utdir=build-ut
rm -r $utdir
rm -r ../$utdir
mkdir ../$utdir
cd ../$utdir

cmake -DCMAKE_SAFETYTEST_ARG="CMAKE_SAFETYTEST_ARG_ON" ..
make -j4

make test


workdir=$(cd ../$(dirname $0)/$utdir; pwd)

mkdir -p report

lcov -d $workdir -c -o ./report/coverage.info

lcov --remove ./coverage/coverage.info '*/tests/*' '*/autotests/*' '*/*_autogen/*' -o ./coverage/coverage.info
genhtml -o ./report ./report/coverage.info

merge all asan.log* into asan.log
find . -name "asan.log*" -exec cat '{}' \; > asan.log

exit 0

