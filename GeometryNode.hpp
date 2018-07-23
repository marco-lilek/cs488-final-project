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

};


struct BubbleNode : public SceneNode {
	BubbleNode(
		const std::string & name
	) : SceneNode(name) {}
  size_t type;
};
