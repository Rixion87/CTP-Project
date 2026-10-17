#include <mesh_platform.h>
#include <mesh.h>
#include <packet.h>
#include <radioHAL.h>
#include <routing.h>
#include <SeenCache.h>
#include <MKRWAN.h>

/*Code for transmitter*/

Radio radio; MeshNode mesh(&radio, NODE_ID);
