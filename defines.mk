include lib/cnext/defines.mk

DEFINES += \
    -DWsResponseObject=RedBlackTree \
    -DWsRequestObject=RedBlackTree \
    -DWsResponseNode=RedBlackNode \
    -DWsRequestNode=RedBlackNode \
    -DwsResponseObjectDestroy=rbTreeDestroy \
    -DwsResponseObjectToJson=rbTreeToJson \

