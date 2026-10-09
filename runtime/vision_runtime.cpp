#include <opencv2/opencv.hpp>
#include <iostream>

extern "C" {

long long edge_load_image(const char* filename) {
    cv::Mat* img = new cv::Mat();
    *img = cv::imread(filename);
    if (img->empty()) {
        std::cerr << "EdgeFlow Runtime Error: Could not load image " << filename << std::endl;
    }
    return reinterpret_cast<long long>(img);
}

long long edge_resize(long long img_ptr, long long width, long long height) {
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    cv::Mat* resized = new cv::Mat();
    if (img && !img->empty()) {
        cv::resize(*img, *resized, cv::Size(width, height));
    }
    return reinterpret_cast<long long>(resized);
}

long long edge_grayscale(long long img_ptr) {
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    if (img && !img->empty()) {
        if (img->channels() == 3) {
            cv::cvtColor(*img, *img, cv::COLOR_BGR2GRAY);
        }
    }
    return img_ptr;
}

long long edge_blur(long long img_ptr, long long ksize) {
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    if (img && !img->empty()) {
        if (ksize % 2 == 0) ksize++; // must be odd
        cv::GaussianBlur(*img, *img, cv::Size(ksize, ksize), 0);
    }
    return img_ptr;
}

long long edge_show(long long img_ptr) {
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    if (img && !img->empty()) {
        cv::imshow("EdgeFlow Vision", *img);
        cv::waitKey(0);
    }
    return 0;
}

long long edge_save(long long img_ptr, const char* filename) {
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    if (img && !img->empty()) {
        cv::imwrite(filename, *img);
    }
    return 0;
}

long long edge_open_camera(long long device_id) {
    cv::VideoCapture* cap = new cv::VideoCapture(device_id);
    if (!cap->isOpened()) {
        std::cerr << "EdgeFlow Runtime Error: Could not open camera " << device_id << std::endl;
    }
    return reinterpret_cast<long long>(cap);
}

long long edge_camera_read(long long cam_ptr) {
    cv::VideoCapture* cap = reinterpret_cast<cv::VideoCapture*>(cam_ptr);
    cv::Mat* img = new cv::Mat();
    if (cap && cap->isOpened()) {
        *cap >> *img;
    }
    return reinterpret_cast<long long>(img);
}

long long edge_load_model(const char* filename) {
    cv::dnn::Net* net = new cv::dnn::Net();
    try {
        *net = cv::dnn::readNetFromONNX(filename);
    } catch (...) {
        std::cerr << "EdgeFlow Runtime Error: Could not load ONNX model " << filename << std::endl;
    }
    return reinterpret_cast<long long>(net);
}

long long edge_detect(long long model_ptr, long long img_ptr) {
    cv::dnn::Net* net = reinterpret_cast<cv::dnn::Net*>(model_ptr);
    cv::Mat* img = reinterpret_cast<cv::Mat*>(img_ptr);
    if (net && !net->empty() && img && !img->empty()) {
        cv::Mat blob = cv::dnn::blobFromImage(*img, 1/255.0, cv::Size(640, 640), cv::Scalar(0,0,0), true, false);
        net->setInput(blob);
        try {
            cv::Mat out = net->forward();
        } catch(...) {}
    }
    return 0;
}

}
