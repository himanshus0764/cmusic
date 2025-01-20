#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raylib.h"
#include <dirent.h>
#include <fftw3.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BAR_COUNT 128
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define CONTROL_PANEL_HEIGHT 150
#define MUSIC_LIST_WIDTH 400
#define MUSIC_LIST_HEIGHT 400
#define BAR_WIDTH 5
#define BAR_SPACING 2
#define FFT_MAGNITUDE_SCALING 0.4f
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 40
#define SEEK_TIME 10.0f
#define SMOOTHING_WINDOW 0
#define MAX_BAR_HEIGHT 300
#define MIN_BAR_HEIGHT 1.0f
#define SMOOTHING_FACTOR 0.1f
#define BACKGROUND_COLOR (Color){0,0,0,255}

float audioData[BUFFER_SIZE];
float fftData[BAR_COUNT];
fftwf_plan fftPlan;
float barHeights[BAR_COUNT];
Music music;
bool isPlaying = false;
bool isPaused = false;
char musicFiles[2048][512];
int musicFileCount = 0;
int selectedFile = -1;

// Color array for changing FFT bar colors
Color barColors[BAR_COUNT];

// Timer to track the 5-second interval
float colorChangeTimer = 0.0f;
const float COLOR_CHANGE_INTERVAL = 5.0f; // 5 seconds

void AudioProcessor(void *audioStream, unsigned int frameCount) {
    float *audio = (float *)audioStream;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        audioData[i] = audio[i];
    }

    fftwf_execute(fftPlan);
    for (int i = 0; i < BAR_COUNT; i++) {
        fftData[i] = audioData[i] * audioData[i];
    }
}

void LoadMusicFiles(const char *path) {
    struct dirent *entry;
    DIR *dp = opendir(path);

    if (dp == NULL) {
        printf("Could not open directory\n");
        return;
    }

    while ((entry = readdir(dp))) {
        if (entry->d_type == DT_REG) {
            if (strstr(entry->d_name, ".mp3") ||
                strstr(entry->d_name, ".wav") ||
                strstr(entry->d_name, ".ogg")) {

                // Create the full file path
                snprintf(musicFiles[musicFileCount],
                         sizeof(musicFiles[musicFileCount]), "%s/%s", path,
                         entry->d_name);

                // Print the full path for debugging purposes
                printf("Found music file: %s\n", musicFiles[musicFileCount]);

                musicFileCount++;
            }
        }
    }
    closedir(dp);
}

