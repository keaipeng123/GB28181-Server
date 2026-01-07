#创建了一个名为 EXE_NAME的 CMake 变量，并将其值设置为 "SipSupService"
SET(EXE_NAME SipSupService)
#打印当前使用的操作系统
MESSAGE(STATUS "operation system is ${CMAKE_SYSTEM}")
#在编译选项中开启所有警告和添加c++11支持
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
#在编译时使用-g选项添加调试信息，编译后的程序可以使用gdb调试
ADD_DEFINITIONS(-g)
#指定需要编译的源文件路径，并指定相应的变量名
AUX_SOURCE_DIRECTORY(../src SRC)
#添加头文件路径
INCLUDE_DIRECTORIES(../include)
#将指定的源文件代码生成相应的可执行文件
ADD_EXECUTABLE(${EXE_NAME} ${SRC})