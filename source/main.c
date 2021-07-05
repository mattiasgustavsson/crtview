#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "app.h"
#include "frametimer.h"
#include "crtemu.h"
#include "crt_frame.h"
#include "crtemu_pc.h"
#include "crt_frame_pc.h"
#include "stb_image.h"
#include "gif_load.h"

typedef struct jo_gif_t jo_gif_t;

jo_gif_t* export_start( app_t* app );

void export_frame( app_t* app, jo_gif_t* gif, int delay );
void export_end( jo_gif_t* gif );

typedef struct gif_t {
    uint32_t* pict;
    uint32_t* prev;
    unsigned long last;
    int width;
    int height;
    uint32_t** frames;
    int* times;
    int capacity;
    int count;
} gif_t;


void gif_frame( void *data, struct GIF_WHDR* whdr ) {
    uint32_t x, y, yoff, iter, ifin, dsrc;
    gif_t* gif = (gif_t*)data;

    #define BGRA(i) ((whdr->bptr[i] == whdr->tran)? 0 : \
          ((uint32_t)(whdr->cpal[whdr->bptr[i]].B << ((GIF_BIGE)? 8 : 16)) \
         | (uint32_t)(whdr->cpal[whdr->bptr[i]].G << ((GIF_BIGE)? 16 : 8)) \
         | (uint32_t)(whdr->cpal[whdr->bptr[i]].R << ((GIF_BIGE)? 24 : 0)) \
         | ((GIF_BIGE)? 0xFF : 0xFF000000)))
    if (!whdr->ifrm) {
        uint32_t ddst = (uint32_t)(whdr->xdim * whdr->ydim);
        gif->pict = (uint32_t*)calloc(sizeof(uint32_t), ddst);
        gif->prev = (uint32_t*)calloc(sizeof(uint32_t), ddst);
        gif->width = whdr->xdim;
        gif->height = whdr->ydim;
        gif->capacity = 16;
        gif->count = 0;
        gif->frames = (uint32_t**) malloc( sizeof( uint32_t* ) * gif->capacity );
        gif->times = (int*) malloc( sizeof( int ) * gif->capacity );
    }

    /** [TODO:] the frame is assumed to be inside global bounds,
                however it might exceed them in some GIFs; fix me. **/
    uint32_t* pict = (uint32_t*)gif->pict;
    uint32_t ddst = (uint32_t)(whdr->xdim * whdr->fryo + whdr->frxo);
    ifin = (!(iter = (whdr->intr)? 0 : 4))? 4 : 5; /** interlacing support **/
    for (dsrc = (uint32_t)-1; iter < ifin; iter++)
        for (yoff = 16U >> ((iter > 1)? iter : 1), y = (8 >> iter) & 7;
             y < (uint32_t)whdr->fryd; y += yoff)
            for (x = 0; x < (uint32_t)whdr->frxd; x++)
                if (whdr->tran != (long)whdr->bptr[++dsrc])
                    pict[(uint32_t)whdr->xdim * y + x + ddst] = BGRA(dsrc);

    if( gif->count >= gif->capacity ) {
        gif->capacity *= 2;
        gif->frames = (uint32_t**) realloc( gif->frames, sizeof( uint32_t* ) * gif->capacity );
        gif->times = (int*) realloc( gif->times, sizeof( int ) * gif->capacity );
    }
    uint32_t* frame = (uint32_t*) malloc( sizeof( uint32_t ) * whdr->xdim * whdr->ydim );
    memcpy( frame, gif->pict, sizeof( uint32_t ) * whdr->xdim * whdr->ydim );
    gif->times[ gif->count ] = whdr->time;
    gif->frames[ gif->count++ ] = frame;

    if ((whdr->mode == GIF_PREV) && !gif->last) {
        whdr->frxd = whdr->xdim;
        whdr->fryd = whdr->ydim;
        whdr->mode = GIF_BKGD;
        ddst = 0;
    }
    else {
        gif->last = (whdr->mode == GIF_PREV)?
                      gif->last : (unsigned long)(whdr->ifrm + 1);
        pict = (uint32_t*)((whdr->mode == GIF_PREV)? gif->pict : gif->prev);
        uint32_t* prev = (uint32_t*)((whdr->mode == GIF_PREV)? gif->prev : gif->pict);
        for (x = (uint32_t)(whdr->xdim * whdr->ydim); --x;
             pict[x - 1] = prev[x - 1]);
    }
    if (whdr->mode == GIF_BKGD) /** cutting a hole for the next frame **/
        for (whdr->bptr[0] = (uint8_t)((whdr->tran >= 0)?
                                        whdr->tran : whdr->bkgd), y = 0,
             pict = (uint32_t*)gif->pict; y < (uint32_t)whdr->fryd; y++)
            for (x = 0; x < (uint32_t)whdr->frxd; x++)
                pict[(uint32_t)whdr->xdim * y + x + ddst] = BGRA(0);
    #undef BGRA
}


gif_t* load_gif( char const* filename ) {
    FILE* fp = fopen( filename, "rb" );
    if( !fp ) {
        return NULL;
    }
    fseek( fp, 0, SEEK_END );
    size_t size = ftell( fp );
    fseek( fp, 0, SEEK_SET );
    void* data = malloc( size );
    fread( data, 1, size, fp );
    fclose( fp );
    gif_t* gif = (gif_t*)malloc( sizeof( gif_t ) );
    long result = GIF_Load( data, (long)size, gif_frame, NULL, (void*)gif, 0L );
    if( result == 0 ) {
        free( data );
        free( gif );
        return NULL;
    }

    free( gif->pict );
    free( gif->prev );
    return gif;
}

