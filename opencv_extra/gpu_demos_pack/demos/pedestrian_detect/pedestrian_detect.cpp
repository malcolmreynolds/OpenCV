#include <utility_lib/utility_lib.h>
#include "opencv2/contrib/contrib.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/gpu/gpu.hpp"

using namespace std;
using namespace cv;
using namespace cv::gpu;

class App : public BaseApp
{
public:
    App();

    virtual void run(int argc, char **argv);
    virtual void parseCmdArgs(int argc, char **argv);
    virtual bool processKey(int key);
    virtual void printHelp();

    bool write_video;
    string dst_video;
    double dst_video_fps;

    bool make_gray;

    bool resize_src;

    double scale;
    int nlevels;
    int gr_threshold;

    double hit_threshold;
    bool hit_threshold_auto;

    int win_width;
    int win_stride_width, win_stride_height;

    bool gamma_corr;

    bool use_gpu;
};


App::App()
{
    write_video = false;
    dst_video_fps = 24.;

    make_gray = false;

    resize_src = false;

    scale = 1.05;
    nlevels = 13;
    gr_threshold = 8;
    hit_threshold = 1.4;
    hit_threshold_auto = true;

    win_width = 48;
    win_stride_width = 8;
    win_stride_height = 8;

    gamma_corr = true;

    use_gpu = true;
}


void App::run(int argc, char **argv)
{
    parseCmdArgs(argc, argv);
    if (help_showed) 
        return;

    if (sources.size() != 1)
    {
        cout << "Using default frames source...\n";
        sources.resize(1);
        sources[0] = new VideoSource("data/pedestrian_detect/mitsubishi.avi");
    }

    cout << "\nControls:\n"
         << "\tESC - exit\n"
         << "\tspace - change mode GPU <-> CPU\n"
         << "\tg - convert image to gray or not\n"
         << "\t1/q - increase/decrease HOG scale\n"
         << "\t2/w - increase/decrease levels count\n"
         << "\t3/e - increase/decrease HOG group threshold\n"
         << "\t4/r - increase/decrease hit threshold\n"
         << endl;

    if (hit_threshold_auto)
        hit_threshold = win_width == 48 ? 1.4 : 0.;

    if (win_width != 64 && win_width != 48)
    {
        cout << "Using default windows width (64)...\n";
        win_width = 64;
    }

    cout << "Scale: " << scale << endl;
    if (resize_src)
        cout << "Resized source: (" << frame_width << ", " << frame_height << ")\n";
    cout << "Group threshold: " << gr_threshold << endl;
    cout << "Levels number: " << nlevels << endl;
    cout << "Win width: " << win_width << endl;
    cout << "Win stride: (" << win_stride_width << ", " << win_stride_height << ")\n";
    cout << "Hit threshold: " << hit_threshold << endl;
    cout << "Gamma correction: " << gamma_corr << endl;
    cout << endl;

    Size win_size(win_width, win_width * 2); //(64, 128) or (48, 96)
    Size win_stride(win_stride_width, win_stride_height);

    // Create HOG descriptors and detectors here
    vector<float> detector;
    if (win_size == Size(64, 128)) 
        detector = cv::gpu::HOGDescriptor::getPeopleDetector64x128();
    else
        detector = cv::gpu::HOGDescriptor::getPeopleDetector48x96();

    cv::gpu::HOGDescriptor gpu_hog(win_size, Size(16, 16), Size(8, 8), Size(8, 8), 9, 
                                   cv::gpu::HOGDescriptor::DEFAULT_WIN_SIGMA, 0.2, gamma_corr, 
                                   cv::gpu::HOGDescriptor::DEFAULT_NLEVELS);
    cv::HOGDescriptor cpu_hog(win_size, Size(16, 16), Size(8, 8), Size(8, 8), 9, 1, -1, 
                              cv::HOGDescriptor::L2Hys, 0.2, gamma_corr, cv::HOGDescriptor::DEFAULT_NLEVELS);
    gpu_hog.setSVMDetector(detector);
    cpu_hog.setSVMDetector(detector);

    Mat frame;
    Mat img_aux, img, img_to_show;
    gpu::GpuMat gpu_img;

    double total_fps = 0;

    // Iterate over all frames
    while (!exited)
    {
        int64 start = getTickCount();

        sources[0]->next(frame);

        // Change format of the image
        if (make_gray) cvtColor(frame, img_aux, CV_BGR2GRAY);
        else if (use_gpu) cvtColor(frame, img_aux, CV_BGR2BGRA);
        else frame.copyTo(img_aux);

        // Resize image
        if (resize_src) 
            resize(img_aux, img, Size(frame_width, frame_height));
        else img = img_aux;
        img_to_show = img;

        gpu_hog.nlevels = nlevels;
        cpu_hog.nlevels = nlevels;

        vector<Rect> found;

        // Perform HOG classification
        int64 proc_start = getTickCount();
        if (use_gpu)
        {
            gpu_img = img;
            gpu_hog.detectMultiScale(gpu_img, found, hit_threshold, win_stride, 
                                     Size(0, 0), scale, gr_threshold);
        }
        else cpu_hog.detectMultiScale(img, found, hit_threshold, win_stride, 
                                      Size(0, 0), scale, gr_threshold);
        double proc_fps = getTickFrequency() / (getTickCount() - proc_start);

        // Draw positive classified windows
        for (size_t i = 0; i < found.size(); i++)
        {
            Rect r = found[i];
            rectangle(img_to_show, r.tl(), r.br(), CV_RGB(0, 255, 0), 3);
        }

        if (use_gpu)
            putText(img_to_show, "Mode: GPU", Point(5, 25), FONT_HERSHEY_SIMPLEX, 1., Scalar(255, 100, 0), 2);
        else
            putText(img_to_show, "Mode: CPU", Point(5, 25), FONT_HERSHEY_SIMPLEX, 1., Scalar(255, 100, 0), 2);
        stringstream txt;
        txt << "FPS (HOG only): " << proc_fps;
        putText(img_to_show, txt.str(), Point(5, 65), FONT_HERSHEY_SIMPLEX, 1., Scalar(255, 100, 0), 2);
        txt.str("");
        txt << "FPS (total): " << total_fps;
        putText(img_to_show, txt.str(), Point(5, 105), FONT_HERSHEY_SIMPLEX, 1., Scalar(255, 100, 0), 2);
        imshow("pedestrian_detect_demo", img_to_show);

        processKey(waitKey(3));

        total_fps = getTickFrequency() / (getTickCount() - start);
    }
}


