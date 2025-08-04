#include <stdbool.h>
#include <stdio.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raylib.h"
#include <dirent.h>
#include "fftw3.h"
#include <string.h>
#include <stdlib.h>
#include "raylib.h"
#include <math.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BAR_COUNT 90
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define CONTROL_PANEL_HEIGHT 150
#define MUSIC_LIST_WIDTH 400
#define BAR_WIDTH 5
#define BAR_SPACING 1
#define FFT_MAGNITUDE_SCALING 0.4f
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 40
#define SEEK_TIME 10.0f
#define SMOOTHING_WINDOW 0
#define MAX_BAR_HEIGHT 300
#define MIN_BAR_HEIGHT 1.0f
#define SMOOTHING_FACTOR 0.1f
#define BACKGROUND_COLOR (Color){0,0,0,255}
#define PADDING 30
double lastColorChangeTime = 0.0;
const double COLOR_CHANGE_INTERVAL = 0.5;


float audioData[BUFFER_SIZE];
float fftData[BAR_COUNT];
float barHeights[BAR_COUNT];
Color barColors[BAR_COUNT];
fftwf_plan fftPlan;

Music music;
bool isPlaying    = false;
bool isPaused     = false;
bool shuffle_play = false;
int  current_play = 0;
bool stuffle_play_on_or_off = false;

char musicFiles[2048][512];
int  musicFileCount = 0;
int  selectedFile   = -1;

void AudioProcessor(void *audioStream, unsigned int frameCount) {
    float *audio = (float *)audioStream;
    for (int i = 0; i < BUFFER_SIZE; i++) audioData[i] = audio[i];
    fftwf_execute(fftPlan);
    for (int i = 0; i < BAR_COUNT; i++)
        fftData[i] = audioData[i] * audioData[i];
}

void LoadMusicFiles(const char *path) {
    DIR *dp = opendir(path);
    if (!dp) return;
    struct dirent *entry;
    while ((entry = readdir(dp))) {
        if (entry->d_type == DT_REG &&
           (strstr(entry->d_name, ".mp3") ||
            strstr(entry->d_name, ".wav") ||
            strstr(entry->d_name, ".ogg"))) {
            snprintf(musicFiles[musicFileCount],
                     sizeof(musicFiles[musicFileCount]),
                     "%s/%s", path, entry->d_name);
            musicFileCount++;
        }
    }
    closedir(dp);
}
void PlayMusicByIndex(int idx) {
    if (idx < 0 || idx >= musicFileCount) return;
    if (isPlaying) StopMusicStream(music);
    UnloadMusicStream(music);
    music = LoadMusicStream(musicFiles[idx]);
    PlayMusicStream(music);
    AttachAudioStreamProcessor(music.stream, AudioProcessor);
    isPlaying    = true;
    isPaused     = false;
    current_play = idx;
}
void ChangeBarColors(void) {
    Color start = (Color){rand() % 256, rand() % 256, rand() % 256, 255},
          mid   = (Color){rand() % 256, rand() % 256, rand() % 256, 255},
          end   = (Color){rand() % 256, rand() % 256, rand() % 256, 255};
    for (int i = 0; i < BAR_COUNT; i++) {
        float r   = (float)i/(BAR_COUNT-1);
        float sub = (r<0.5f)? r/0.5f : (r-0.5f)/0.5f;
        Color from = (r<0.5f)? start : mid;
        Color to   = (r<0.5f)? mid   : end;
        barColors[i] = (Color){
            (unsigned char)(from.r + sub*(to.r-from.r)),
            (unsigned char)(from.g + sub*(to.g-from.g)),
            (unsigned char)(from.b + sub*(to.b-from.b)),
            255
        };
    }
}

