#!/bin/sh

cd ../
#[ -d build-ut ] && rm -fr build-ut
mkdir -p build-ut/html
mkdir -p build-ut/report
[ -d build ] && rm -fr build
mkdir -p build

cd build/

# Sanitize for asan.log
######################
export DISPLAY=:0.0
export XDG_CURRENT_DESKTOP=Deepin
export QT_IM_MODULE=fcitx

cd ../autotests
sh cmake-lcov-test.sh CMAKE_SAFETYTEST_ARG_ON
cd -
cd ..
mv ./sanitizer/asan.log ./build-ut/asan_dde-kwin.log
######################
#cd -

# UT for index.html and ut-report.txt
cd autotests
sh cmake-lcov-test.sh CMAKE_SAFETYTEST_ARG_OFF
pwd
cd -
mv build-ut/bin/report/index.html build-ut/html/cov_dde-kwin.html
mv build-ut/ut-report.txt build-ut/report/report_dde-kwin.xml
