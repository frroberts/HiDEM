# Ice sheet with mesh collision

**NOTE!!!** the code behind this sample is still being developed and is still subject to change, and might not be entirely polished. 

This sample creates an ice sheet, with a cone mesh inside it, the sheet is then pushed sideways into the cone.
The particles will collide with the cone and be pushed away.

The collision is all handled in the `applyForce` function, there is a new section in there that handles the force coming from the mesh.

The section looks as follows:
```c++
        // NOTE!!! this is here more as a demo og how interactions can be done, it it not nessecarily physcially acurate !!!!
        mesh->closestTriFun(xCord,yCord, zCord, bedInteract, pInd, [&] (int tri, lpReal_t dist){
            dist = sqrt(dist);

            lpReal_t xForceCol = mesh->dataPtr->xNorm[tri] * (bedStiffness * dist);
            lpReal_t yForceCol = mesh->dataPtr->yNorm[tri] * (bedStiffness * dist);
            lpReal_t zForceCol = mesh->dataPtr->zNorm[tri] * (bedStiffness * dist);

            mesh->addForce(tri, xForceCol, yForceCol, zForceCol);

            xForce += xForceCol;
            yForce += yForceCol;
            zForce += zForceCol;
        });


```
On the `mesh` object we calla function `closestTriFun` that takes asa arguments, the x,y,z coordinates, the interaction distance and the particle id. 
Finally it takes a lambda function that we call for any particle close enough to the mesh. 
In this lambda, if we want to deposit force on the mesh we call `addForce` on the mesh object, this force does not affect the simulation in any way but can be retrieved when writing out the mesh data.

The force calculation is free to be changed.