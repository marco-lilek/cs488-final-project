-- Simple Scene:
-- An extremely simple scene that will render right out of the box
-- with the provided skeleton code.  It doesn't rely on hierarchical
-- transformations.

-- Create the top level root node named 'root'.
rootNode = gr.node('root')
rootNode:translate(0,0,2)

-- Create a GeometryNode with MeshId = 'cube', and name = 'torso'.
-- MeshId's must reference a .obj file that has been previously loaded into
-- the MeshConsolidator instance within A3::init().
cubeMesh = gr.mesh('sphere', '~player')
cubeMesh:scale(1.0, 1.0, 1.0)
cubeMesh:rotate('y', 45.0)
cubeMesh:set_material(gr.material({1, 1, 1}, {1, 1, 1}, 10.0))
--cubeMesh:set_texture('brick.jpg', 'brick-normal.jpg');
-- Add the cubeMesh GeometryNode to the child list of rootnode.
rootNode:add_child(cubeMesh)

-- Create a GeometryNode with MeshId = 'sphere', and name = 'head'.
sphereMesh = gr.mesh('sphere', 'name-of-sphere')
sphereMesh:scale(0.5, 0.5, 0.5)
sphereMesh:set_material(gr.material({1, 1, 1}, {1, 1, 1}, 10.0))
-- Add the sphereMesh GeometryNode to the child list of rootnode.
rootNode:add_child(sphereMesh)

floorMesh = gr.mesh('cube', 'cubie')
floorMesh:scale(5,1,5);
floorMesh:rotate('x', -15.0)
floorMesh:translate(0,-3,0);
floorMesh:set_material(gr.material({1, 1, 1}, {0.1, 0.1, 0.1}, 20.0))
floorMesh:set_texture('', '');
rootNode:add_child(floorMesh)

windowMesh = gr.mesh('cube', 'cubie2')
windowMesh:scale(3,1,3);
windowMesh:rotate('x', -15.0)
windowMesh:translate(-3,0.5,0);
windowMesh:set_material(gr.material({1, 1, 1}, {1, 1, 1}, 10.0, 0.5))
windowMesh:set_texture('~perlin', '');
rootNode:add_child(windowMesh);


rw = gr.mesh('cube', 'right wall')
rw:scale(1,5,5);
rw:translate(4+1, 0,0)
rw:set_material(gr.material({1, 1, 1}, {1, 1, 1}, 10.0, 1))
rootNode:add_child(rw)

-- Return the root with all of it's childern.  The SceneNode A3::m_rootNode will be set
-- equal to the return value from this Lua script.
return rootNode
