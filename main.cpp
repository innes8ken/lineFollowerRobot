#include "opencv2/opencv.hpp"
#include "serialib.h"
#include <boost/circular_buffer.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "neural.h"
#include "external.h"
#define CVUI_IMPLEMENTATION
#include "cvui.h"

#if defined(_WIN32) || defined(_WIN64)
#define DEVICE_PORT "COM4" // COM1 for windows
#endif

#ifdef __linux__
#define DEVICE_PORT "/dev/ttyUSB0" // This is for Arduino, ttyS0 for linux, otherwise ttyUSB0, if it does not open try: sudo chmod 666 /dev/ttyS0 or ttyUSB0
#endif

#define STAT_WINDOW "statistics & options"

using namespace cv;
using namespace std;
constexpr int ESC_key = 27;
const int numSens9 = 9;

int main(int, char **) {
  Extern* external = NULL;
  external = new Extern();
  int nPredictors = external->getNpredictors();
  srand(4); //random number generator
  cv::namedWindow("robot view");
  cvui::init(STAT_WINDOW);
  auto statFrame = cv::Mat(400, 600, CV_8UC3);
  initialize_samanet(nPredictors);
  serialib LS; // for Arduino
  char Ret = LS.Open(DEVICE_PORT, 115200); // for arduino
  
  if (Ret != 1) { // If an error occurred...
    printf("Error while opening port. Permission problem?\n");
    return Ret; // ... quit the application
  }
  char startChar = {'d'};
  Ret = LS.Write(&startChar, sizeof(startChar)); //start the communication
  printf("Serial port opened successfully !\n");
  VideoCapture cap(0); //0 for Rpi camera
  
  if (!cap.isOpened()) {
    printf("The selected video capture device is not available.\n");
    return -1;
  }
    std::vector<double> predictorDeltaMeans;
    predictorDeltaMeans.reserve(nPredictors);
    std::vector<uint8_t> sensorsArray;
    sensorsArray.reserve(numSens9);
  
  for (;;) {
    //getting the predictor signals from the camera
    statFrame = cv::Scalar(100, 100, 100);
    Mat origframe, frame;
    cap >> origframe; // get a new frame from camera
    flip(origframe,frame,-1); // 0 horizontal, 1 vertical, -1 both
    predictorDeltaMeans.clear();
    external->calcPredictors(frame, predictorDeltaMeans);
    // getting the error signal form sensors
    sensorsArray.clear();
    uint8_t readToThis[numSens9] = {0,0,0,0,0,0,0,0,0};
    Ret = LS.Read(&readToThis, sizeof(readToThis));
    
    for (int i = 0 ; i < numSens9; i++){
      sensorsArray.push_back(readToThis[i]);
      //cout << "return: " << (int16_t)readToThis[i] << endl;
    }
    //cout << "....................." << endl;

    //confirm serial communication
    /*
    int16_t repeatSignal[4] = {0,0,0,0};
    Ret = LS.Read(&repeatSignal, sizeof(repeatSignal));
    for (int i = 0 ; i < 4; i++){
      cout << "return: " << repeatSignal[i] << endl;
    }
    cout << "....................." << endl;*/

    double sensorError = external->calcError(statFrame, sensorsArray);
    
    if (Ret > 0){
      int speedError = external->onStepCompleted(statFrame, sensorError, predictorDeltaMeans);
      int16_t mainDifferentialVelocity = (int16_t)speedError;
      int16_t mainLeftVelocity = 0;
      int16_t mainRightVelocity = 0;
      int16_t startMarker = 32767;
      int16_t speedErrorArray[4] = {mainDifferentialVelocity, mainLeftVelocity , mainRightVelocity, startMarker};
    Ret = LS.Write(&speedErrorArray, sizeof(speedErrorArray));
  }
    // Show everything on the screen
    cvui::update();
    cv::imshow(STAT_WINDOW, statFrame);
    
    if (waitKey(20) == ESC_key)
      break;
  }
  return 0;
}
