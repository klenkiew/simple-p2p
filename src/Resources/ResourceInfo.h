//
// Created by kamil on 14/05/17.
//

#ifndef SIMPLE_P2P_RESOURCEINFO_H
#define SIMPLE_P2P_RESOURCEINFO_H


class ResourceInfo
{
    enum class State {Active, Blocked, Invalid};

    State state;

    // TODO a list of nodes(IP addresses) with this resource? threads which send this resources to other nodes?
};


#endif //SIMPLE_P2P_RESOURCEINFO_H