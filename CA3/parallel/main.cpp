#include <fstream>
#include <iostream>
#include <chrono>
#include <pthread.h>

typedef int LONG;
typedef unsigned short WORD;
typedef unsigned int DWORD;

using namespace std;

#define FOR(i, x, y) for (int i = x; i < y; i++)
#define MAX_ROW 10000
#define MAX_COL 10000
#define OUTPUT_ADDR "output.bmp"
#define THREAD_CNT 8

#pragma pack(push, 1)
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;
#pragma pack(pop)

int rows;
int cols;
char* fileBuffer;
int bufferSize;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} pixel;

pixel Image[MAX_ROW][MAX_COL];
float kernelFilter[9] = {1, -1, 2, 2, 1, -1, 3, -2, 1};
float blurFilter[9] = {0.0625, 0.125, 0.0625, 0.125, 0.25, 0.125, 0.0625, 0.125, 0.0625};

bool fillAndAllocate(char*& buffer, const char* fileName, int& rows, int& cols, int& bufferSize) {
    std::ifstream file(fileName);
    if (!file) {
        std::cout << "File" << fileName << " doesn't exist!" << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[length];
    file.read(&buffer[0], length);

    PBITMAPFILEHEADER file_header;
    PBITMAPINFOHEADER info_header;

    file_header = (PBITMAPFILEHEADER)(&buffer[0]);
    info_header = (PBITMAPINFOHEADER)(&buffer[0] + sizeof(BITMAPFILEHEADER));
    rows = info_header->biHeight;
    cols = info_header->biWidth;
    bufferSize = file_header->bfSize;
    return true;
}

void* getPixelsFromBMP24(void* t) {
    long tid = (long)t;
    int firstRow = float(float(rows)/THREAD_CNT) * tid;
    int lastRow = float(float(rows)/THREAD_CNT) * (tid + 1);
    if (tid == THREAD_CNT - 1) {
        lastRow = max(lastRow, rows);
    }
    int extra = cols % 4;
    int count = (tid * (rows/THREAD_CNT)) * (cols * 3 + extra) + 1;
    for (int i = firstRow; i < lastRow; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                switch (k) {
                case 0:
                    Image[i][j].red = fileBuffer[bufferSize - count];
                    break;
                case 1:
                    Image[i][j].green = fileBuffer[bufferSize - count];
                    break;
                case 2:
                    Image[i][j].blue = fileBuffer[bufferSize - count];
                    break;
                }
                count++;
            }
        }
    }
    pthread_exit(NULL);
}

void* writeOutBmp24(void* t) {
    std::ofstream write(OUTPUT_ADDR);
    if (!write) {
        std::cout << "Failed to write " << OUTPUT_ADDR << std::endl;
        exit(-1);
    }

    long tid = (long)t;
    int firstRow = float(float(rows)/THREAD_CNT) * tid;
    int lastRow = float(float(rows)/THREAD_CNT) * (tid + 1);
    if (tid == THREAD_CNT - 1) {
        lastRow = max(lastRow, rows);
    }
    int extra = cols % 4;
    int count = (tid * (rows/THREAD_CNT)) * (cols * 3 + extra) + 1;
    for (int i = firstRow; i < lastRow; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                switch (k) {
                case 0:
                    fileBuffer[bufferSize - count] = Image[i][j].red;
                    break;
                case 1:
                    fileBuffer[bufferSize - count] = Image[i][j].green;
                    break;
                case 2:
                    fileBuffer[bufferSize - count] = Image[i][j].blue;
                    break;
                }
                count++;
            }
        }
    }
    write.write(fileBuffer, bufferSize);
    pthread_exit(NULL);
}

void* mirror(void* t) {
    long tid = (long)t; 
    int firstCol = float(float(cols)/THREAD_CNT) * tid;
    int lastCol = float(float(cols)/THREAD_CNT) * (tid + 1);
    if (tid == THREAD_CNT - 1) {
        lastCol = max(lastCol, cols);
    }
    FOR(j, firstCol, lastCol) {
        FOR(i, 0, rows/2) {
            pixel temp = Image[i][j];
            Image[i][j] = Image[rows - i - 1][j];
            Image[rows - i - 1][j] = temp;
        }
    }
    pthread_exit(NULL);
}

pixel mult(pixel p, float x) {
    pixel res;
    res.red = p.red * x;
    res.green = p.green * x;
    res.blue = p.blue * x;
    return res;
}

bool isOut(int i, int j) {
    return (i < 0 || i >= rows || j < 0 || j >= cols);
}

pixel result[MAX_ROW][MAX_COL];

