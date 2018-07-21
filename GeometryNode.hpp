#pragma once

#include "SceneNode.hpp"

class GeometryNode : public SceneNode {
public:
	GeometryNode(
		const std::string & meshId,
		const std::string & name
	);

	Material material;

	// Mesh Identifier. This must correspond to an object name of
	// a loaded .obj file.
	std::string meshId;
  std::string texture;
  std::string bumpMap;

  void move();
  void setMoveVector(const glm::vec3 &mv);
  void setPos(glm::vec3 pos);

  virtual const glm::mat4 get_transform() const;
  virtual void scale(const glm::vec3 & amount);

  glm::vec3 position;
  glm::vec3 moveVector;
  float collisionRadius;
};
