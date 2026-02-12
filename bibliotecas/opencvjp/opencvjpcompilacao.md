g++ -shared -o bibliotecas/opencvjp/opencvjp.jpd bibliotecas/opencvjp/opencvjp.cpp -I bibliotecas/opencvjp/include -L bibliotecas/opencvjp/lib -lopencv_core455 -lopencv_highgui455 -lopencv_imgcodecs455 -lopencv_imgproc455 -lopencv_videoio455 -lopencv_objdetect455 -O3


g++ -c bibliotecas/opencvjp/opencvjp.cpp -o bibliotecas/opencvjp/opencvjp.o -I bibliotecas/opencvjp/include -O3
ar rcs bibliotecas/opencvjp/libopencvjp.a bibliotecas/opencvjp/opencvjp.o