void ChangeBarColors() {
    Color startColor = (Color){255, 0, 0, 255}; // Red
    Color endColor = (Color){0, 0, 255, 255};   // Blue

    for (int i = 0; i < BAR_COUNT; i++) {
        float ratio = (float)i / (BAR_COUNT - 1); // Normalize index to [0, 1]
        barColors[i].r = (unsigned char)(startColor.r + ratio * (endColor.r - startColor.r));
        barColors[i].g = (unsigned char)(startColor.g + ratio * (endColor.g - startColor.g));
            barColors[i].b = (unsigned char)(startColor.b + ratio * (endColor.b - startColor.b));
            barColors[i].a = 255; // Full opacity
        }
    }

    int main(int argc, char *argv[]) {
        const char *directoryPath = (argc == 1) ? GetApplicationDirectory() : argv[1];

        float volume = 100.0f;
        LoadMusicFiles(directoryPath);

        if (musicFileCount == 0) {
            printf("No audio files found in the directory.\n");
            return 1;
        }

        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                   "Raylib Music Player with Visualizer and Raygui");
        InitAudioDevice();
        SetTargetFPS(60);

        fftPlan = fftwf_plan_r2r_1d(BUFFER_SIZE, audioData, audioData, FFTW_R2HC,
                                    FFTW_ESTIMATE);

        music = LoadMusicStream(musicFiles[0]);
        PlayMusicStream(music);

        AttachAudioStreamProcessor(music.stream, AudioProcessor);

        int scrollIndex = 0;
        int active = 0;
        int focus = -1;
        int prev_active = 0;
        float music_len = GetMusicTimeLength(music);
        int window_width;
        int window_height;

        ChangeBarColors();
        GuiLoadStyle("dark.rgs");

        while (!WindowShouldClose()) {
            UpdateMusicStream(music);
            window_width = GetScreenWidth();
            window_height = GetScreenHeight();

            // Update the timer
            colorChangeTimer += GetFrameTime();

            if (colorChangeTimer >= COLOR_CHANGE_INTERVAL) {
                // Reset the timer and change the colors for all bars
                colorChangeTimer = 0.0f;
                ChangeBarColors(); // Change color for all bars
            }

            if (IsKeyPressed(KEY_SPACE)) {
                if (isPaused) {
                    ResumeMusicStream(music);
                    isPaused = false;
                } else {
                    PauseMusicStream(music);
                    isPaused = true;
                }
            }

            if (IsKeyPressed(KEY_RIGHT)) {
                SeekMusicStream(music, GetMusicTimePlayed(music) + SEEK_TIME);
            }
            if (IsKeyPressed(KEY_LEFT)) {
                SeekMusicStream(music, GetMusicTimePlayed(music) - SEEK_TIME);
            }

            if (IsKeyPressed(KEY_ENTER)) {
                if (isPlaying) {
                    StopMusicStream(music);
                    isPlaying = false;
                } else {
                    PlayMusicStream(music);
                    isPlaying = true;
                }
            }

            const char *fileList[musicFileCount];
            for (int i = 0; i < musicFileCount; i++) {
                fileList[i] = strrchr(musicFiles[i], '/') + 1;
            }

            BeginDrawing();
            GuiListViewEx(
                    (Rectangle){0, 0, MUSIC_LIST_WIDTH, GetScreenHeight()},
                    fileList, musicFileCount, &scrollIndex, &active, &focus);
            {
                if (active == -1) {
                    active = prev_active;
                } else if (active != prev_active) {
                    if (isPlaying) {
                        StopMusicStream(music);
                        isPlaying = false;
                    }

                    UnloadMusicStream(music);
                    music = LoadMusicStream(musicFiles[active]);
                    music_len = GetMusicTimeLength(music);
                    PlayMusicStream(music);
                    isPlaying = true;
                    AttachAudioStreamProcessor(music.stream, AudioProcessor);
                }
            }

            prev_active = active;

            ClearBackground(BACKGROUND_COLOR);

            // Expand the visualizer to fill most of the screen space
            float visualizerWidth = window_width - MUSIC_LIST_WIDTH;
            float d = visualizerWidth / BAR_COUNT;
            float visualizerHeight = window_height - CONTROL_PANEL_HEIGHT;

            // Normalize the magnitude across all bars
            float maxMagnitude = 0.0f;
            for (int i = 0; i < BAR_COUNT; i++) {
                maxMagnitude = fmax(maxMagnitude, fftData[i]);
            }

            for (int i = 0; i < BAR_COUNT; i++) {
                float magnitude = fftData[i] * FFT_MAGNITUDE_SCALING;

                // Apply logarithmic scaling for higher frequencies
                if (i > BAR_COUNT / 3) {
                    magnitude = log(magnitude + 1.0f) * 1.2f;
                }

                // Normalize the magnitude across all bars
                magnitude = magnitude / maxMagnitude * MAX_BAR_HEIGHT;

                // Apply smoothing (Moving average)
                float avg = 0;
                int count = 0;

                    for (int j = -SMOOTHING_WINDOW; j <= SMOOTHING_WINDOW; j++) {
                        if (i + j >= 0 && i + j < BAR_COUNT) {
                            avg += fftData[i + j];
                            count++;
                        }
                    }
                    magnitude = avg / count;

                    // Smooth the height transition of bars
                    barHeights[i] += SMOOTHING_FACTOR * (magnitude - barHeights[i]);

                    // Get the color for the current bar from the barColors array
                    Color barColor = barColors[i];

                    // Limit the maximum height of the bars
                    barHeights[i] = fmin(barHeights[i], visualizerHeight);
                    barHeights[i] = fmax(barHeights[i], MIN_BAR_HEIGHT);

                    // Center the bars vertically
                    float barX = MUSIC_LIST_WIDTH + 10 + d * i + (BAR_SPACING * i);
                    float barY = (visualizerHeight - barHeights[i]) / 2;
                    DrawRectangle(barX, barY, BAR_WIDTH, (int)barHeights[i], barColor);
                }

                // Control Panel and Buttons
                GuiPanel((Rectangle){MUSIC_LIST_WIDTH, window_height - CONTROL_PANEL_HEIGHT,
                                     WINDOW_WIDTH + 340, CONTROL_PANEL_HEIGHT},
                         "Controls");

                // Play/Pause button
                if (GuiButton((Rectangle){MUSIC_LIST_WIDTH,
                                          window_height - CONTROL_PANEL_HEIGHT + 50,
                                          BUTTON_WIDTH, BUTTON_HEIGHT},
                              isPaused ? "Resume" : "Pause")) {
                    if (isPaused) {
                        ResumeMusicStream(music);
                        isPaused = false;
                    } else {
                        PauseMusicStream(music);
                        isPaused = true;
                    }
                }

                SetMusicVolume(music, volume / 100.0f);

                // Seek +10s button
                if (GuiButton((Rectangle){MUSIC_LIST_WIDTH + 110,
                                          window_height - CONTROL_PANEL_HEIGHT + 50,
                                          BUTTON_WIDTH, BUTTON_HEIGHT},
                              "Seek +10s")) {
                    SeekMusicStream(music, GetMusicTimePlayed(music) + SEEK_TIME);
                }

                // Seek -10s button
                if (GuiButton((Rectangle){MUSIC_LIST_WIDTH + 230,
                                          window_height - CONTROL_PANEL_HEIGHT + 50,
                                          BUTTON_WIDTH, BUTTON_HEIGHT},
                              "Seek -10s")) {
                    SeekMusicStream(music, GetMusicTimePlayed(music) - SEEK_TIME);
                }

                Rectangle des = {
                        MUSIC_LIST_WIDTH + 300,
                        window_height - CONTROL_PANEL_HEIGHT + 100,
                        BUTTON_WIDTH, 10
                };
                GuiSliderBar(des, "Volume", NULL, &volume, 0, 100);

                EndDrawing();
            }

            UnloadMusicStream(music);
            fftwf_destroy_plan(fftPlan);
            CloseAudioDevice();
            CloseWindow();

            return 0;
        }
