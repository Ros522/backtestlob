# CMakeList.txt : backtest の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#
cmake_minimum_required (VERSION 3.8)


# TODO: テストを追加し、必要な場合は、ターゲットをインストールします。

add_subdirectory("pybind11")
pybind11_add_module(backtest SHARED backtest.cpp)  

