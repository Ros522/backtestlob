cmake_minimum_required (VERSION 3.8)


add_subdirectory("pybind11")
pybind11_add_module(backtest SHARED backtest.cpp)  