int main(int argc, char *argv[]) {
    const char *dirPath = (argc == 1)
                        ? GetApplicationDirectory()
                        : argv[1];
    float volume      = 100.0f;
    float current_pos = 0.0f;
    float music_len;
    bool  is_dragging = false;
    int   scrollIndex = 0, focus = -1, prev_active = -1;
    srand((unsigned)time(NULL));
    LoadMusicFiles(dirPath);
    if (musicFileCount == 0) return 1;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Music Player");
    InitAudioDevice();
    SetTargetFPS(60);
    fftPlan = fftwf_plan_r2r_1d(
        BUFFER_SIZE, audioData, audioData,
        FFTW_R2HC, FFTW_ESTIMATE
    );
    GuiLoadStyle("dark.rgs");
    PlayMusicByIndex(0);
    music_len    = GetMusicTimeLength(music);
    selectedFile = current_play;
    prev_active  = current_play;
    while (!WindowShouldClose()) {
        UpdateMusicStream(music);
        float currentTime = GetMusicTimePlayed(music);
        float totalTime   = GetMusicTimeLength(music);
        int curMin = (int)currentTime / 60;
        int curSec = (int)currentTime % 60;
        int totMin = (int)totalTime / 60;
        int totSec = (int)totalTime % 60;
        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr),
                 "%02d:%02d / %02d:%02d",
                 curMin, curSec, totMin, totSec);
        if (curMin == totMin && curSec == totSec && isPlaying && !isPaused) {
            if (shuffle_play) {
                int shuffle_index = rand() % musicFileCount;
                if (musicFileCount > 1 && shuffle_index == current_play)
                    shuffle_index = (shuffle_index + 1) % musicFileCount;

                current_play = shuffle_index;
            } else {
                current_play++;
                if (current_play >= musicFileCount) current_play = 0;
            }
            PlayMusicByIndex(current_play);
            selectedFile = current_play;
        }

        if (GetTime() - lastColorChangeTime >= COLOR_CHANGE_INTERVAL) {
            ChangeBarColors();
            lastColorChangeTime = GetTime();
        }
        int w = GetScreenWidth(), h = GetScreenHeight();
        if (IsKeyPressed(KEY_SPACE)) {
            if (isPaused) ResumeMusicStream(music), isPaused = false;
            else          PauseMusicStream(music),  isPaused = true;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            float t = GetMusicTimePlayed(music);
            if (t + SEEK_TIME >= GetMusicTimeLength(music)) {
                int next = current_play+=1;
                PlayMusicByIndex(next);
                selectedFile = current_play;
                scrollIndex  = current_play;
                music_len    = GetMusicTimeLength(music);
            } else {
                SeekMusicStream(music, t + SEEK_TIME);
            }
            current_pos = GetMusicTimePlayed(music);
        }
        if (IsKeyPressed(KEY_LEFT)) {
            SeekMusicStream(music,
                           GetMusicTimePlayed(music) - SEEK_TIME);
            current_pos = GetMusicTimePlayed(music);
        }
        const char *fileList[musicFileCount];
        for (int i = 0; i < musicFileCount; i++)
            fileList[i] = strrchr(musicFiles[i], '/') + 1;
        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        GuiListViewEx(
            (Rectangle){0, 0, MUSIC_LIST_WIDTH, h},
            fileList, musicFileCount,
            &scrollIndex, &selectedFile, &focus
        );
        if (selectedFile >= 0 && selectedFile != prev_active) {
            PlayMusicByIndex(selectedFile);
            current_play = selectedFile;
            prev_active = selectedFile;
            music_len   = GetMusicTimeLength(music);
        }
        {
            float vw = w - MUSIC_LIST_WIDTH;
            float vh = h - CONTROL_PANEL_HEIGHT;
            float d  = vw / BAR_COUNT;
            float maxMag = 0;
            for (int i = 0; i < BAR_COUNT; i++)
                maxMag = fmax(maxMag, fftData[i]);
            for (int i = 0; i < BAR_COUNT; i++) {
                int index;
                if (i < BAR_COUNT / 2) {
                    index = i;
                } else {
                    index = BAR_COUNT - 1 - i;
                }
                float mag = fftData[index] * FFT_MAGNITUDE_SCALING;
                if (index > BAR_COUNT / 3) mag = log(mag + 1) * 1.2f;
                float avg = 0; int cnt = 0;
                for (int j = -SMOOTHING_WINDOW; j <= SMOOTHING_WINDOW; j++) {
                    if (index + j >= 0 && index + j < BAR_COUNT) {
                        avg += fftData[index + j];
                        cnt++;
                    }
                }
                mag = avg / cnt;
                barHeights[i] += SMOOTHING_FACTOR * (mag - barHeights[i]);
                barHeights[i] = fmin(barHeights[i], vh);
                barHeights[i] = fmax(barHeights[i], MIN_BAR_HEIGHT);
                float x = MUSIC_LIST_WIDTH + 10 + d * i + BAR_SPACING * i;
                float y = (vh - barHeights[i]) / 2;
                DrawRectangle(x, y, BAR_WIDTH, (int)barHeights[i], barColors[i]);
            }
        }
        GuiPanel((Rectangle){
            MUSIC_LIST_WIDTH, h - CONTROL_PANEL_HEIGHT,
            w - MUSIC_LIST_WIDTH, CONTROL_PANEL_HEIGHT
        }, "Controls");
        if (GuiButton((Rectangle){
                MUSIC_LIST_WIDTH+110,
                h-CONTROL_PANEL_HEIGHT+50,
                BUTTON_WIDTH, BUTTON_HEIGHT},
            isPaused?"Resume":"Pause")) {
            if (isPaused) ResumeMusicStream(music), isPaused=false;
            else          PauseMusicStream(music),  isPaused=true;
        }
        SetMusicVolume(music, volume/100.0f);
        GuiSliderBar((Rectangle){
            MUSIC_LIST_WIDTH+60,
            h-CONTROL_PANEL_HEIGHT+100,
            BUTTON_WIDTH,10},
            "Volume", NULL, &volume, 0, 100
        );
        DrawText(timeStr,
                 MUSIC_LIST_WIDTH + 60 + BUTTON_WIDTH + 20,
                 h - CONTROL_PANEL_HEIGHT + 95,
                 18,
                 RAYWHITE);
        if (GuiButton((Rectangle){
                MUSIC_LIST_WIDTH,
                h-CONTROL_PANEL_HEIGHT+50,
                BUTTON_WIDTH, BUTTON_HEIGHT},
            "Seek -10s")) {
            SeekMusicStream(music,
                           GetMusicTimePlayed(music) - SEEK_TIME);
            current_pos = GetMusicTimePlayed(music);
        }
        if (GuiButton((Rectangle){
                MUSIC_LIST_WIDTH+230,
                h-CONTROL_PANEL_HEIGHT+50,
                BUTTON_WIDTH, BUTTON_HEIGHT},
            "Seek +10s")) {
            float t = GetMusicTimePlayed(music);
            if (t + SEEK_TIME >= GetMusicTimeLength(music)) {
                int next = current_play+=1;
                PlayMusicByIndex(next);
                selectedFile = current_play;
                prev_active  = current_play;
                scrollIndex  = current_play;
                music_len    = GetMusicTimeLength(music);
            } else {
                SeekMusicStream(music, t + SEEK_TIME);
            }
            current_pos = GetMusicTimePlayed(music);
        }
        if (GuiButton((Rectangle){
                MUSIC_LIST_WIDTH+350,
                h-CONTROL_PANEL_HEIGHT+50,
                BUTTON_WIDTH, BUTTON_HEIGHT},
            shuffle_play ? "Shuffle On" : "Shuffle Off")) {

            shuffle_play = !shuffle_play;
            stuffle_play_on_or_off = shuffle_play;
        }
        {
            Rectangle seekBar = {
                MUSIC_LIST_WIDTH + 10,
                h - CONTROL_PANEL_HEIGHT + PADDING,
                w - MUSIC_LIST_WIDTH - PADDING - 10,
                10
            };
            if (CheckCollisionPointRec(GetMousePosition(), seekBar) &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                is_dragging = true;
            }
            if (is_dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                is_dragging = false;
                if (current_pos >= music_len - 0.01f) {
                    int next = shuffle_play
                             ? rand() % musicFileCount
                             : current_play + 1;
                    PlayMusicByIndex(next);
                    selectedFile = current_play;
                    prev_active  = current_play;
                    scrollIndex  = current_play;
                    music_len    = GetMusicTimeLength(music);
                } else {
                    SeekMusicStream(music, current_pos);
                }
                current_pos = GetMusicTimePlayed(music);
            }
            if (!is_dragging) {
                current_pos = GetMusicTimePlayed(music);
            }
            GuiSliderBar(seekBar, NULL, NULL,
                         &current_pos, 0, music_len);
        }
        EndDrawing();
    }
    UnloadMusicStream(music);
    fftwf_destroy_plan(fftPlan);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
