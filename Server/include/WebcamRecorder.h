#ifndef WEBCAMRECORDER_H
#define WEBCAMRECORDER_h

#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <ctime>
#include <mutex>
namespace fs = std::filesystem;

bool startRecording(std::string &path);
void stopRecording();
bool isRecording();
bool captureSnapshot();

#endif // WEBCAMRECORDER_H