#pragma once

#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <tuple>
#include <regex>

#define mrWithOpenCV

#ifdef mrWithOpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#endif // mrWithOpenCV
