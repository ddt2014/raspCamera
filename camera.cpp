#include "camera.h"

vector<FILE*> CAMERA :: fp;
long CAMERA :: bufferStamp = 0;
int CAMERA :: bufferCount = 0;
int CAMERA :: changeFPSCount = 0;
int CAMERA :: FPS = 0;
int CAMERA :: picNum = 0;
int64_t CAMERA :: stamp = 0;
uint64_t CAMERA :: returnTimeStamp = 0;
bool CAMERA :: ifSavePic = false;
string CAMERA :: savedFileName = "";

int fill_port_buffer(MMAL_PORT_T *port, MMAL_POOL_T *pool) {
    int q;
    int num = mmal_queue_length(pool->queue);
    for (q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        if (!buffer)
			printf("Unable to get a required buffer %d from pool queue\n", q);
        if (mmal_port_send_buffer(port, buffer) != MMAL_SUCCESS) 
            printf("Unable to send a buffer to port (%d)\n", q);
    }
}

bool  CAMERA :: setupCamera(PORT_USERDATA *userdata) {
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T * camera_preview_port;
    MMAL_PORT_T * camera_video_port;
    MMAL_PORT_T * camera_still_port;
    MMAL_POOL_T * camera_still_port_pool;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    if (status != MMAL_SUCCESS) 
	{
        printf("Error: create camera %x\n", status);
        return false;
    }
    userdata->camera = camera;
    userdata->camera_preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    userdata->camera_video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    userdata->camera_still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];
    camera_preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    camera_video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    camera_still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    MMAL_PARAMETER_INT32_T camera_num = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, 0};
    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to enable camera video port (%u)\n", status);
        return false;
    }
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof (cam_config)},
            .max_stills_w = VIDEO_WIDTH,
            .max_stills_h = VIDEO_HEIGHT,
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w = VIDEO_WIDTH,
            .max_preview_video_h = VIDEO_HEIGHT,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
        };

        mmal_port_parameter_set(camera->control, &cam_config.hdr);
	mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
	MMAL_PARAMETER_MIRROR_T mirror = {{MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)}, MMAL_PARAM_MIRROR_NONE};
	mirror.value = MMAL_PARAM_MIRROR_VERTICAL; 
      //mmal_port_parameter_set(camera->control, &mirror.hdr);
	mmal_port_parameter_set(camera_preview_port, &mirror.hdr);
	mmal_port_parameter_set(camera_still_port, &mirror.hdr);
	mmal_port_parameter_set(camera_video_port, &mirror.hdr);
    }

    // Setup camera preview port format 
    format = camera_preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = VIDEO_WIDTH;
    format->es->video.height = VIDEO_HEIGHT;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = VIDEO_WIDTH;
    format->es->video.crop.height = VIDEO_HEIGHT;
    status = mmal_port_format_commit(camera_preview_port);
    if (status != MMAL_SUCCESS) 
	{
        printf("Error: camera viewfinder format couldn't be set\n");
        return false;
    }

    // Setup camera video port format
    mmal_format_copy(camera_video_port->format, camera_preview_port->format);
    format = camera_video_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = VIDEO_WIDTH;
    format->es->video.height = VIDEO_HEIGHT;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = VIDEO_WIDTH;
    format->es->video.crop.height = VIDEO_HEIGHT;
    format->es->video.frame_rate.num = VIDEO_FPS;
    format->es->video.frame_rate.den = 1;
    camera_video_port->buffer_size = format->es->video.width * format->es->video.height * 12 / 8;
    camera_video_port->buffer_num = 2;
    status = mmal_port_format_commit(camera_video_port);
    if (status != MMAL_SUCCESS) 
    {
        printf("Error: unable to commit camera video port format (%u)\n", status);
        return false;
    }
    camera_video_port->userdata = (struct MMAL_PORT_USERDATA_T *) userdata;

    // Setup camera still port format
    mmal_format_copy(camera_still_port->format, camera_preview_port->format);
    format = camera_still_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = VIDEO_WIDTH;
    format->es->video.height = VIDEO_HEIGHT;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = VIDEO_WIDTH;
    format->es->video.crop.height = VIDEO_HEIGHT;
    format->es->video.frame_rate.num = VIDEO_FPS;
    format->es->video.frame_rate.den = 1;
    camera_still_port->buffer_size = format->es->video.width * format->es->video.height * 12 / 8;
    camera_still_port->buffer_num = 2; 
    status = mmal_port_format_commit(camera_still_port);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to commit camera video port format (%u)\n", status);
        return false;
    }
    camera_still_port->userdata = (struct MMAL_PORT_USERDATA_T *) userdata;

    camera_still_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(camera_still_port, 
    camera_still_port->buffer_num, camera_still_port->buffer_size);
    userdata->camera_still_port_pool = camera_still_port_pool;
    status = mmal_port_enable(camera_still_port, camera_control_callback);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to enable camera video port (%u)\n", status);
        return false;
    }

