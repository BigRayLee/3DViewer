#include "File.h"

// void SaveScreenshotToFile(std::string filename, int windowWidth, int windowHeight)
// {
//     const int numberOfPixels = windowWidth * windowHeight * 3;
//     unsigned char pixels[numberOfPixels];

//     glPixelStorei(GL_PACK_ALIGNMENT, 1);
//     glReadBuffer(GL_FRONT);
//     glReadPixels(0, 0, windowWidth, windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

//     FILE *outputFile = fopen(filename.c_str(), "w");
//     short header[] = {0, 2, 0, 0, 0, 0, (short)windowWidth, (short)windowHeight, 24};

//     fwrite(&header, sizeof(header), 1, outputFile);
//     fwrite(pixels, numberOfPixels, 1, outputFile);
//     fclose(outputFile);

//     printf("Finish writing to file.\n");
// }