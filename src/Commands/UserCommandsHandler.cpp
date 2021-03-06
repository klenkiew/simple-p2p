//
// Created by kamil on 25/05/17.
//

#include "UserCommandsHandler.h"
#include "../Messages/BroadcastMessage.h"
#include "CommandTypes/AddCommand.h"
#include "CommandTypes/DownloadCommand.h"
#include "CommandTypes/BlockCommand.h"
#include "NetworkCommandInterface.h"
#include "ResourceDownloadHandler.h"
#include "../Files/FileManager.h"
#include "../Messages/ResourceManagementMessage.h"
#include "CommandTypes/UnblockCommand.h"
#include "CommandTypes/InvalidateCommand.h"
#include "CommandTypes/DeleteCommand.h"
#include "CommandTypes/CancelCommand.h"
#include "../Crypto/Hash.h"
#include "../ConfigHandler.h"

namespace
{
    template<typename InfoType>
    void printResourceMap(const ResourceManager::ResourceMap<InfoType> &resources, std::ostream& stream)
    {
        for (const auto &keyRes : resources)
        {
            Hash authorKeyHash(keyRes.first, Hash::InputTextType::Text);

            stream << "Key: ";
            stream << authorKeyHash.getString() << std::endl << std::endl;
            for (const auto &res : keyRes.second)
            {
                stream <<  "Id: " << res.second.getLocalId() << std::endl;
                stream  << res.first.toString(); // << std::endl;
                stream  << res.second.toString() << std::endl;
            }
        }
    }
}

UserCommandsHandler::UserCommandsHandler(int port, int clientPort) : broadcastPort(port), socket(Socket::Domain::Ip4, Socket::Type::Udp),
                                                     commandInterface(std::make_unique<NetworkCommandInterface>(clientPort)),
                                                                     downloader(port)
{
    socket.enableBroadcast();
}

UserCommandsHandler::~UserCommandsHandler()
{
    socket.close();
}

void UserCommandsHandler::handleUserInput()
{
    while (true)
    {
        std::unique_ptr<Command> command = commandInterface->getNextCommand();
        try {
            command->accept(this);
        }
        catch(std::runtime_error e)
        {
            stringstream ss;
            ss << "Last command failed, reason: " << e.what();
            std::cout << ss.str();
            commandInterface->sendResponse(ss.str());
        }
        catch(std::exception e)
        {
            stringstream ss;
            ss << "Last command failed, reason: " << e.what();
            std::cout << ss.str();
            commandInterface->sendResponse(ss.str());
        }

    }
}

std::pair<AuthorKeyType, Resource> UserCommandsHandler::resourceFromFile(std::string filePath)
{
    const std::string publicKeyFileName = ConfigHandler::getInstance()->get("keys.dir")+"rsa_public.pem";
    const std::string privateKeyFileName = ConfigHandler::getInstance()->get("keys.dir")+"rsa_private.pem";
    AuthorKey key(publicKeyFileName, privateKeyFileName);
    AddFileRequest request(key.getPublicKey(), key.getPrivateKey(), filePath);
    auto hashAndSign = fileManagerInstance.addFile(request);
    size_t size = file_size(filePath);
    std::string fileName = path(filePath).filename().string();
    return std::make_pair(key.getPublicKey(), Resource(fileName, size, hashAndSign.first.getVector(), hashAndSign.second));
}

std::unordered_map<std::string, std::vector<Resource>> UserCommandsHandler::convertInfoMapToResourceMap(ResourceManager::ResourceMap<LocalResourceInfo> map)
{
    std::unordered_map<std::string, std::vector<Resource>> allResources;
    for (const auto& keyResource : map)
    {
        std::vector<Resource> resources;
        for (const auto& resourceWithInfo : keyResource.second)
        {
            if (resourceWithInfo.second.getResourceState() == Resource::State::Active)
                resources.push_back(resourceWithInfo.first);
        }
        if (!resources.empty())
            allResources.insert(std::make_pair(keyResource.first, std::move(resources)));
    }
    return allResources;
}

void UserCommandsHandler::handle(BroadcastCommand *command)
{
    std::stringstream stream = broadcastOnDemand();
    commandInterface->sendResponse(stream.str());
}

std::stringstream UserCommandsHandler::broadcastOnDemand() {
    const auto ownedResources = resourceManager.getOwnedResources();
    const auto localResources = resourceManager.getLocalResources();
    std::stringstream stream;
    log << "--- Sending broadcast." << std::endl;
    stream << "--- Sending broadcast." << std::endl;

    auto ownedResourcesMap = convertInfoMapToResourceMap(ownedResources);
    broadcastResourceMap(ownedResourcesMap);
    auto localResourcesMap = convertInfoMapToResourceMap(localResources);
    broadcastResourceMap(localResourcesMap);

    stream << "Broadcast sent." << std::endl;
    return stream;
}

void UserCommandsHandler::handle(DisplayCommand *command)
{
    log << "Displaying network resources." << std::endl;
    std::stringstream stream;
    stream << "Network resources: " << std::endl;
    const auto resources = resourceManager.getNetworkResources();
    printResourceMap<NetworkResourceInfo>(resources, stream);
    stream << "Downloaded resources: " << std::endl;
    const auto localResources = resourceManager.getLocalResources();
    printResourceMap<LocalResourceInfo>(localResources, stream);
    stream << "Owned resources: " << std::endl;
    const auto ownedResources = resourceManager.getOwnedResources();
    printResourceMap<LocalResourceInfo>(ownedResources, stream);
    commandInterface->sendResponse(stream.str());
}

