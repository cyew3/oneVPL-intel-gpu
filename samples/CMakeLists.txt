cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project( mediasdk-samples )

include( ../builder/FindFunctions.cmake )
include( ../builder/FindGlobals.cmake )
include( ../builder/FindMFX.cmake )
include( ../builder/FindPackages.cmake )

create_build( )
