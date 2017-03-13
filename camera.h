//#pragma once
#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/core/mmal_component_private.h"

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

#define VIDEO_FPS 25 
#define VIDEO_WIDTH 1280 
#define VIDEO_HEIGHT 720

#define SHUTTER_SPEED 2000
#define PREVIEW_WIDTH 640
#define PREVIEW_HEIGHT 360
#define PREVIEW_X 600
#define PREVIEW_Y 200

using namespace std;

typedef struct {
    int width;
    int height;
    MMAL_COMPONENT_T *camera;
    MMAL_COMPONENT_T *encoder;
    MMAL_COMPONENT_T *preview;
    MMAL_PORT_T *camera_preview_port;
    MMAL_PORT_T *camera_video_port;
    MMAL_PORT_T *camera_still_port;
    MMAL_PORT_T *encoder_input_port;
    MMAL_POOL_T *camera_still_port_pool;
    MMAL_PORT_T *encoder_output_port;
    MMAL_POOL_T *encoder_output_pool;
    float fps;
} PORT_USERDATA;

class CAMERA
{
private:	
	PORT_USERDATA userdata;
	bool ifPreview; // if preview
	int shutterSpeed;

public:
	static vector<FILE*> fp;
	static bool ifSavePic;
	static bool ifStopCamera;
	static int picNum;
	static int64_t stamp;
	static uint64_t returnTimeStamp;
	static int bufferCount;
	static int changeFPSCount;
	static int FPS;
	static long bufferStamp;
	static string savedFileName; // save a pic or a video , the name

	bool initCamera();
	bool setupCamera(PORT_USERDATA *userdata);
	bool setupEncoder(PORT_USERDATA *userdata);
	bool setupPreview(PORT_USERDATA *userdata);
	bool setupNoPreview(PORT_USERDATA *userdata);
//	static void* startCamera(void* pThis);
	void startCamera();
	void stopCamera();
	static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
	static void encoder_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

	static void setupSaveVideo(int bufferC);//setup filename and saving pics num;
	void setupSavePic();//setup filename and saving pics num;
	int ifSaveEnd();
	static void closeFiles();
	void changeShutterSpeed(const int speed);
};

#endif
