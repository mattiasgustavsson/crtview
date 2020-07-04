![build](https://github.com/mattiasgustavsson/crtview/workflows/build/badge.svg)
# crtview
View any image with my CRT filter applied
Use Up/Down cursor keys to toggle display mode
Use F11 to toggle fullscreen
ESC to exit

Pass the filename of your image as command-line parameter (or just drag-drop it onto the EXE)

Build instructions
------------------

### Windows
From a Visual Studio command prompt, go to the `runtime` folder and do
```
cl source\main.c
```

### Mac
Go to the `runtime` folder and do
```
clang source/main.c -lSDL2 -lGLEW -framework OpenGL
```

SDL2 and GLEW are required - if you don't have then installed you can do so by running
```
brew install sdl2 glew
```

### Linux
Go to the `runtime` folder and do
```
gcc source/main.c -lSDL2 -lGLEW -lGL -lm -lpthread
```

SDL2 and GLEW are required - if you don't have them installed you can do so by running
```
sudo apt-get install libsdl2-dev
sudo apt-get install libglew-dev
```


