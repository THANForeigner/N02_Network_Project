#include "WebcamRecorder.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <ctime>
#include <mutex>

namespace fs = std::filesystem;

static std::thread recordThread;
static std::atomic<bool> recording(false);
static std::mutex recordMutex;

static std::string getCurrentTimestamp(const char* format) {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), format, std::localtime(&now));
    return std::string(buf);
}

static fs::path getRecordFolderPath() {
    return fs::current_path().parent_path() / "record";
}

static void ensureRecordFolder() {
    fs::path folder = getRecordFolderPath();
    try {
        if (!fs::exists(folder)) fs::create_directories(folder);
    }
    catch (const std::exception& e) {

    }
}

bool startRecording() {
    std::lock_guard<std::mutex> lock(recordMutex);
    if (recording) return false;
    ensureRecordFolder();
    std::string filename = (getRecordFolderPath() / (getCurrentTimestamp("%Y%m%d_%H%M%S") + ".avi")).string();

    recording = true;

    recordThread = std::thread([filename]() {
        cv::VideoCapture cap(0);
        if (!cap.isOpened()) {
            recording = false;
            return;
        }

        int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

        cv::VideoWriter writer(
            filename,
            cv::VideoWriter::fourcc('M','J','P','G'),
            30,
            cv::Size(width, height)
        );

        if (!writer.isOpened()) {
            recording = false;
            return;
        }

        while (recording) {
            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) break;

            writer.write(frame);

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        cap.release();
        writer.release();
    });

    return true;
}

void stopRecording() {
    std::lock_guard<std::mutex> lock(recordMutex);
    if (recording) {
        recording = false;
        if (recordThread.joinable())
            recordThread.join();
    }
}

bool isRecording() {
    return recording;
}

bool captureSnapshot() {
    ensureRecordFolder();
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return false;

    cv::Mat frame;
    cap >> frame;
    cap.release();

    if (frame.empty()) return false;

    std::string filename = (getRecordFolderPath() / (getCurrentTimestamp("%Y%m%d_%H%M%S") + ".jpg")).string();

    return cv::imwrite(filename, frame);
}