void App::parseCmdArgs(int argc, char **argv)
{
    for (int i = 1; i < argc && !help_showed; ++i)
    {
        if (parseBaseCmdArgs(i, argc, argv))
            continue;
        string key = argv[i];
        if (key == "--make-gray") make_gray = (string(argv[++i]) == "true");
        else if (key == "--resize-src") resize_src = (string(argv[++i]) == "true");
        else if (key == "--hit-threshold") 
        { 
            hit_threshold = atof(argv[++i]); 
            hit_threshold_auto = false; 
        }
        else if (key == "--scale") scale = atof(argv[++i]);
        else if (key == "--nlevels") nlevels = atoi(argv[++i]);
        else if (key == "--win-width") win_width = atoi(argv[++i]);
        else if (key == "--win-stride-width") win_stride_width = atoi(argv[++i]);
        else if (key == "--win-stride-height") win_stride_height = atoi(argv[++i]);
        else if (key == "--gr-threshold") gr_threshold = atoi(argv[++i]);
        else if (key == "--gamma-correct") gamma_corr = (string(argv[++i]) == "true");
        else if (key == "--write-video") write_video = (string(argv[++i]) == "true");
        else if (key == "--dst-video") dst_video = string(argv[++i]);
        else if (key == "--dst-video-fps") dst_video_fps= atof(argv[++i]);
        else throwBadArgError(key.c_str());
    }
}


bool App::processKey(int key)
{
    if (BaseApp::processKey(key))
        return true;
    switch (key)
    {
    case 32:
        use_gpu = !use_gpu;
        cout << "Switched to " << (use_gpu ? "CUDA" : "CPU") << " mode\n";
        break;
    case 'g':
    case 'G':
        make_gray = !make_gray;
        cout << "Convert image to gray: " << (make_gray ? "YES" : "NO") << endl;
        break;
    case '1':
        scale *= 1.05;
        cout << "Scale: " << scale << endl;
        break;
    case 'q':
    case 'Q':
        scale /= 1.05;
        cout << "Scale: " << scale << endl;
        break;
    case '2':
        nlevels++;
        cout << "Levels number: " << nlevels << endl;
        break;
    case 'w':
    case 'W':
        nlevels = max(nlevels - 1, 1);
        cout << "Levels number: " << nlevels << endl;
        break;
    case '3':
        gr_threshold++;
        cout << "Group threshold: " << gr_threshold << endl;
        break;
    case 'e':
    case 'E':
        gr_threshold = max(0, gr_threshold - 1);
        cout << "Group threshold: " << gr_threshold << endl;
        break;
    case '4':
        hit_threshold+=0.25;
        cout << "Hit threshold: " << hit_threshold << endl;
        break;
    case 'r':
    case 'R':
        hit_threshold = max(0.0, hit_threshold - 0.25);
        cout << "Hit threshold: " << hit_threshold << endl;
        break;
    case 'c':
    case 'C':
        gamma_corr = !gamma_corr;
        cout << "Gamma correction: " << gamma_corr << endl;
        break;
    default:
        return false;
    }
    return true;
}


void App::printHelp()
{
    cout << "Histogram of Oriented Gradients descriptor and detector sample.\n"
         << "\nUsage: demo_pedestrian_detect <frames_source>\n"
         << "  [--make-gray <true/false>] # convert image to gray one or not\n"
         << "  [--resize-src <true/false>] # do resize of the source image or not\n"
         << "  [--hit-threshold <double>] # classifying plane distance threshold (0.0 usually)\n"
         << "  [--scale <double>] # HOG window scale factor\n"
         << "  [--nlevels <int>] # max number of HOG window scales\n"
         << "  [--win-width <int>] # width of the window (48 or 64)\n"
         << "  [--win-stride-width <int>] # distance by OX axis between neighbour wins\n"
         << "  [--win-stride-height <int>] # distance by OY axis between neighbour wins\n"
         << "  [--gr-threshold <int>] # merging similar rects constant\n"
         << "  [--gamma-correct <int>] # do gamma correction or not\n"
         << "  [--write-video <bool>] # write video or not\n"
         << "  [--dst-video <path>] # output video path\n"
         << "  [--dst-video-fps <double>] # output video fps\n\n";
    BaseApp::printHelp();
}

RUN_APP(App)