//    status = mmal_component_enable(camera);
//    if (status != MMAL_SUCCESS) { 
//        printf("Error: unable to enable camera (%u)\n", status);
//        return false;
//    }

    fill_port_buffer(userdata->camera_still_port, userdata->camera_still_port_pool);

    if (mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) 
        printf("%s: Failed to start capture\n", __func__);
    return true;
}

bool CAMERA :: setupEncoder(PORT_USERDATA *userdata) {
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input_port = NULL, *encoder_output_port = NULL;
    MMAL_POOL_T *encoder_output_port_pool;
    MMAL_CONNECTION_T *camera_encoder_connection = 0;


    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
    if (status != MMAL_SUCCESS) 
	{
        printf("Error: unable to create preview (%u)\n", status);
        return false;
    }

    encoder_input_port = encoder->input[0];
    encoder_output_port = encoder->output[0];
    userdata->encoder_input_port = encoder_input_port;
    userdata->encoder_output_port = encoder_output_port;

    mmal_format_copy(encoder_input_port->format, userdata->camera_video_port->format);
    mmal_format_copy(encoder_output_port->format, encoder_input_port->format);

    // Only supporting H264 at the moment
    encoder_output_port->format->encoding = MMAL_ENCODING_H264;
    encoder_output_port->format->bitrate = 2000000;
    encoder_output_port->buffer_num = encoder_output_port->buffer_num_recommended;
    if (encoder_output_port->buffer_num < encoder_output_port->buffer_num_min) 
        encoder_output_port->buffer_num = encoder_output_port->buffer_num_min;
    if (mmal_port_parameter_set_boolean(encoder_output_port, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, 1) != MMAL_SUCCESS) {
      printf("failed to set INLINE HEADER FLAG parameters");
    }
    status = mmal_port_format_commit(encoder_output_port);
    if (status != MMAL_SUCCESS) 
	{
        printf("Error: unable to commit encoder output port format (%u)\n", status);
        return false;
    }

    encoder_output_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(encoder_output_port, encoder_output_port->buffer_num, encoder_output_port->buffer_size);
    userdata->encoder_output_pool = encoder_output_port_pool;
    encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *) userdata;
    status = mmal_connection_create(&camera_encoder_connection, userdata->camera_video_port, encoder_input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
    if (status != MMAL_SUCCESS)
    {
        printf("encoder Error: unable to create connection (%u)\n", status);
        return false;
    }
    status = mmal_connection_enable(camera_encoder_connection);
    if (status != MMAL_SUCCESS)
    {
        printf("Error: unable to enable connection (%u)\n", status);
        return false;
    }
   status = mmal_port_enable(encoder_output_port, encoder_output_buffer_callback);
    if (status != MMAL_SUCCESS) {
        printf("Error: unable to enable encoder output port (%u)\n", status);
        return false;
    }
    fill_port_buffer(encoder_output_port, encoder_output_port_pool);

   return true;
}

bool CAMERA :: setupPreview(PORT_USERDATA *userdata) {
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *preview = 0;
    MMAL_CONNECTION_T *camera_preview_connection = 0;
    MMAL_PORT_T *preview_input_port;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &preview);
    if (status != MMAL_SUCCESS) 
    {
        printf("Error: unable to create preview (%u)\n", status);
        return false;
    }
    userdata->preview = preview;
    preview_input_port = preview->input[0];
    {
        MMAL_DISPLAYREGION_T param;
        param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        param.hdr.size = sizeof (MMAL_DISPLAYREGION_T);
        param.set = MMAL_DISPLAY_SET_LAYER;
        param.layer = 0;
        param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
        param.fullscreen = 1;
        status = mmal_port_parameter_set(preview_input_port, &param.hdr);
        if (status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
            printf("Error: unable to set preview port parameters (%u)\n", status);
            return false;
        }
    }
    status = mmal_connection_create(&camera_preview_connection, userdata->camera_preview_port, preview_input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
    if (status != MMAL_SUCCESS)
    {
        printf("Error: unable to create connection (%u)\n", status);
        return false;
    }
    status = mmal_connection_enable(camera_preview_connection);
    if (status != MMAL_SUCCESS)
    {
        printf("Error: unable to enable connection (%u)\n", status);
        return false;
    }
    return true;
}

