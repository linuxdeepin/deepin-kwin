#!/bin/bash

funcAT_UT()
{
	rm -r $1
	rm -r ../$1
	mkdir ../$1
	cd ../$1
	cmake -DCMAKE_SAFETYTEST_ARG="$2" ..
	make -j4
	make test
	if [ $3 = "UT" ];then
		echo "------------UT-----------"
		workdir=$(cd ../$(dirname $0)/$1; pwd)
		mkdir -p report
		lcov -d $workdir -c -o ./report/coverage.info
		lcov --remove ./coverage/coverage.info '*/tests/*' '*/autotests/*' '*/*_autogen/*' -o ./coverage/coverage.info
		genhtml -o ./report ./report/coverage.info
	else
		echo "------------AT-----------"
		find . -name "asan.log.*" -exec cat '{}' \; > asan.log
	fi
}

CMAKE_ASAN_ARG=$1
if [ ! -n "$CMAKE_ASAN_ARG" ];then
	echo "---can not input null arg, please input 'CMAKE_SAFETYTEST_ARG_ON' or 'CMAKE_SAFETYTEST_ARG_OFF'---."
	exit 0
elif [ $CMAKE_ASAN_ARG != "CMAKE_SAFETYTEST_ARG_ON" ] && [ $CMAKE_ASAN_ARG != "CMAKE_SAFETYTEST_ARG_OFF" ];then
	echo "---please input 'CMAKE_SAFETYTEST_ARG_ON' or 'CMAKE_SAFETYTEST_ARG_OFF'---."
	exit 0
fi

export DISPLAY=:0.0
utdir=""
tType=""
if [ $CMAKE_ASAN_ARG = "CMAKE_SAFETYTEST_ARG_ON" ];then
	utdir="sanitizer"
	tType="AT"
elif [ $CMAKE_ASAN_ARG = "CMAKE_SAFETYTEST_ARG_OFF" ];then
	utdir="build-ut"
	tType="UT"
fi
echo "---CMAKE_ASAN_ARG=$CMAKE_ASAN_ARG;utdir=$utdir;tType=$tType---"
funcAT_UT $utdir $CMAKE_ASAN_ARG $tType

