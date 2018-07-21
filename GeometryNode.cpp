#include "GeometryNode.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

//---------------------------------------------------------------------------------------
GeometryNode::GeometryNode(
		const std::string & meshId,
		const std::string & name
)
	: SceneNode(name),
	  meshId(meshId), position(0), moveVector(0), collisionRadius(1)
{
	m_nodeType = NodeType::GeometryNode;
}

void GeometryNode::setMoveVector(const glm::vec3 &mv) {
  moveVector = mv;
}

void GeometryNode::move() {
  position += moveVector;
}

void GeometryNode::setPos(glm::vec3 pos) {
  position = pos;
}

const glm::mat4 GeometryNode::get_transform() const {
  return glm::translate(mat4(), position) * trans;
}


void GeometryNode::scale(const glm::vec3 & amount) { // NOTE assumes we do scales first
  SceneNode::scale(amount);
  if (amount.x == amount.y && amount.x == amount.z)
    collisionRadius *= amount.x;
  else
    collisionRadius = 0;
}
