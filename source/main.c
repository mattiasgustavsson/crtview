#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include "app.h"
#include "frametimer.h"
#include "crtemu.h"
#include "crt_frame.h"
#include "crtemu_pc.h"
#include "crt_frame_pc.h"
#include "stb_image.h"


int app_proc( app_t* app, void* user_data ) {
    char const* filename = (char const*) user_data;
    app_title( app, "CRTView - UP/DOWN F11 ESC" );
    app_interpolation( app, APP_INTERPOLATION_NONE );
    app_screenmode_t screenmode = APP_SCREENMODE_WINDOW;
    app_screenmode( app, screenmode );
    frametimer_t* frametimer = frametimer_create( NULL );
    frametimer_lock_rate( frametimer, 60 );

    app_yield( app );
    crtemu_pc_t* crtemu_pc = crtemu_pc_create( NULL );
    crtemu_pc_frame( crtemu_pc, (CRTEMU_U32*) a_crt_frame, 1024, 1024 );

    crtemu_t* crtemu = NULL;

    CRT_FRAME_U32* frame = (CRT_FRAME_U32*) malloc( CRT_FRAME_WIDTH * CRT_FRAME_HEIGHT * sizeof( CRT_FRAME_U32 ) );
    crt_frame( frame );

    APP_U32 blank = 0;
    int w = 1;
    int h = 1;
    int c;

    APP_U32* xbgr = filename ? (APP_U32*) stbi_load( filename, &w, &h, &c, 4 ) : NULL;
    if( !xbgr ) {
        xbgr = &blank;
    }

    APP_U64 start = app_time_count( app );

    int mode = 0;

    int exit = 0;
    while( !exit && app_yield( app ) != APP_STATE_EXIT_REQUESTED ) {
        frametimer_update( frametimer );

        int newmode = mode;
        app_input_t input = app_input( app );
        for( int i = 0; i < input.count; ++i ) {
            if( input.events->type == APP_INPUT_KEY_DOWN ) {
                if( input.events->data.key == APP_KEY_ESCAPE ) {
                    exit = 1;
                }
                else if( input.events->data.key == APP_KEY_F11 ) {
                    screenmode = screenmode == APP_SCREENMODE_WINDOW ? APP_SCREENMODE_FULLSCREEN : APP_SCREENMODE_WINDOW;
                    app_screenmode( app, screenmode );
                } else if( input.events->data.key == APP_KEY_UP ) {
                    newmode = ( 3 + mode - 1 ) % 3;
                }
                else if( input.events->data.key == APP_KEY_DOWN ) {
                    newmode = ( mode + 1 ) % 3;
                }
            }
        }

        if( mode != newmode ) {
            if( mode == 0 ) {
                crtemu_pc_destroy( crtemu_pc );
                crtemu_pc = NULL;
            } else if ( mode == 1 ) {
                crtemu_destroy( crtemu );
                crtemu = NULL;
            }
            mode = newmode;
            if( mode == 0 ) {
                crtemu_pc = crtemu_pc_create( NULL );
                crtemu_pc_frame( crtemu_pc, (CRTEMU_U32*) a_crt_frame, 1024, 1024 );
            } else if ( mode == 1 ) {
                crtemu = crtemu_create( NULL );
                crtemu_frame( crtemu, frame, CRT_FRAME_WIDTH, CRT_FRAME_HEIGHT);
            }
        }

        APP_U64 div = app_time_freq( app ) / 1000000ULL;
        APP_U64 t = ( app_time_count( app ) - start ) / div;

        if( mode == 0 ) {
            crtemu_pc_present( crtemu_pc, t, xbgr, w, h, 0xffffff, 0x181818 );
            app_present( app, NULL, w, h, 0xffffff, 0x000000 );
        } else if( mode == 1 ) {
            crtemu_present( crtemu, t, xbgr, w, h, 0xffffff, 0x181818 );
            app_present( app, NULL, w, h, 0xffffff, 0x000000 );
        } else {
            app_present( app, xbgr, w, h, 0xffffff, 0x181818 );
        }
    }

    if( mode == 0 ) {
        crtemu_pc_destroy( crtemu_pc );
    } else if( mode == 1 ) {
        crtemu_destroy( crtemu );
    }
    free( frame );
    frametimer_destroy( frametimer );
    if( xbgr != &blank ) {
        stbi_image_free( (stbi_uc*) xbgr );
    }
    return 0;
}




/*
-------------------------
    ENTRY POINT (MAIN)
-------------------------
*/     

#if defined( _WIN32 ) && !defined( __TINYC__ )
    #ifndef NDEBUG
        #pragma warning( push ) 
        #pragma warning( disable: 4619 ) // pragma warning : there is no warning number 'number'
        #pragma warning( disable: 4668 ) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
        #include <crtdbg.h>
        #pragma warning( pop ) 
    #endif
#endif /* _WIN32 && !__TINYC__ */

int main( int argc, char** argv ) {
    (void) argc, (void) argv;
    #if defined( _WIN32 ) && !defined( __TINYC__ )
        #ifndef NDEBUG
            int flag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG ); // Get current flag
            flag ^= _CRTDBG_LEAK_CHECK_DF; // Turn on leak-checking bit
            _CrtSetDbgFlag( flag ); // Set flag to the new value
            _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG );
            _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
            //_CrtSetBreakAlloc( 0 );
        #endif
    #endif

    char* filename = argc > 1 ? argv[ 1 ] : NULL;
    return app_run( app_proc, filename, NULL, NULL, NULL );
} 

#ifdef _WIN32
    // pass-through so the program will build with either /SUBSYSTEM:WINDOWS or /SUBSYSTEM:CONSOLE
    struct HINSTANCE__;
    int __stdcall WinMain( struct HINSTANCE__* a, struct HINSTANCE__* b, char* c, int d ) { 
        (void) a, (void) b, (void) c, (void) d; 
        return main( __argc, __argv ); 
    }
#endif /* _WIN32 */


/*
---------------------------------
    LIBRARIES IMPLEMENTATIONS
---------------------------------
*/

#define APP_IMPLEMENTATION
#ifdef _WIN32
    #define APP_WINDOWS
#else
    #define APP_SDL    
#endif
#define APP_LOG( ctx, level, message )
#include "app.h"

#define CRTEMU_IMPLEMENTATION
#include "crtemu.h"

#define CRT_FRAME_IMPLEMENTATION
#include "crt_frame.h"

#define CRTEMU_PC_IMPLEMENTATION
#define CRTEMU_PC_REPORT_SHADER_ERRORS
#include "crtemu_pc.h"

#define FRAMETIMER_IMPLEMENTATION
#include "frametimer.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#pragma warning( push )
#pragma warning( disable: 4296 )
#include "stb_image.h"
#pragma warning( pop )