#include <cassert>
#include <cstring>
#include <iostream>

#include <unistd.h>

#include "raylib.h"

#define READ_END 0
#define WRITE_END 1

ssize_t readBuf(int fd, ssize_t len, char* buf) {
  ssize_t tmp = 0, ret = 0, want = len;
  do {
    ret = read(fd, buf + tmp, want);
    tmp += ret;
    want -= ret;
  } while (want > 0);

  return tmp;
}

int main(int argc, char *argv[])
{
  int pipefd[2];

  if (pipe(pipefd) < 0) {
    fprintf(stderr, "ERROR: could not create a pipe: %s\n", strerror(errno));
    return -1;
  }

  pid_t child = fork();
  if (child < 0) {
      fprintf(stderr, "ERROR: could not fork a child: %s\n", strerror(errno));
      return -1;
  }

  if (child == 0) {
    if (dup2(pipefd[WRITE_END], STDOUT_FILENO) < 0) {
      fprintf(stderr, "ERROR: could not reopen read end of pipe as stdin: %s\n", strerror(errno));
      exit(1);
    }
    close(pipefd[READ_END]);

    int ret = execlp("ffmpeg",
      "ffmpeg",
      "-loglevel", "verbose",
      "-y",

      "-f", "hevc",
      "-r", "20",
      "-i", "video.hevc",

      "-s", "512x256",
      "-pix_fmt", "yuv420p",
      "-f", "rawvideo",
      "-",
      NULL
    );
    if (ret < 0) {
        fprintf(stderr, "ERROR: could not run ffmpeg as a child process: %s\n", strerror(errno));
        exit(1);
    }
    assert(0 && "unreachable");
  }

  close(pipefd[WRITE_END]);

  SetTraceLogLevel(LOG_TRACE);
  InitWindow(640, 480, "tcpilot");

  SetWindowState(FLAG_WINDOW_TOPMOST);
  
  char buf[512*256] = {0}, abuf[512*256 * 3 / 2] = {0};
  char ubuf[256*128] = {0}, vbuf[256*128] = {0};
  ssize_t ret;

  SetTargetFPS(20);

  Image yimg = {
      .data = buf,
      .width = 512,
      .height = 256,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
    };

  Image uimg = {
      .data = buf,
      .width = 256,
      .height = 128,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
    };

  Image vimg = {
      .data = buf,
      .width = 256,
      .height = 128,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
    };

  

  Texture2D ytexture = LoadTextureFromImage(yimg);
  Texture2D utexture = LoadTextureFromImage(uimg);
  Texture2D vtexture = LoadTextureFromImage(vimg);

  
  while(!WindowShouldClose()) {
    ret = readBuf(pipefd[READ_END], 512 * 256 * 3 / 2, abuf);
    memcpy(buf, abuf, 512 * 256);
    memcpy(ubuf, abuf + 512 * 256, 512 * 256 / 4);
    memcpy(vbuf, abuf + (512 * 256) + (512 * 256 / 4), 512 * 256 / 4);
    UpdateTexture(ytexture, buf);
    UpdateTexture(utexture, ubuf);
    UpdateTexture(vtexture, vbuf);

    
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawFPS(512, 256);
    DrawTexture(ytexture, 0, 0, WHITE);
    DrawTexture(utexture, 0, 256, WHITE);
    DrawTexture(vtexture, 256, 256, WHITE);
    EndDrawing();
  }

   CloseWindow();

   close(pipefd[READ_END]);
   
  return 0;
}
