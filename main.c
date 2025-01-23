#include <stdbool.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "raylib.h"
#include <dirent.h>
#include "fftw3.h"
#include <string.h>
#include <math.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BAR_COUNT 90
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define CONTROL_PANEL_HEIGHT 150
#define MUSIC_LIST_WIDTH 400
#define MUSIC_LIST_HEIGHT 400
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
#define minvalue 0 //colour mix value
#define maxvalue 255 //colour max value
#define PADDING 30

float audioData[BUFFER_SIZE];
float fftData[BAR_COUNT];
fftwf_plan fftPlan;
float barHeights[BAR_COUNT];
Music music;
bool stuffle_play=0;
bool cliked_stufflebutton=0;
bool isPlaying = false;
bool isPaused = false;
char musicFiles[2048][512];
int musicFileCount = 0;
int selectedFile = -1;
int value = 0; //for update a colour value in bars
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
int shuffle_music() {
    if (stuffle_play&&musicFileCount > 0) {
        srand(time(NULL));
        int num = rand() % musicFileCount;
        return num;
    }
    return -1;
}
void colour_scrolling_adjust(){
    value=(int)GuiSliderBar((Rectangle){ 50, 100, 300, 20 }, NULL, NULL,0, minvalue, maxvalue);
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText(TextFormat("Value: %d", value), 50, 50, 20, WHITE);
}

void ChangeBarColors() {
    Color startColor = (Color){0, 255, 0, 255}; // Blue
    Color middleColor = (Color){255, 0, 0, 255}; // Red
    Color endColor = (Color){0, 0, 255, 255};   // Green

    for (int i = 0; i < BAR_COUNT; i++) {
        float ratio = (float)i / (BAR_COUNT - 1); // Normalize index to [0, 1]

        if (ratio < 0.5f) {
            float subRatio = ratio / 0.5f; // Normalize ratio to [0, 1] for first half
            barColors[i].r = (unsigned char)(startColor.r + subRatio * (middleColor.r - startColor.r));
            barColors[i].g = (unsigned char)(startColor.g + subRatio * (middleColor.g - startColor.g));
            barColors[i].b = (unsigned char)(startColor.b + subRatio * (middleColor.b - startColor.b));
        } else {
            float subRatio = (ratio - 0.5f) / 0.5f; // Normalize ratio to [0, 1] for second half
            barColors[i].r = (unsigned char)(middleColor.r + subRatio * (endColor.r - middleColor.r));
            barColors[i].g = (unsigned char)(middleColor.g + subRatio * (endColor.g - middleColor.g));
            barColors[i].b = (unsigned char)(middleColor.b + subRatio * (endColor.b - middleColor.b));
        }

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
    float current_pos = 0.0f;
    int window_width;
    bool is_dragging = false;
    int window_height;
    ChangeBarColors();
    GuiLoadStyle("dark.rgs");

    while (!WindowShouldClose()) {
        UpdateMusicStream(music);
        window_width = GetScreenWidth();
        window_height = GetScreenHeight();

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
        if (GuiButton((Rectangle){MUSIC_LIST_WIDTH + 110,
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
        if (GuiButton((Rectangle){MUSIC_LIST_WIDTH + 230,
                                window_height - CONTROL_PANEL_HEIGHT + 50,
                                BUTTON_WIDTH, BUTTON_HEIGHT},
                      "Seek +10s")) {
            SeekMusicStream(music, GetMusicTimePlayed(music) + SEEK_TIME);
        }

        // Seek -10s button
        if (GuiButton((Rectangle){MUSIC_LIST_WIDTH,
                                window_height - CONTROL_PANEL_HEIGHT + 50,
                                BUTTON_WIDTH, BUTTON_HEIGHT},
                      "Seek -10s")) {
            SeekMusicStream(music, GetMusicTimePlayed(music) - SEEK_TIME);
        }

        Rectangle des = {
                MUSIC_LIST_WIDTH + 60,
                window_height - CONTROL_PANEL_HEIGHT + 100,
                BUTTON_WIDTH, 10
        };
        //volume slider
        GuiSliderBar(des, "Volume", NULL, &volume, 0, 100);

        Rectangle des_seek = {
                MUSIC_LIST_WIDTH + 10,
                window_height - CONTROL_PANEL_HEIGHT + PADDING,
                window_width - MUSIC_LIST_WIDTH - PADDING + 25, 10
        };

        if (CheckCollisionPointRec(GetMousePosition(), des_seek) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            is_dragging = true;
        }

        if (!is_dragging) {
            current_pos = GetMusicTimePlayed(music);
        }

        if (is_dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            is_dragging = false;
            SeekMusicStream(music, current_pos);
        }

        GuiSliderBar(des_seek, NULL, NULL, &current_pos, 0, music_len);
        // if (GuiButton((Rectangle){MUSIC_LIST_WIDTH + 110,
        //                           window_height - CONTROL_PANEL_HEIGHT + 100,
        //                           BUTTON_WIDTH, BUTTON_HEIGHT},
        //               stuffle_play ? "Shuffle On" : "Shuffle Off")) {
        //     stuffle_play = !stuffle_play; // Toggle shuffle state
        //     if (stuffle_play && musicFileCount > 0) {
        //         active = shuffle_music(); // Get a random index
        //         if (active != -1 && active != prev_active) { // Avoid replaying the same track
        //             UnloadMusicStream(music); // Unload current track
        //             music = LoadMusicStream(musicFiles[active]); // Load new track
        //             PlayMusicStream(music); // Start playback
        //             prev_active = active; // Update the previous active track
        //         }
        //     }
        // }
        EndDrawing();
    }

    UnloadMusicStream(music);
    fftwf_destroy_plan(fftPlan);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
