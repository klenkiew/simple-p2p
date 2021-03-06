//
// Created by erxyi on 02.06.17.
//

#ifndef SIMPLE_P2P_BLOCKCOMMAND_H
#define SIMPLE_P2P_BLOCKCOMMAND_H

#include <cstdint>
#include <boost/serialization/access.hpp>
#include "Command.h"
#include "../Visitor.h"
#include "OneIntegerParamCommand.h"

class BlockCommand : public OneIntegerParamCommand {
public:
    BlockCommand(uint64_t localId) : OneIntegerParamCommand(localId) {}
    BlockCommand() {}
    void accept(Visitor* visitor) {visitor->handle(this);};
    Command::Type getType() { return Command::Type::Block; }
};


#endif //SIMPLE_P2P_BLOCKCOMMAND_H