bool CAMERA :: setupNoPreview(PORT_USERDATA *userdata) {
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *preview = 0;
    MMAL_CONNECTION_T *camera_preview_connection = 0;
    MMAL_PORT_T *preview_input_port;

    status = mmal_component_create("vc.null_sink", &preview);
    if (status != MMAL_SUCCESS) 
    {
        printf("Error: unable to create preview (%u)\n", status);
        return false;
    }
    userdata->preview = preview;
    preview_input_port = preview->input[0];
    status = mmal_connection_create(&camera_preview_connection, userdata->camera_preview_port, preview_input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
    if (status != MMAL_SUCCESS)
    {
        printf("Error: unable to create connection (%u)\n", status);
        return false;
    }
    status = mmal_connection_enable(camera_preview_connection);
    if (status != MMAL_SUCCESS)
    {
        printf("Error: unable to enable connection (%u)\n", status);
        return false;
    }
    return true;
}

bool CAMERA :: initCamera() {
	bcm_host_init();
    userdata.width = VIDEO_WIDTH;
    userdata.height = VIDEO_HEIGHT;
	picNum = 33;
    shutterSpeed = SHUTTER_SPEED;

	if (!setupCamera(&userdata))
	{
        printf("Error: setup camera \n");
        return false;
    }
	if (!setupEncoder(&userdata))
	{
        printf("Error: setup Encoder \n");
        return false;
    }
	if (!setupPreview(&userdata))
	{
        printf("Error: setup Preview \n");
        return false;
    }

	return true;
}

void CAMERA :: startCamera()
{
    MMAL_STATUS_T status;

//    status = mmal_component_enable(userdata.camera);
//    if (status != MMAL_SUCCESS) { 
//        printf("Error: unable to enable camera (%u)\n", status);
//    }
}       

void CAMERA :: camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer;
    PORT_USERDATA *userdata = (PORT_USERDATA *) port->userdata;
    MMAL_POOL_T *pool = userdata->camera_still_port_pool;

    if(buffer->length > 5) {
        int bytes_written;
        int64_t currentTime;
        currentTime = buffer->pts;
        savedFileName = ".raw";
        while(currentTime > 0) {
        	savedFileName = "0" + savedFileName;
        	savedFileName[0] += currentTime % 10;
        	currentTime /= 10;
        }
        FILE* file = fopen(savedFileName.c_str(), "w");

        mmal_buffer_header_mem_lock(buffer);
        bytes_written = fwrite(buffer->data, 1, buffer->length, file);
        mmal_buffer_header_mem_unlock(buffer);
        if (bytes_written != buffer->length) {
            printf("Failed to write buffer data ()- aborting");
        } 
        fclose(file);
        cout << "pic :" << savedFileName << " saved." <<  endl;
    }
    
    mmal_buffer_header_release(buffer);
    if (port->is_enabled) {
        MMAL_STATUS_T status;
        new_buffer = mmal_queue_get(pool->queue);
        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS) {
            printf("1Unable to return a buffer to the video port\n");
        }
    }

}

 void CAMERA :: encoder_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    PORT_USERDATA *userdata = (PORT_USERDATA *) port->userdata;
    MMAL_BUFFER_HEADER_T *new_buffer;
    MMAL_POOL_T *pool = userdata->encoder_output_pool;
    int bytes_written;
    int64_t currentTime;
    uint64_t time;
    if(bufferCount) {
        mmal_buffer_header_mem_lock(buffer);
        bytes_written = fwrite(buffer->data, 1, buffer->length, fp[0]);
        bufferCount--;
        currentTime = vcos_getmicrosecs64();
        cout << buffer->pts << "-" <<  buffer->pts - stamp << "-" << bufferCount << "-" << bytes_written << endl;
        stamp = buffer->pts;
/*
	if(bufferCount == 100) {
		MMAL_PARAMETER_FRAME_RATE_T fps = {{MMAL_PARAMETER_VIDEO_FRAME_RATE, sizeof(fps)}, {25, 1}};
		mmal_port_parameter_set(userdata->camera_video_port, &fps.hdr);
		mmal_port_parameter_set(userdata->camera_still_port, &fps.hdr);
		mmal_port_parameter_set(userdata->camera->control, &fps.hdr);
	}
	if(bufferCount == 130) {
		MMAL_PARAMETER_FRAME_RATE_T fps = {{MMAL_PARAMETER_VIDEO_FRAME_RATE, sizeof(fps)}, {20, 1}};
		mmal_port_parameter_set(userdata->camera_video_port, &fps.hdr);
		mmal_port_parameter_set(userdata->camera_still_port, &fps.hdr);
		mmal_port_parameter_set(userdata->camera->control, &fps.hdr);
	}
*/
        if(bufferCount == 0) {
            cout << "saving finished" << endl;
            fflush(fp[0]);
            fclose(fp[0]);
		    string command = "MP4Box -fps 25 -add ";
		    command += savedFileName;
            command += " " + savedFileName.substr(0, savedFileName.length() - 5) + ".mp4";
            system(command.c_str());
        }
        mmal_buffer_header_mem_unlock(buffer);
        if (bytes_written != buffer->length) {
            printf("Failed to write buffer data ()- aborting");
        } 
    }

    if(changeFPSCount > 0) {
        --changeFPSCount;
        if(FPS > 0) {
            MMAL_PARAMETER_FRAME_RATE_T fps = {{MMAL_PARAMETER_VIDEO_FRAME_RATE, sizeof(fps)}, {FPS, 1}};
            mmal_port_parameter_set(userdata->camera_video_port, &fps.hdr);
            mmal_port_parameter_set(userdata->camera_still_port, &fps.hdr);
            mmal_port_parameter_set(userdata->camera->control, &fps.hdr);
            FPS = 0;
            cout << "start to change fps" << endl;
        }
        if(changeFPSCount == 0) {
            MMAL_PARAMETER_FRAME_RATE_T fps = {{MMAL_PARAMETER_VIDEO_FRAME_RATE, sizeof(fps)}, {VIDEO_FPS, 1}};
            mmal_port_parameter_set(userdata->camera_video_port, &fps.hdr);
            mmal_port_parameter_set(userdata->camera_still_port, &fps.hdr);
            mmal_port_parameter_set(userdata->camera->control, &fps.hdr);
            cout << "change finished" << endl;
        }
    }

    mmal_buffer_header_release(buffer);
    if (port->is_enabled) {
        MMAL_STATUS_T status;
        new_buffer = mmal_queue_get(pool->queue);
        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS) {
            printf("3Unable to return a buffer to the video port\n");
        }
    }
}

