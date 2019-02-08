rootNode = gr.node('root')
rootNode:rotate('x', 10)
rootNode:translate(0,0,12);

cannonShift = gr.node('cannonShift')
rootNode:add_child(cannonShift);
cannonShift:translate(0,-6,-0.5)

cannon = gr.node('~cannon')
cannonShift:add_child(cannon)

cannonShaft = gr.mesh('cylinder', 'cannonShaft')
cannonShaft:scale(0.1, 1,0.1);
cannonShaft:translate(0,1,0)
cannon:add_child(cannonShaft)

bubblesHolder = gr.node('~bubblesHolder')
bubblesHolder:translate(0,0,-0.5)
rootNode:add_child(bubblesHolder)

-- TODO create new mesh with same uv maps on each face
marbleBG = gr.mesh('cube-sameface', 'marbleBG')
marbleBG:scale(11.1,13,2)
marbleBG:translate(0,0,1)
marbleBG:set_material(gr.material({0.6, 0.6, 0.6}, {0.3, 0.3, 0.4}, 20))
marbleBG:set_texture('~perlin', '')
rootNode:add_child(marbleBG)

wall = gr.node('cylinder')

wallColumn = gr.mesh('cylinder', 'wallColumn')
wall:add_child(wallColumn)
wallColumn:scale(0.25,6,0.25)
wallColumn:set_material(gr.material({0.0, 0.0, 1.0}, {0.4, 0.0, 0.3}, 5, 0.5))

wallBase = gr.mesh('cube-sameface', 'wallBase')
wall:add_child(wallBase)
wallBase:scale(0.5,0.5,0.5)
wallBase:translate(0,-6,0)
wallBase:set_material(gr.material({1,1,1}, {0.1, 0.1, 0.1}, 10))
wallBase:set_texture('','');

wallTop = gr.mesh('cube-sameface', 'wallTop')
wall:add_child(wallTop)
wallTop:scale(0.5,0.5,0.5)
wallTop:translate(0,6,0)
wallTop:set_material(gr.material({1,1,1}, {0.1, 0.1, 0.1}, 10))
wallTop:set_texture('','');

walldist = 4.8

leftWall = gr.node('leftWall')
rootNode:add_child(leftWall)
leftWall:translate(walldist,0,-1)

leftWall:add_child(wall)

rightWall = gr.node('rightWall')
rootNode:add_child(rightWall)
rightWall:translate(-walldist,0,-1)
rightWall:add_child(wall)

wallwidth = 4.25
topWall = gr.mesh('cube', '~topWall')
rootNode:add_child(topWall)
topWall:rotate('z',90);
topWall:scale(wallwidth, 0.1,0.1);
topWall:translate(0,0.1,-0.75);
topWall:set_material(gr.material({0.3, 0.3, 0.3}, {1, 1,1}, 10,1))
topWall:set_texture('');

bottom = gr.mesh('cube', 'bottom')
rootNode:add_child(bottom)
bottom:rotate('z',90);
bottom:scale(wallwidth, 0.1,0.1);
bottom:translate(0,-4.25,-0.25);
bottom:set_material(gr.material({1.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, 10,1))
bottom:set_texture('');

hidden = gr.node("~hidden")
rootNode:add_child(hidden)

br = 0.5 -- bubble rad

-- bubble types available
bubble1 = gr.mesh('sphere-square-uv', '~bubble1')
hidden:add_child(bubble1)
bubble1:scale(br,br,br)
bubble1:rotate('x',90)
bubble1:set_material(gr.material({1,1,1}, {0.2, 0.2, 0.2}, 10))
bubble1:set_texture('earth-2.jpg', 'earth-normal-2.jpg');

bubble2 = gr.mesh('sphere-square-uv', '~bubble2')
hidden:add_child(bubble2)
bubble2:scale(br,br,br)
bubble2:rotate('x',90)
bubble2:set_material(gr.material({1,1,1}, {0.1, 0.1, 0.1}, 10, 1))
bubble2:set_texture('', 'golf-ball-3.jpg');

bubble3 = gr.node('~bubble3')
hidden:add_child(bubble3)
bubble3Ext = gr.mesh('sphere-square-uv', 'bubble3Ext')
bubble3:add_child(bubble3Ext)
bubble3Ext:scale(br,br,br)
bubble3Ext:rotate('x',90)
bubble3Ext:set_material(gr.material({0.8,0.8,1}, {0,0,1}, 5, 0.5))
bubble3Ext:set_texture('', '');

bubble3Int = gr.mesh('cube-sameface', 'bubble3Int')
bubble3:add_child(bubble3Int)
bubble3Int:scale(br,br,br)
bubble3Int:set_material(gr.material({1,1,1}, {0.7, 0.0, 0.0}, 10, 1))
bubble3Int:set_texture('octa-star-img.jpg', 'crystal-normal.jpg');

bubble4 = gr.node('~bubble4')
hidden:add_child(bubble4)

bubble4ext = gr.mesh('sphere-square-uv', 'bubble4ext')
bubble4:add_child(bubble4ext)
bubble4ext:scale(br,br,br)
bubble4ext:rotate('x',90)
bubble4ext:set_material(gr.material({0.6,0.6,0}, {1,0.0,0}, 5, 0.4))
bubble4ext:set_texture('', '');

bubble4Inside = gr.mesh('cube', 'bubble4Inside')
bubble4:add_child(bubble4Inside)
bubble4Inside:scale(br/2,br/2,br/2)
bubble4Inside:set_material(gr.material({1,1,0}, {0.4, 0.3, 0}, 20, 1))
bubble4Inside:set_texture('bubble-bobble.jpg', '');

bubble5 = gr.mesh('sphere-square-uv', '~bubble5')
hidden:add_child(bubble5)
bubble5:scale(br,br,br)
bubble5:rotate('x',90)
bubble5:set_material(gr.material({0.4,0.2,0.8}, {0.1, 0.5, 0.5}, 10, 1))
bubble5:set_texture('', 'crystal-normal-sphere.jpg');


--bubble6 = gr.node('~bubble6')
--hidden:add_child(bubble6)

--bubble6ext = gr.mesh('sphere-square-uv', 'bubble6ext')
--bubble6:add_child(bubble6ext)
--bubble6ext:scale(br,br,br)
--bubble6ext:rotate('x',90)
--bubble6ext:set_material(gr.material({0.8,0,0}, {0.8,0,0}, 5, 0.4))
--bubble6ext:set_texture('', '');

--bubble6Inside = gr.mesh('sphere-square-uv', 'bubble6Inside')
--bubble6:add_child(bubble6Inside)
--bubble6Inside:scale(br/2,br/2,br/2)
--bubble6Inside:set_material(gr.material({0,1,0}, {0, 0.3, 0}, 20, 0.4))
--bubble6Inside:set_texture('', '');

return rootNode
