#pragma once

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <cmath>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include <random>
#include <regex>
#include <iterator>

// cv
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

// v8
#include <libplatform/libplatform.h>
#include <v8.h>