void CAMERA :: setupSaveVideo(int bufferC) {// save video
	int64_t currentTime = vcos_getmicrosecs64() % 10000000;
	savedFileName = ".h264";
	while(currentTime > 0) {
		savedFileName = "0" + savedFileName;
		savedFileName[0] += currentTime % 10;
		currentTime /= 10;
	}
	fp.push_back(fopen(savedFileName.c_str(), "a"));
	bufferCount = bufferC;
}

void CAMERA :: setupSavePic() {// save pic
    if (mmal_port_parameter_set_boolean(userdata.camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) 
        printf("%s: Failed to start capture\n", __func__);
}

void CAMERA :: closeFiles() {
	for(int i = 0; i < fp.size(); ++i)
	{
		fflush(fp[i]);
		fclose(fp[i]);
	}
	fp.clear();
	cout << "close file finished" << endl;
}

int CAMERA :: ifSaveEnd() {
	return picNum;
}

void CAMERA :: changeShutterSpeed(const int speed) {
    shutterSpeed = speed;
    mmal_port_parameter_set_uint32(userdata.camera->control, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
    mmal_port_parameter_set_uint32(userdata.camera_preview_port, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
    mmal_port_parameter_set_uint32(userdata.camera_video_port, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
    mmal_port_parameter_set_uint32(userdata.camera_still_port, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
    mmal_port_parameter_set_uint32(userdata.preview->control, MMAL_PARAMETER_SHUTTER_SPEED, shutterSpeed);
    cout << "change shutter speed over" << endl;
}
