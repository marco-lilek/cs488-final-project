#include "SceneNode.hpp"

#include "cs488-framework/MathUtils.hpp"

#include "SceneNode.hpp"
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

using namespace glm;

// Static class variable
unsigned int SceneNode::nodeInstanceCount = 0;


//---------------------------------------------------------------------------------------
SceneNode::SceneNode(const std::string& name)
  : m_name(name),
	m_nodeType(NodeType::SceneNode),
	trans(mat4()),
	isSelected(false),
	m_nodeId(nodeInstanceCount++),position(0), moveVector(0), collisionRadius(1)
{

}

//---------------------------------------------------------------------------------------
// Deep copy
SceneNode::SceneNode(const SceneNode & other)
	: m_nodeType(other.m_nodeType),
	  m_name(other.m_name),
	  trans(other.trans),
	  invtrans(other.invtrans)
{
	for(SceneNode * child : other.children) {
		this->children.push_front(new SceneNode(*child));
	}
}

#include <set>

// lol
set<SceneNode*> deleted;
//---------------------------------------------------------------------------------------
SceneNode::~SceneNode() {
	for(SceneNode * child : children) {
		if (deleted.count(child) == 0) {
      deleted.insert(child);
      delete child;
    }
    
	}
}

//---------------------------------------------------------------------------------------
void SceneNode::set_transform(const glm::mat4& m) {
	trans = m;
	invtrans = m;
}


const glm::mat4 SceneNode::get_transform() const {
  return glm::translate(mat4(), position) * trans;
}


//---------------------------------------------------------------------------------------
const glm::mat4& SceneNode::get_inverse() const {
	return invtrans;
}

//---------------------------------------------------------------------------------------
void SceneNode::add_child(SceneNode* child) {
	children.push_back(child);
}

//---------------------------------------------------------------------------------------
void SceneNode::remove_child(SceneNode* child) {
	children.remove(child);
}

//---------------------------------------------------------------------------------------
void SceneNode::rotate(char axis, float angle) {
	vec3 rot_axis;

	switch (axis) {
		case 'x':
			rot_axis = vec3(1,0,0);
			break;
		case 'y':
			rot_axis = vec3(0,1,0);
	        break;
		case 'z':
			rot_axis = vec3(0,0,1);
	        break;
		default:
			break;
	}
	mat4 rot_matrix = glm::rotate(degreesToRadians(angle), rot_axis);
	trans = rot_matrix * trans;
}


//---------------------------------------------------------------------------------------
void SceneNode::translate(const glm::vec3& amount) {
	trans = glm::translate(amount) * trans;
}


//---------------------------------------------------------------------------------------
int SceneNode::totalSceneNodes() const {
	return nodeInstanceCount;
}

void SceneNode::getNodes(std::vector<SceneNode *> &nodes) {
  nodes.push_back(this);
  
  for (auto child: children) {
    child->getNodes(nodes);
  }
}

//---------------------------------------------------------------------------------------
std::ostream & operator << (std::ostream & os, const SceneNode & node) {

	//os << "SceneNode:[NodeType: ___, name: ____, id: ____, isSelected: ____, transform: ____"
	switch (node.m_nodeType) {
		case NodeType::SceneNode:
			os << "SceneNode";
			break;
		case NodeType::GeometryNode:
			os << "SceneNode";
			break;
		case NodeType::JointNode:
			os << "JointNode";
			break;
	}
	os << ":[";

	os << "name:" << node.m_name << ", ";
	os << "id:" << node.m_nodeId;
	os << "]";

	return os;
}

void SceneNode::setMoveVector(const glm::vec3 &mv) {
  moveVector = mv;
}

void SceneNode::move() {
  position += moveVector;
}

void SceneNode::setPos(glm::vec3 pos) {
  position = pos;
}

void SceneNode::scale(const glm::vec3 & amount) { // NOTE assumes we do scales first
	trans = glm::scale(amount) * trans;
  if (amount.x == amount.y && amount.x == amount.z)
    collisionRadius *= amount.x;
  else
    collisionRadius = 0;
}
