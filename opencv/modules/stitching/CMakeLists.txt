if(ANDROID)
    define_opencv_module(stitching opencv_core opencv_imgproc opencv_features2d opencv_calib3d opencv_flann opencv_objdetect)
else()
    define_opencv_module(stitching opencv_core opencv_imgproc opencv_features2d opencv_calib3d opencv_flann opencv_objdetect opencv_gpu)
endif()