void UserCommandsHandler::handle(AddCommand *command)
{
    std::pair<AuthorKeyType, Resource> resource = resourceFromFile(command->getFileName());
    resourceManager.addOwnedResource(resource.first, resource.second);
    std::stringstream stream;
    stream << "File " << command->getFileName() << " added to local resources.";
    log << stream.str() << std::endl;
    commandInterface->sendResponse(stream.str());
}

void UserCommandsHandler::handle(UnknownCommand *command)
{
    log << "Unknown command." << std::endl;
    commandInterface->sendResponse("Daemon unable to recognize the command");
}

void UserCommandsHandler::handle(DownloadCommand *command)
{
    const auto resource = resourceManager.getResourceById(command->getLocalId());
    const auto info = resourceManager.getNetworkResourceInfo(resource.first, resource.second);
    if (info.getResourceState() != Resource::State::Active)
    {
        log << "Cannot download file: " << resource.second.getName() << " because of invalid state"<< std::endl;
        commandInterface->sendResponse("Cannot download file: " +  resource.second.getName() + " because of invalid state");
        return;
    }
    log << "Downloading file: " << resource.second.getName() << std::endl;

    commandInterface->sendResponse("Downloading in progress.");
    downloader.downloadResource(std::move(resource));
    std::unordered_map<std::string, std::vector<Resource>> resourceToBroadcast;
    resourceToBroadcast[resource.first] = std::vector<Resource>{resource.second};
    broadcastResourceMap(resourceToBroadcast);
    std::cout << "Broadcasting downlaoded resource" << std::endl;
}


void UserCommandsHandler::handle(BlockCommand *command) {
    uint64_t id = command->getLocalId();
    MessageType messageType = MessageType::BlockResource;
    Resource::State state = Resource::State::Blocked;

    sendBroadcastMessage(id, messageType, state);

}

void
UserCommandsHandler::sendBroadcastMessage(uint64_t id, const MessageType &messageType, const Resource::State &state) {
    auto result = resourceManager.getResourceById(id);
    resourceManager.setOwnedResourceInfoState(result.first, result.second, state);

    stringstream stream;
    stream << "File " << result.second.getName() << " is ";
    if(messageType==MessageType::BlockResource)
        stream << "blocked";
    if(messageType==MessageType::UnblockResource)
        stream << "unblocked";
    if(messageType==MessageType::DeleteResource)
        stream << "deleted";
    if(messageType==MessageType::InvalidateResource)
        stream << "invalidated";

    stream << std::endl;
    sendBroadcastMessage(result, messageType);
    log << stream.str() << endl;
    commandInterface->sendResponse(stream.str());
}

void UserCommandsHandler::sendBroadcastMessage(const std::pair<std::string, Resource> &result, const MessageType &messageType) {
    ConfigHandler *config = ConfigHandler::getInstance();
    AuthorKey authorKey(config->get("keys.dir") + "rsa_public.pem", config->get("keys.dir") + "rsa_private.pem");

    vector<unsigned char> sign;
    ResourceManagementMessage message(result.first, result.second, sign);
    vector<unsigned char> binaryMessage {static_cast<unsigned char>(messageType)};
    auto messageData = message.toByteStream(false);
    binaryMessage.insert(binaryMessage.end(), messageData.begin(), messageData.end());

    sign = authorKey.signMessage(binaryMessage);
    message.setSign(sign);
    binaryMessage = {static_cast<unsigned char>(messageType)};
    messageData = message.toByteStream(true);
    binaryMessage.insert(binaryMessage.end(), messageData.begin(), messageData.end());
    socket.writeTo(&binaryMessage[0], binaryMessage.size(), ConfigHandler::getInstance()->get("network.broadcast_ip"),
                   broadcastPort);
}

void UserCommandsHandler::handle(UnblockCommand *command) {
    uint64_t id = command->getLocalId();
    MessageType messageType = MessageType::UnblockResource;
    Resource::State state = Resource::State::Active;

    sendBroadcastMessage(id, messageType, state);
}

void UserCommandsHandler::handle(DeleteCommand *command) {
    uint64_t id = command->getLocalId();
    ConfigHandler *config = ConfigHandler::getInstance();
    auto result = resourceManager.getResourceById(id);

//    AuthorKey authorKey(config->get("keys.dir") + "rsa_public.pem", config->get("keys.dir") + "rsa_private.pem");

    MessageType messageType = MessageType::DeleteResource;
    Resource::State state = Resource::State::Invalid;

    sendBroadcastMessage(id, messageType, state);
    std::cout << "handleDelete on author node in action" << std::endl;
    resourceManager.deleteOwnedResource(result.first, result.second);
    SetFileStateRequest sfss(result.first, Hash(result.second.getHash()), FileRecordState::Deleted);
    fileManagerInstance.setFileState(sfss);
}

void UserCommandsHandler::handle(InvalidateCommand *command) {
    uint64_t id = command->getLocalId();
    MessageType messageType = MessageType::InvalidateResource;
    Resource::State state = Resource::State::Invalid;

    sendBroadcastMessage(id, messageType, state);
}

void UserCommandsHandler::handle(CancelCommand *command) {
    uint64_t id = command->getLocalId();
    MessageType messageType = MessageType::InvalidateResource;
    Resource::State state = Resource::State::Invalid;

    sendBroadcastMessage(id, messageType, state);
}

void UserCommandsHandler::broadcastResourceMap(std::unordered_map<std::string, std::vector<Resource>> &map)
{
    if (map.empty())
    {
        std::cout << "No resources to broadcast" << std::endl;
        return;
    }
    BroadcastMessage message(map);
    const auto bytes = message.toByteStream();
    socket.writeTo(&bytes[0], bytes.size(), ConfigHandler::getInstance()->get("network.broadcast_ip"), broadcastPort);
}





















