#include <android_native_app_glue.h>
#include "Common.h"
#include "FpsCollector.h"
#include "Camera/AndroidCameraPermission.h"

#ifdef GRAPHIC_API_GLES
#include "GL/GLRenderer.h"
GLRenderer renderer{};
#else

#endif

FpsCollector collector("FPS", 1e9 / 85);

void CmdHandler(struct android_app *app, int32_t cmd) {
    switch(cmd) {
        case APP_CMD_START: {
            LOG_D("onStart()");
            LOG_D("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            LOG_D("onResume()");
            LOG_D("    APP_CMD_RESUME");
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            LOG_D("onGainedFocus()");
            LOG_D("    APP_CMD_GAINED_FOCUS");
            break;
        }
        case APP_CMD_PAUSE: {
            LOG_D("onPause()");
            LOG_D("    APP_CMD_PAUSE");
            break;
        }
        case APP_CMD_STOP: {
            LOG_D("onStop()");
            LOG_D("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            LOG_D("onDestroy()");
            LOG_D("    APP_CMD_DESTROY");
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            LOG_D("surfaceCreated()");
            LOG_D("    APP_CMD_INIT_WINDOW");
            renderer.Init(app);
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            LOG_D("surfaceDestroyed()");
            LOG_D("    APP_CMD_TERM_WINDOW");
            renderer.Destroy();
            break;
        }
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events.
 */
void android_main( struct android_app *app){
    LOG_D( "----------------------------------------------------------------" );
    LOG_D( "    android_main()" );

    app->onAppCmd = CmdHandler;

    //request camera permission
    AndroidCameraPermission::requestCameraPermission(app);

    int32_t result;
    android_poll_source* source;
    uint64_t frameIndex = 0;

    for (;;) {
        while((result = ALooper_pollAll(renderer.IsRunning() ? 0 : -1, nullptr, nullptr, reinterpret_cast<void**>(&source))) >= 0){
            if(source != nullptr)
                source->process(app, source);

            if(app->destroyRequested)
            {
                return;
            }
        }

        if(renderer.IsRunning()){
            renderer.ProcessFrame(frameIndex++);
            collector.collect();
        }
    }
}