void* kernel(void* t) {
    long tid = (long)t;
    int firstRow = float(float(rows)/THREAD_CNT) * tid;
    int lastRow = float(float(rows)/THREAD_CNT) * (tid + 1);
    if (tid == THREAD_CNT - 1) {
        lastRow = max(lastRow, rows);
    }
    int di[9] = {-1, -1, -1, 0, 0, 0, 1, 1, 1};
    int dj[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
    FOR(i, firstRow, lastRow) {
        FOR(j, 0, cols) {
            pixel sum;
            sum.red = 0; sum.green = 0; sum.blue = 0;
            FOR(k, 0, 9) {
                if (isOut(i + di[k], j + dj[k])) {
                    pixel res = mult(Image[i][i], blurFilter[k]);
                    sum.red += res.red; sum.green += res.green; sum.blue += res.blue;
                } else {
                    pixel res = mult(Image[i + di[k]][j + dj[k]], blurFilter[k]);
                    sum.red += res.red; sum.green += res.green; sum.blue += res.blue;
                }
            }
            result[i][j].red = sum.red < 0 ? 0 : sum.red > 255 ? 255 : sum.red;
            result[i][j].green = sum.green < 0 ? 0 : sum.green > 255 ? 255 : sum.green;
            result[i][j].blue = sum.blue < 0 ? 0 : sum.blue > 255 ? 255 : sum.blue;
        }
    }
    FOR(i, firstRow, lastRow) {
        FOR(j, 0, cols) {
            Image[i][j] = result[i][j];
        }
    }
    pthread_exit(NULL);
}

void* purpleHaze(void* t) {
    long tid = (long)t;
    int firstRow = float(float(rows)/THREAD_CNT) * tid;
    int lastRow = float(float(rows)/THREAD_CNT) * (tid + 1);
    if (tid == THREAD_CNT - 1) {
        lastRow = max(lastRow, rows);
    }
    FOR(i, firstRow, lastRow) {
        FOR(j, 0, cols) {
            pixel curr = Image[i][j];
            pixel temp;
            temp.green = max(0.0, min(255.0, 0.16 * curr.red + 0.5 * curr.green + 0.16 * curr.blue));
            temp.blue = max(0.0, min(255.0, 0.6 * curr.red + 0.2 * curr.green + 0.8 * curr.blue));
            temp.red = max(0.0, min(255.0, 0.5 * curr.red + 0.3 * curr.green + 0.5 * curr.blue));
            Image[i][j] = temp;
        }
    }
    pthread_exit(NULL);
}

void hashur() {
    FOR(i, 0, rows) {
        int j = floor(cols * (rows-i)/rows);
        Image[i][j].red = 255; Image[i][j].green = 255; Image[i][j].blue = 255;
        j = floor(cols/2 * (rows/2-i)/(rows/2));
        Image[i][j].red = 255; Image[i][j].green = 255; Image[i][j].blue = 255;
        j = floor(3*cols/2 * (3*rows/2-i)/(3*rows/2));
        Image[i][j].red = 255; Image[i][j].green = 255; Image[i][j].blue = 255;
    }
}

void makeThreads(void* (*func)(void*)) {
    pthread_t threads[THREAD_CNT];
    for (long t = 0; t < THREAD_CNT; t++) {
        if (pthread_create(&threads[t], NULL, func, (void*)t) != 0) {
            cout << "Error creating a thread." << endl;
            exit(-1);
        }
    }
    FOR(t, 0, THREAD_CNT) {
        if (pthread_join(threads[t], NULL) != 0) {
            cout << "Error joining threads." << endl;
            exit(-1);
        }
    }
}

void applyFilter(void* (*func)(void*), string funcName) {
    auto start = chrono::high_resolution_clock::now();
    makeThreads(func);
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::duration<double>> (end - start);
    cout << funcName << ": " << fixed << setprecision(3) << duration.count() * 1000.0 << " ms" << endl;
}

void applyChanges() {
    applyFilter(getPixelsFromBMP24, "Read");
    applyFilter(mirror, "Flip");
    applyFilter(kernel, "Blur");
    applyFilter(purpleHaze, "Purple");
    auto start = chrono::high_resolution_clock::now();
    hashur();
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::duration<double>> (end - start);
    cout << "Lines: " << fixed << setprecision(3) << duration.count() * 1000.0 << " ms" << endl;
    applyFilter(writeOutBmp24, "Write");
}

int main(int argc, char* argv[]) {
    if (!fillAndAllocate(fileBuffer, argv[1], rows, cols, bufferSize)) {
        std::cout << "File read error" << std::endl;
        return 1;
    }
    auto start_time = chrono::high_resolution_clock::now();
    applyChanges();
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::duration<double>> (end_time - start_time);
    cout << "Execution: " << fixed << setprecision(3) << duration.count() * 1000.0 << " ms" << std::endl;
    
    return 0;
}