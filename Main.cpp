#include "Project.hpp"

#include <iostream>
using namespace std;

int main( int argc, char **argv ) 
{
		std::string luaSceneFile("Assets/gameScene.lua");
		std::string title("Assignment 3 - [");
		title += luaSceneFile;
		title += "]";

		CS488Window::launch(argc, argv, new Project(luaSceneFile), 1024, 768, title);

	return 0;
}
