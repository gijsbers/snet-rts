/*
 * huffman.h
 *
 *  Created on: Feb 11, 2013
 *      Author: vicentsanzmarco
 */

#ifndef HUFFMAN_H_
#define HUFFMAN_H_


#include "pc.h"
#include <string.h>

static unsigned int vlc_remaining;
static unsigned char vlc_amount_remaining;
static unsigned char dcvalue[4];   // 3 is enough


#define vlc_output_byte(c, row, col, nameMatrix)             put_char(c,row, col, nameMatrix)


static unsigned char convertDCMagnitudeCLengthTable[16] = {
    0x02, 0x02, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x00, 0x00, 0x00, 0x00
};

static unsigned short convertDCMagnitudeCOutTable[16] = {
    0x0000, 0x0001, 0x0002, 0x0006, 0x000e, 0x001e, 0x003e, 0x007e,
    0x00fe, 0x01fe, 0x03fe, 0x07fe, 0x0000, 0x0000, 0x0000, 0x0000
};


static unsigned char convertACMagnitudeCLengthTable[256] = {
    0x02, 0x02, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07, 0x09, 0x0a, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,    // 00 - 0f
    0x00, 0x04, 0x06, 0x08, 0x09, 0x0b, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 10 - 1f
    0x00, 0x05, 0x08, 0x0a, 0x0c, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 20 - 2f
    0x00, 0x05, 0x08, 0x0a, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 30 - 3f
    0x00, 0x06, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 40 - 4f
    0x00, 0x06, 0x0a, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 50 - 5f
    0x00, 0x07, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 60 - 6f
    0x00, 0x07, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 70 - 7f
    0x00, 0x08, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 80 - 8f
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 90 - 9f
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // a0 - af
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // b0 - bf
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // c0 - cf
    0x00, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // d0 - df
    0x00, 0x0e, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // e0 - ef
    0x0a, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned short convertACMagnitudeCOutTable[256] = {
    0x0000, 0x0001, 0x0004, 0x000a, 0x0018, 0x0019, 0x0038, 0x0078, 0x01f4, 0x03f6, 0x0ff4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 00 - 0f
    0x0000, 0x000b, 0x0039, 0x00f6, 0x01f5, 0x07f6, 0x0ff5, 0xff88, 0xff89, 0xff8a, 0xff8b, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 10 - 1f
    0x0000, 0x001a, 0x00f7, 0x03f7, 0x0ff6, 0x7fc2, 0xff8c, 0xff8d, 0xff8e, 0xff8f, 0xff90, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 20 - 2f
    0x0000, 0x001b, 0x00f8, 0x03f8, 0x0ff7, 0xff91, 0xff92, 0xff93, 0xff94, 0xff95, 0xff96, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 30 - 3f
    0x0000, 0x003a, 0x01f6, 0xff97, 0xff98, 0xff99, 0xff9a, 0xff9b, 0xff9c, 0xff9d, 0xff9e, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 40 - 4f
    0x0000, 0x003b, 0x03f9, 0xff9f, 0xffa0, 0xffa1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0xFFA6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 50 - 5f
    0x0000, 0x0079, 0x07f7, 0xffa7, 0xffa8, 0xffa9, 0xffaa, 0xffab, 0xFFAc, 0xFFAf, 0xFFAe, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 60 - 6f
    0x0000, 0x007a, 0x07f8, 0xffaf, 0xffb0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5, 0xFFB6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 70 - 7f
    0x0000, 0x00f9, 0xffb7, 0xFFB8, 0xFFB9, 0xFFBa, 0xFFBb, 0xFFBc, 0xFFBd, 0xFFBe, 0xFFBf, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 80 - 8f
    0x0000, 0x01f7, 0xffc0, 0xffc1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC6, 0xFFC7, 0xFFC8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 90 - 9f
    0x0000, 0x01f8, 0xffc9, 0xFFCa, 0xFFCb, 0xFFCc, 0xFFCd, 0xFFCe, 0xFFCf, 0xFFd0, 0xFFd1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // a0 - af
    0x0000, 0x01f9, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6, 0xFFD7, 0xFFD8, 0xFFD9, 0xFFDa, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // b0 - bf
    0x0000, 0x01fa, 0xFFDb, 0xFFDc, 0xFFDd, 0xFFDe, 0xFFDf, 0xFFe0, 0xFFe1, 0xFFe2, 0xFFe3, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // c0 - cf
    0x0000, 0x07f9, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 0xFFEa, 0xFFEb, 0xFFEc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // d0 - df
    0x0000, 0x3fe0, 0xffed, 0xFFEe, 0xFFEf, 0xFFf0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // e0 - ef
    0x03fa, 0x7fc3, 0xFFF6, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};



static unsigned char convertDCMagnitudeYLengthTable[16] = {
    0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x00, 0x00, 0x00, 0x00
};

static unsigned short convertDCMagnitudeYOutTable[16] = {
    0x0000, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x000e, 0x001e,
    0x003e, 0x007e, 0x00fe, 0x01fe, 0x0000, 0x0000, 0x0000, 0x0000
};



static unsigned char convertACMagnitudeYLength[256] = {
    0x04, 0x02, 0x02, 0x03, 0x04, 0x05, 0x07, 0x08, 0x0a, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 00 - 0f
    0x00, 0x04, 0x05, 0x07, 0x09, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 10 - 1f
    0x00, 0x05, 0x08, 0x0a, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 20 - 2f
    0x00, 0x06, 0x09, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 30 - 3f
    0x00, 0x06, 0x0a, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 40 - 4f
    0x00, 0x07, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 50 - 5f
    0x00, 0x07, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 60 - 6f
    0x00, 0x08, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 70 - 7f
    0x00, 0x09, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 80 - 8f
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // 90 - 9f
    0x00, 0x09, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // a0 - af
    0x00, 0x0a, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // b0 - bf
    0x00, 0x0a, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // c0 - cf
    0x00, 0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // d0 - df
    0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,    // e0 - ef
    0x0b, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned short convertACMagnitudeYOut[256] = {
    0xFFFA, 0xFFF0, 0xFFF1, 0xFFF4, 0xFFFB, 0xFFFA, 0xFFF8, 0xFFF8, 0xFFF6, 0xFF82, 0xFF83, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 00 - 0f
    0x0000, 0xFFFC, 0xFFFB, 0xFFF9, 0xFFF6, 0xFFF6, 0xFF84, 0xFF85, 0xFF86, 0xFF87, 0xFF88, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 10 - 1f
    0x0000, 0xFFFC, 0xFFF9, 0xFFF7, 0xFFF4, 0xFF89, 0xFF8A, 0xFF8B, 0xFF8C, 0xFF8D, 0xFF8E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 20 - 2f
    0x0000, 0xFFFA, 0xFFF7, 0xFFF5, 0xFF8F, 0xFF90, 0xFF91, 0xFF92, 0xFF93, 0xFF94, 0xFF95, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 30 - 3f
    0x0000, 0xFFFB, 0xFFF8, 0xFF96, 0xFF97, 0xFF98, 0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 40 - 4f
    0x0000, 0xFFFA, 0xFFF7, 0xFF9E, 0xFF9F, 0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 50 - 5f
    0x0000, 0xFFFB, 0xFFF6, 0xFFA6, 0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 0xFFAD, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 60 - 6f
    0x0000, 0xFFFA, 0xFFF7, 0xFFAE, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 70 - 7f
    0x0000, 0xFFF8, 0xFFC0, 0xFFB6, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC, 0xFFBD, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 80 - 8f
    0x0000, 0xFFF9, 0xFFBE, 0xFFBF, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC6, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // 90 - 9f
    0x0000, 0xFFFA, 0xFFC7, 0xFFC8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE, 0xFFCF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // a0 - af
    0x0000, 0xFFF9, 0xFFD0, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6, 0xFFD7, 0xFFD8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // b0 - bf
    0x0000, 0xFFFA, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF, 0xFFE0, 0xFFE1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // c0 - cf
    0x0000, 0xFFF8, 0xFFE2, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // d0 - df
    0x0000, 0xFFEB, 0xFFEC, 0xFFED, 0xFFEE, 0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,    // e0 - ef
    0xFFF9, 0xFFF5, 0xFFF6, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};


int vlc_init_start();
int vlc_stop_done(int row, int col, char nameMatrix);

void ConvertDCMagnitudeC(unsigned char magnitude,unsigned short int *out, unsigned short int *lenght);
void ConvertACMagnitudeC(unsigned char magnitude,unsigned short int *out, unsigned short int *lenght);
void ConvertDCMagnitudeY(unsigned char magnitude,unsigned short int *out, unsigned short int *lenght);
void ConvertACMagnitudeY(unsigned char magnitude,unsigned short int *out, unsigned short int *lenght);
char Extend (char additional, unsigned char magnitude);
void ReverseExtend (char value, unsigned char *magnitude, unsigned char *bits);


void WriteRawBits16(unsigned char amount_bits, unsigned int bits, int row, int col, char nameMatrix);
void HuffmanEncodeFinishSend(int row, int col, char nameMatrix);
void HuffmanEncodeUsingDCTable(unsigned char magnitude, int row, int col, char nameMatrix);
void HuffmanEncodeUsingACTable(unsigned char mag, int row, int col, char nameMatrix);

char EncodeDataUnit(char dataunit[64], unsigned int color, int row, int col, char nameMatrix, int sample);

#endif /* HUFFMAN_H_ */
