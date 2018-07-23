#include "PerlinNoise.hpp"

#include <glm/glm.hpp>

using namespace std;
PerlinNoise::PerlinNoise() {
  generateNoise();
}

double PerlinNoise::smoothNoise(double x, double y) {
  double fracX = x - (int)(x);
  double fracY = y - (int)(y);

  int x1 = ((int)(x) + NOISE_DIM) % NOISE_DIM;
  int y1 = ((int)(y) + NOISE_DIM) % NOISE_DIM;
  
  int x2 = (x1 + NOISE_DIM - 1) % NOISE_DIM;
  int y2 = (y1 + NOISE_DIM - 1) % NOISE_DIM;

  double value = 0.0;
  value += fracX* fracY * noise[x1 * NOISE_DIM + y1];
  value += (1 - fracX) * fracY * noise[x2 * NOISE_DIM + y1];
  value += fracX * (1 - fracY) * noise[x1 * NOISE_DIM + y2];
  value += (1 - fracX) * (1 - fracY) * noise[x2 *NOISE_DIM+y2];

  return value;
}

double PerlinNoise::turbulence(double x, double y, double size) {
  double value = 0.0, initialSize = size;
  while (size >=1) {
    value += smoothNoise(x/size, y/size) * size;
    size /= 2.0;
  }

  return 128.0 * value / initialSize;
}

void PerlinNoise::generateNoise() {
  for (int y = 0; y < NOISE_DIM; y++) {
    for (int x = 0; x < NOISE_DIM; x++) {
      noise[x * NOISE_DIM + y] = (rand() % 32768) / 32768.0;
    }
  }
}

char* PerlinNoise::getNoise() {
  double xPeriod = 5.0;
  double yPeriod = 10.0;
  double turbPower = 5.0;
  double turbSize = 32.0;

  char *toRet = new char[NOISE_DIM * NOISE_DIM * 3];
  for (int y = 0; y < NOISE_DIM; y++) {
    for (int x = 0; x < NOISE_DIM; x++) {
      double xyValue = x * xPeriod / NOISE_DIM + 
          y * yPeriod / NOISE_DIM +
          turbPower * turbulence(x,y,turbSize) / 256.0;
      double sinVal = 256 * glm::abs(glm::sin(xyValue * 3.1415926));
      char cval = sinVal;
      toRet[3* (x * NOISE_DIM + y)] = cval;
      toRet[3* (x * NOISE_DIM + y)+1] = cval;
      toRet[3* (x * NOISE_DIM + y)+2] = cval;
    }
  }
  return toRet;
}