int app_proc( app_t* app, void* user_data ) {
    char const* filename = (char const*) user_data;
    app_title( app, "CRTView - UP/DOWN F11 ESC - F8 Export GIF" );
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


    gif_t* gif = filename ? load_gif( filename ) : NULL;
    if( gif ) {
        w = gif->width;
        h = gif->height;
    }
    APP_U32* xbgr = filename && !gif ? (APP_U32*) stbi_load( filename, &w, &h, &c, 4 ) : NULL;
    int delay = gif ? gif->times[ 0 ] * 10 : 0;

    APP_U64 start = app_time_count( app );

    bool prepare_export = false;
    jo_gif_t* exp_gif = NULL;
    
    int curr_frame = 0;
    int mode = 0;

    int exit = 0;
    while( !exit && app_yield( app ) != APP_STATE_EXIT_REQUESTED ) {
        frametimer_update( frametimer );

        char const* dropped_file = app_last_dropped_file(app);
        if (dropped_file) {
            gif = load_gif( dropped_file );
            if( gif ) {
                w = gif->width;
                h = gif->height;
            }
            xbgr = !gif ? (APP_U32*) stbi_load( dropped_file, &w, &h, &c, 4 ) : NULL;
            delay = gif ? gif->times[ 0 ] : 0;
            curr_frame = 0;
        }

        if( gif ) {
            delay -= 16;
            if( delay <= 0 ) {
                if( exp_gif && !prepare_export ) {
                   export_frame( app, exp_gif, gif->times[ curr_frame ] );
                }
                curr_frame = curr_frame + 1;
                if( curr_frame >= gif->count ) {
                    if( exp_gif && !prepare_export ) {
                        export_end( exp_gif );
                        exp_gif = NULL;
                        app_title( app, "CRTView - UP/DOWN F11 ESC - F8 Export GIF" );
                    }
                    curr_frame = 0;
                    prepare_export = false;
                }
                delay = gif->times[ curr_frame ] * 10;
            }
        }

        int newmode = mode;
        app_input_t input = app_input( app );
        for( int i = 0; i < input.count; ++i ) {
            if( input.events->type == APP_INPUT_KEY_DOWN ) {
                if( input.events->data.key == APP_KEY_ESCAPE ) {
                    exit = 1;
                } else if( input.events->data.key == APP_KEY_F11 ) {
                    screenmode = screenmode == APP_SCREENMODE_WINDOW ? APP_SCREENMODE_FULLSCREEN : APP_SCREENMODE_WINDOW;
                    app_screenmode( app, screenmode );
                } else if( input.events->data.key == APP_KEY_UP ) {
                    newmode = ( 3 + mode - 1 ) % 3;
                } else if( input.events->data.key == APP_KEY_DOWN ) {
                    newmode = ( mode + 1 ) % 3;
                } else if( input.events->data.key == APP_KEY_F8 ) {
                    if( gif && !exp_gif ) {
                        exp_gif = export_start( app );
                        curr_frame = 0;
                        delay = gif->times[ curr_frame ] * 10;
                        prepare_export = true;
                        app_title( app, "CRTView - EXPORTING GIF" );
                    }
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
        if( exp_gif ) t = 0;

        if( mode == 0 ) {
            crtemu_pc_present( crtemu_pc, t, xbgr ? xbgr : gif ? gif->frames[ curr_frame ] : &blank, w, h, 0xffffff, 0x181818 );
            app_present( app, NULL, w, h, 0xffffff, 0x000000 );
        } else if( mode == 1 ) {
            crtemu_present( crtemu, t, xbgr ? xbgr : gif ? gif->frames[ curr_frame ] : &blank, w, h, 0xffffff, 0x181818 );
            app_present( app, NULL, w, h, 0xffffff, 0x000000 );
        } else {
            app_present( app, xbgr ? xbgr : gif ? gif->frames[ curr_frame ] : &blank, w, h, 0xffffff, 0x181818 );
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


#include "jo_gif.h"


jo_gif_t* export_start( app_t* app ) {
    APP_GLint dims[4] = {0};
    app->gl.GetIntegerv(APP_GL_VIEWPORT, dims);
    int w = dims[2];
    int h = dims[3];

    jo_gif_t* jo_gif = malloc( sizeof( jo_gif_t ) );
    *jo_gif = jo_gif_start("crtview.gif", w, h, 0, 256); 
    return jo_gif;
}


void export_frame( app_t* app, jo_gif_t* gif, int delay ) {
    uint8_t* pixels = malloc( 4 * gif->width * gif->height * 2 );  
    
    app->gl.ReadBuffer( APP_GL_FRONT );
    app->gl.ReadPixels( 0, 0, gif->width, gif->height, APP_GL_RGBA, APP_GL_UNSIGNED_BYTE, pixels + 4 * gif->width * gif->height );
    for( int y = 0; y < gif->height; ++y ) {
        memcpy( pixels + 4 * gif->width * y, pixels + 4 * gif->width * ( gif->height * 2 - 1 - y ), 4 * gif->width );
    }
    jo_gif_frame( gif, pixels, delay, true ); 
    free( pixels );
}


void export_end( jo_gif_t* gif ) {
    jo_gif_end(gif);
    free( gif );
}
