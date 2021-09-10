#!/bin/bash

funcAT_UT()
{
	rm -r $1
	rm -r ../$1
	mkdir ../$1
	cd ../$1
	cmake -DCMAKE_SAFETYTEST_ARG="$2" ..
	make -j4
	cd bin/
	testList=`ls -F | grep test | grep -v "cursorhotspottest\|orientationtest\|libinputtest\|waylandclienttest\|screenedgeshowtest\|normalhintsbasesizetest"`
	if [ $3 = "UT" ];then
		echo "------------UT-----------"
		for case in $testList
		do
			./$case >> ../ut-report-detail.txt
			echo "$case`grep 'Totals:' ../ut-report-detail.txt | tail -n 1`" >> ../ut-report.txt 2> /dev/null
		done
		echo "==============================" >> ../ut-report.txt
		echo "Totals: `cat ../ut-report.txt | awk '{sum += $2}END{print sum}'` passed, `cat ../ut-report.txt | awk '{sum += $4}END{print sum}'` failed, `cat ../ut-report.txt | awk '{sum += $6}END{print sum}'` skipped, `cat ../ut-report.txt | awk '{sum += $8}END{print sum}'` blacklisted" >> ../ut-report.txt
		echo "==============================" >> ../ut-report.txt
		workdir=$(cd ../; pwd)
		mkdir -p report
		lcov -d $workdir -c -o ./report/coverage.info
		lcov --remove ./coverage/coverage.info '*/tests/*' '*/autotests/*' '*/*_autogen/*' -o ./coverage/coverage.info
		genhtml -o ./report ./report/coverage.info
	else
		echo "------------AT-----------";pwd
		for case in $testList
		do
			./$case 
		done
		find . -name "asan.log.*" -exec cat '{}' \; > ../asan.log
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

