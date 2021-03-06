//
// Created by erxyi on 21.05.2017.
//

#ifndef SIMPLE_P2P_FILEMANAGER_H
#define SIMPLE_P2P_FILEMANAGER_H

#include <vector>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/list.hpp>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "FileRecord.h"
#include "AuthorKeyHasher.h"
#include "FileManagerTypes.h"
#include "Requests/FilePartRequest.h"
#include "Requests/FilePartResponse.h"
#include "Requests/FileCreateRequest.h"
#include "Requests/FileSavePartRequest.h"
#include "Requests/AddFileRequest.h"
#include "../Crypto/AuthorKey.h"
#include "../Crypto/Hash.h"
#include "Requests/SetFileStateRequest.h"

class FileManager {

    friend class boost::serialization::access;
public:
    FileManager(const AuthorLookupMap &alm);
    FileManager();

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & BOOST_SERIALIZATION_NVP(authorLookupMap);
    }

    void setWorkingDirectory(const std::string & path);

    AuthorsList        getAllAuthors();
    AuthorFilesHashList    getAllFilesFromAuthor(const AuthorKeyType & author_key);

    FilePartResponse                             getFilePart(const FilePartRequest &request);
    bool                                         saveFilePart(const FileSavePartRequest &request);
    bool                                         createFile(const FileCreateRequest & request);
    std::pair<Hash, std::vector<unsigned char> > addFile(const AddFileRequest &request);
    void                                         setFileState(const SetFileStateRequest &request);
private:
    AuthorLookupMap     authorLookupMap;
    std::string         cwd;
    std::shared_timed_mutex almMutex;

    ResourcesFindResult findInResourcesManager(const Hash &hash);
    boost::filesystem::path createFilePath(const FileCreateRequest &request, AuthorKey authorKey);
    FileRecord findFileFromTable(const GenericFileRequest &request);
};

extern FileManager fileManagerInstance;

#endif //SIMPLE_P2P_FILEMANAGER_H
