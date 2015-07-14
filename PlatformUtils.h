#pragma once

#include "Cxx11Utils.h"
#include <queue>
#include <string>
#include <algorithm>
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <unordered_map>
#include <deque>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifndef XSTR_BASE
#define XSTR_CONCAT(s1,s2) XSTR_CONCAT_BASE(s1,s2)
#define XSTR_CONCAT_BASE(s1,s2) s1##s2
#define XSTR(s) XSTR_BASE(s)
#define XSTR_BASE(s) #s
#endif //XSTR_BASE
#define TIMER_TIC(x) int64 XSTR_CONCAT(__nCPUTimerTick_,x) = cv::getTickCount()
#define TIMER_TOC(x) int64 XSTR_CONCAT(__nCPUTimerVal_,x) = cv::getTickCount()-XSTR_CONCAT(__nCPUTimerTick_,x)
#define TIMER_ELAPSED_MS(x) (double(XSTR_CONCAT(__nCPUTimerVal_,x))/(cv::getTickFrequency()/1000))
#if defined(_MSC_VER)
#define PLATFORM_USES_WIN32API defined(_MSC_VER)
#define NOMINMAX
#include <windows.h>
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifndef DBG_NEW
#define DBG_NEW new (_NORMAL_BLOCK , __FILE__ , __LINE__)
#define new DBG_NEW
#endif //!DBG_NEW
#endif //_DEBUG
#endif //WIN32
#if PLATFORM_USES_WIN32API
#include <stdint.h>
#define __func__ __FUNCTION__
#else //!PLATFORM_USES_WIN32API
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif //!PLATFORM_USES_WIN32API

namespace PlatformUtils {

    void GetFilesFromDir(const std::string& sDirPath, std::vector<std::string>& vsFilePaths);
    void GetSubDirsFromDir(const std::string& sDirPath, std::vector<std::string>& vsSubDirPaths);
    bool CreateDirIfNotExist(const std::string& sDirPath);

    inline bool compare_lowercase(const std::string& i, const std::string& j) {
        std::string i_lower(i), j_lower(j);
        std::transform(i_lower.begin(),i_lower.end(),i_lower.begin(),tolower);
        std::transform(j_lower.begin(),j_lower.end(),j_lower.begin(),tolower);
        return i_lower<j_lower;
    }

    template<typename T> inline int decimal_integer_digit_count(T number) {
        int digits = number<0?1:0;
        while(std::abs(number)>=1) {
            number /= 10;
            digits++;
        }
        return digits;
    }

#if PLATFORM_USES_WIN32API
    void SetConsoleWindowSize(int x, int y, int buffer_lines=-1);
#endif //PLATFORM_USES_WIN32API

}; //namespace PlatformUtils
