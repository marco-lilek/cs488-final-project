#pragma once

#include "Material.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <list>
#include <string>
#include <iostream>

class GeometryNode;

enum class NodeType {
	SceneNode,
	GeometryNode,
	JointNode
};

class SceneNode {
public:
    SceneNode(const std::string & name);

	SceneNode(const SceneNode & other);

    virtual ~SceneNode();
    
	int totalSceneNodes() const;
    
    virtual const glm::mat4 get_transform() const;
    virtual void scale(const glm::vec3 & amount);
    const glm::mat4& get_inverse() const;
    
    void set_transform(const glm::mat4& m);
    
    void add_child(SceneNode* child);
    
    void remove_child(SceneNode* child);

	//-- Transformations:
    void rotate(char axis, float angle);
    void translate(const glm::vec3& amount);


	friend std::ostream & operator << (std::ostream & os, const SceneNode & node);

	bool isSelected;
    
    // Transformations
    glm::mat4 trans;
    glm::mat4 invtrans;
    
    std::list<SceneNode*> children;

	NodeType m_nodeType;
	std::string m_name;
	unsigned int m_nodeId;

void getNodes(std::vector<SceneNode *> &nodes);

  void move();
  void setMoveVector(const glm::vec3 &mv);
  void setPos(glm::vec3 pos);
  glm::vec3 position;
  glm::vec3 moveVector;
  float collisionRadius;

private:
	// The number of SceneNode instances.
	static unsigned int nodeInstanceCount;
};
