#pragma once

#include <random>
#include <memory>

class PerlinNoise {
public:
  PerlinNoise();
  std::shared_ptr<char> getNoise();

  static const size_t NOISE_DIM = 512;
private:
  void generateNoise();
  double noise[NOISE_DIM * NOISE_DIM * 3];

  double smoothNoise(double x, double y);
  double turbulence(double x, double y, double size);
};

